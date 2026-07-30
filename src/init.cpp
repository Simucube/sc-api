#include "sc-api/session.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

#include "compatibility.h"
#include "control.h"
#include "crypto/gcm.h"
#include "device_info_internal.h"
#include "sc-api/protocol/commands.h"
#include "sc-api/protocol/core.h"
#include "variable_internal.h"

#define SC_API_PROTOCOL_CORE_SHM_FILENAME "$sc-api-core$"

#pragma comment(lib, "Ws2_32.lib")

static const SC_API_PROTOCOL_Core_t *s_shm_core_ptr = NULL;

using namespace sc::api_internal;

static sc::api_internal::SharedMemory s_shm_core;

static ShmBlock s_controller_shm;
static ShmBlock s_device_info_shm;
static ShmBlock s_variable_header_shm;
static ShmBlock s_variable_data_shm;
static std::chrono::steady_clock::time_point s_prev_keep_alive;
static uint32_t                              s_session_id            = 0;
static uint32_t                              s_prev_keep_alive_value = 0;
static constexpr std::chrono::milliseconds k_keep_alive_timeout{200};

static bool first_init_ = true;

static void closeCoreShm(void)
{
    s_controller_shm.close();
    s_device_info_shm.close();
    s_variable_header_shm.close();
    s_variable_data_shm.close();

    s_shm_core.close();
    s_shm_core_ptr = NULL;
}

static bool isCoreShmActiveAndValid(void)
{
    std::atomic_thread_fence(std::memory_order_acquire);
    uint32_t start_revision = s_shm_core_ptr->data_revision_counter;
    if (start_revision & 1) {
        return false;
    }

    bool valid = SC_API_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_CORE_SHM_VERSION,
                                                  s_shm_core_ptr->version)
                 && s_shm_core_ptr->state == SC_API_PROTOCOL_CORE_ACTIVE;

    std::atomic_thread_fence(std::memory_order_acq_rel);
    return valid && s_shm_core_ptr->data_revision_counter == start_revision;
}

void postSharedMemoryOpen() {
    dev_info_state = new DeviceInfoState(s_device_info_shm);
    initVariableDataShm(&s_variable_header_shm, &s_variable_data_shm);
}

static SC_API_Result_t initializeApi(const char* id_name, const char* display_name_utf8, uint32_t flags) {
    if (std::strlen(id_name) > 16 || std::strlen(display_name_utf8) > 64) return SC_API_ERROR_ARGUMENT;

    if (first_init_) {
        SC_API_gcm_initialize();
        first_init_ = false;
    }

    /* Backend should be updating keep_alive_counter in at least 10Hz frequency
     *
     * So 200ms should be plenty to detect it changing.
     */
    static constexpr auto timeout = std::chrono::milliseconds(500);

    if (s_shm_core.isOpen()) return SC_API_ERROR_BUSY;

    if (!s_shm_core.openForReadOnly(SC_API_PROTOCOL_CORE_SHM_FILENAME)) {
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    s_shm_core_ptr = (const SC_API_PROTOCOL_Core_t*)s_shm_core.getBuffer();

    if (!SC_API_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_CORE_SHM_VERSION, s_shm_core_ptr->version)) {
        closeCoreShm();
        return SC_API_ERROR_INCOMPATIBLE;
    }

    if (s_shm_core_ptr->udp_control_protocol_version >> 16 != SC_API_PROTOCOL_UDP_PROTOCOL_VERSION_MAJOR) {
        closeCoreShm();
        return SC_API_ERROR_INCOMPATIBLE;
    }

    // Wait until shared memory data is fully filled to avoid race condition
    SC_API_PROTOCOL_Core_t core_data;
    bool                   data_acquired    = false;
    auto                   wait_valid_start = std::chrono::steady_clock::now();

    // Try to get copy of the core data as atomic, so all info is from the same version
    // and fully initialized
    while (wait_valid_start + timeout > std::chrono::steady_clock::now()) {
        if (isCoreShmActiveAndValid()) {
            uint32_t pre_revision_counter = s_shm_core_ptr->data_revision_counter;
            std::atomic_thread_fence(std::memory_order_acq_rel);
            core_data = *s_shm_core_ptr;
            std::atomic_thread_fence(std::memory_order_acq_rel);
            if (pre_revision_counter == s_shm_core_ptr->data_revision_counter) {
                data_acquired = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!data_acquired) {
        closeCoreShm();
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    if (!initializeUdpSocket(core_data.udp_control_address, core_data.udp_control_port)) {
        closeCoreShm();
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    s_session_id = s_shm_core_ptr->session_id;

    SC_API_PROTOCOL_CommandRegister_t reg_cmd;
    reg_cmd.hdr.command_id    = SC_API_PROTOCOL_CMD_REGISTER;
    reg_cmd.hdr.version_flags = SC_API_PROTOCOL_CMD_REGISTER_VERSION_FLAGS;
    reg_cmd.hdr.controller_id = 0;
    reg_cmd.hdr.flags         = SC_API_PROTOCOL_CMD_FLAG_NONE;

    reg_cmd.flags             = flags;
    strncpy_s(reg_cmd.name_id, id_name, sizeof(reg_cmd.name_id));
    strncpy_s(reg_cmd.display_name, display_name_utf8, sizeof(reg_cmd.display_name));

    if (!sendControlPacketSync(&reg_cmd.hdr, sizeof(reg_cmd))) {
        // We couldn't send packet to the control address so maybe backend crashed or was killed
        // but some other application kept the shared memory alive
        destructUdpSocket();
        closeCoreShm();
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    if (!s_controller_shm.open(core_data.registered_ctrl_shm, false) ||
        !s_device_info_shm.open(core_data.device_info_shm, false) ||
        !s_variable_header_shm.open(core_data.var_header_shm, false) ||
        !s_variable_data_shm.open(core_data.var_data_shm, false)) {
        destructUdpSocket();
        closeCoreShm();
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    while (wait_valid_start + timeout > std::chrono::steady_clock::now()) {
        uint16_t this_reg_id    = 0;
        unsigned this_reg_index = 0;

        // Search for this registration as one atomic access so we can be sure that
        // it is correct
        bool success            = s_controller_shm.tryAtomicDataAccess([&](const void* shm_data, uint32_t shm_size) {
            const auto* shm = reinterpret_cast<const SC_API_PROTOCOL_RegisteredControllerShm_t*>(shm_data);
            const char* registered_ctrl_start = reinterpret_cast<const char*>(shm_data) + shm->registered_ctrl_offset;
            auto        get_ctrl_ptr          = [&](int idx) {
                return reinterpret_cast<const SC_API_PROTOCOL_RegisteredController_t*>(registered_ctrl_start +
                                                                                       idx * shm->registered_ctrl_size);
            };

            for (uint32_t i = 0; i < shm->registered_ctrl_count; ++i) {
                const auto* ctrl_ptr = get_ctrl_ptr(i);
                if (std::strncmp(ctrl_ptr->name_id, id_name, sizeof(ctrl_ptr->name_id)) == 0) {
                    this_reg_id    = ctrl_ptr->num_id;
                    this_reg_index = i;
                    return true;
                }
            }

            return false;
        });

        if (success) {
            controllerRegistered(this_reg_index, this_reg_id);

            postSharedMemoryOpen();

            s_prev_keep_alive_value = s_shm_core_ptr->keep_alive_counter;
            s_prev_keep_alive       = std::chrono::steady_clock::now();
            return SC_API_OK;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    destructUdpSocket();
    closeCoreShm();
    return SC_API_ERROR_TIMEOUT;
}
SC_API_Result_t SC_API_initAndConnectForControl(const char *id_name, const char *display_name_utf8)
{
    return initializeApi(id_name, display_name_utf8, SC_API_PROTOCOL_COMMAND_REGISTER_CONTROL);
}

SC_API_Result_t SC_API_initAndConnectForMonitoring(const char* id_name, const char* display_name_utf8) {
    return initializeApi(id_name, display_name_utf8, 0);
}

SC_API_Result_t SC_API_shutdown()
{
    if (!s_shm_core.isOpen()) {
        // Nothing to shutdown
        return SC_API_ERROR_STATUS;
    }
    destructUdpSocket();
    closeCoreShm();
    return SC_API_OK;
}

SC_API_Result_t SC_API_updateStatus() {
    if (!s_shm_core.isOpen()) {
        return SC_API_ERROR_STATUS;
    }

    if (s_shm_core_ptr->session_id != s_session_id || s_shm_core_ptr->state != SC_API_PROTOCOL_CORE_ACTIVE) {
        // Session id or state has changed so the session must have been closed
        return SC_API_ERROR_NOT_AVAILABLE;
    }

    uint32_t current_keep_alive = s_shm_core_ptr->keep_alive_counter;
    auto     cur_time           = std::chrono::steady_clock::now();
    if (s_prev_keep_alive_value == current_keep_alive) {
        if (s_prev_keep_alive + k_keep_alive_timeout < cur_time) {
            // Too long time has passed from the keep alive variable update
            return SC_API_ERROR_NOT_AVAILABLE;
        }
    } else {
        s_prev_keep_alive_value = current_keep_alive;
        s_prev_keep_alive       = cur_time;
    }

    return SC_API_OK;
}
