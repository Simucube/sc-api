#include "sc-api/core/api_core.h"

#include <cassert>
#include <chrono>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

#include "api_internal.h"
#include "compatibility.h"
#include "crypto/gcm.h"
#include "device_info_internal.h"
#include "sc-api/core/protocol/actions.h"
#include "sc-api/core/protocol/bson_shm_blocks.h"
#include "sc-api/core/protocol/core.h"
#include "sc-api/core/protocol/telemetry.h"
#include "sc-api/core/protocol/variables.h"
#include "sim_data_internal.h"

namespace sc_api::core {

#define SC_API_PROTOCOL_CORE_SHM_FILENAME "$sc-api-core$"

using namespace sc_api::core::internal;

static std::once_flag s_init_only_once_flag;
static void           initOnlyOnce() {
              std::call_once(s_init_only_once_flag, []() { SC_API_gcm_initialize(); });
}

ApiCore::Impl::~Impl() {
    if (active_session_) {
        std::lock_guard lock(active_session_->m_);
        active_session_->api_ = nullptr;
    }
}

ResultCode ApiCore::Impl::openSession() {
    std::lock_guard lock(m_);
    if (session_state_ != Session::State::invalid) return ResultCode::error_invalid_session_state;

    /* Backend should be updating keep_alive_counter in at least 10Hz frequency
     *
     * So 500ms should be plenty to detect it changing.
     */
    static constexpr auto timeout_duration = std::chrono::milliseconds(500);

    if (auto r = openCoreShmHandle(); r != ResultCode::ok) {
        return r;
    }

    ResultCode r                = ResultCode::error_busy;
    auto   wait_valid_start = std::chrono::steady_clock::now();

    internal::AlignedUniquePtr<void> session_data_buf;
    const SC_API_PROTOCOL_Session_t* session_data = nullptr;

    internal::SharedMemory session_shm;
    // Try to get copy of the core data as atomic, so all info is from the same version
    // and fully initialized
    while (wait_valid_start + timeout_duration > std::chrono::steady_clock::now()) {
        SessionRef session_ref;
        r = tryCopySessionRef(session_ref);
        if (r == ResultCode::error_busy) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }

        if (!session_shm.openForReadOnly(session_ref.path, session_ref.size)) {
            return ResultCode::error_cannot_connect;
        }

        const SC_API_PROTOCOL_Session_t* session_ptr =
            reinterpret_cast<const SC_API_PROTOCOL_Session_t*>(session_shm.getBuffer());

        uint32_t session_data_size = session_ptr->session_data_size;
        if (session_ptr->session_id != session_ref.id || session_ptr->session_version != session_ref.version ||
            session_data_size > session_ref.size) {
            // Something weird is happing and there is mismatch between shared memory data
            return ResultCode::error_cannot_connect;
        }

        if (session_ptr->state != SC_API_PROTOCOL_SESSION_ACTIVE) {
            return ResultCode::error_cannot_connect;
        }

        // Copy all session data to separate buffer so we avoid any potential issues with malicious actors modifying
        // shared memory during this handshake process

        // Annoying work-arounds to make sure the buffer is properly aligned
        session_data_buf.reset(internal::alignedAlloc(8, session_data_size));
        std::memcpy(session_data_buf.get(), session_ptr, session_data_size);
        session_data = reinterpret_cast<const SC_API_PROTOCOL_Session_t*>(session_data_buf.get());

        if (session_data->state != SC_API_PROTOCOL_SESSION_ACTIVE) {
            return ResultCode::error_cannot_connect;
        }
        break;
    }

    if (!session_data) {
        return ResultCode::error_timeout;
    }

    return tryOpeningSession(session_data, std::move(session_shm));
}

void ApiCore::Impl::sessionClosed(Session* session) {
    std::lock_guard lock(m_);

    if (active_session_.get() == session) {
        active_session_ = nullptr;
    }
}

ResultCode ApiCore::Impl::openCoreShmHandle() {
    if (!shm_core_ptr_) {
        if (!shm_core_.openForReadOnly(SC_API_PROTOCOL_CORE_SHM_FILENAME, SC_API_PROTOCOL_CORE_SHM_SIZE)) {
            return ResultCode::error_cannot_connect;
        }

        shm_core_ptr_ = (const SC_API_PROTOCOL_Core_t*)shm_core_.getBuffer();
    }

    if (!SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_CORE_SHM_VERSION, shm_core_ptr_->version)) {
        return ResultCode::error_incompatible;
    }

    return ResultCode::ok;
}

ResultCode ApiCore::Impl::tryCopySessionRef(SessionRef& session) {
    uint32_t start_revision = shm_core_ptr_->revision_counter;
    if ((start_revision & 1) != 0) {
        return ResultCode::error_busy;
    }
    std::atomic_thread_fence(std::memory_order_acquire);

    bool valid = SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_CORE_SHM_VERSION, shm_core_ptr_->version);
    if (!valid) {
        return ResultCode::error_incompatible;
    }

    if (shm_core_ptr_->state != SC_API_PROTOCOL_CORE_ACTIVE) {
        return ResultCode::error_cannot_connect;
    }

    session.id      = shm_core_ptr_->session_id;
    session.version = shm_core_ptr_->session_version;
    session.size    = shm_core_ptr_->session_shm_size;

    static_assert(sizeof(session.path) == sizeof(shm_core_ptr_->session_shm_path));

    for (int i = 0; i < sizeof(shm_core_ptr_->session_shm_path); ++i) {
        session.path[i] = shm_core_ptr_->session_shm_path[i];
    }

    std::atomic_thread_fence(std::memory_order_release);

    if (shm_core_ptr_->revision_counter != start_revision) {
        return ResultCode::error_busy;
    }

    // Make sure that null-termination is correct
    if (session.path[sizeof(session.path) - 1] != '\0') {
        return ResultCode::error_protocol;
    }
    return ResultCode::ok;
}

ResultCode ApiCore::Impl::tryOpeningSession(const SC_API_PROTOCOL_Session_t* session_copy,
                                            internal::SharedMemory&&         shm_buf) {
    if (session_copy->udp_control_protocol_version >> 16 != SC_API_PROTOCOL_UDP_PROTOCOL_VERSION_MAJOR) {
        return ResultCode::error_incompatible;
    }

    if (session_copy->udp_control_max_plaintext_packet_size <
            SC_API_PROTOCOL_UDP_CONTROL_MIN_PLAINTEXT_PACKET_SIZE_LIMIT ||
        session_copy->udp_control_max_encrypted_packet_size <
            SC_API_PROTOCOL_UDP_CONTROL_MIN_ENCRYPTED_PACKET_SIZE_LIMIT) {
        // This shouldn't really ever happen unless some other program has edited the shared memory or bit flip occurs
        // or what ever weird
        return ResultCode::error_protocol;
    }

    std::vector<const SC_API_PROTOCOL_ShmBlockReference_t*> shm_refs;
    auto is_within_session_buffer = [&](uint32_t offset, uint32_t size, uint32_t count) {
        return (uint64_t)offset + (uint64_t)size * count <= session_copy->session_data_size;
    };

    const uint8_t* shm_block_ref_start =
        reinterpret_cast<const uint8_t*>(session_copy) + session_copy->shm_reference_offset;
    uint32_t shm_block_size = session_copy->shm_reference_size;

    if (!is_within_session_buffer(session_copy->shm_reference_offset, session_copy->shm_reference_size,
                                  session_copy->shm_reference_count)) {
        // Corrupted data
        return ResultCode::error_protocol;
    }

    shm_refs.resize(session_copy->shm_reference_count);
    for (unsigned i = 0; i < session_copy->shm_reference_count; ++i) {
        const uint8_t* ref_ptr = shm_block_ref_start + (std::size_t)shm_block_size * i;
        shm_refs[i]            = reinterpret_cast<const SC_API_PROTOCOL_ShmBlockReference_t*>(ref_ptr);
    }

    struct RequiredShmRef {
        uint32_t id;
        uint32_t version;
    };

    std::vector<RequiredShmRef> required_shm_refs = {
        {SC_API_PROTOCOL_DEVICE_INFO_SHM_ID, SC_API_PROTOCOL_DEVICE_INFO_SHM_VERSION},
        {SC_API_PROTOCOL_VARIABLE_HEADER_SHM_ID, SC_API_PROTOCOL_VARIABLE_HEADER_SHM_VERSION},
        {SC_API_PROTOCOL_VARIABLE_DATA_SHM_ID, SC_API_PROTOCOL_VARIABLE_DATA_SHM_VERSION},
        {SC_API_PROTOCOL_TELEMETRY_DEFINITION_SHM_ID, SC_API_PROTOCOL_TELEMETRY_DEFINITION_SHM_VERSION},
        {SC_API_PROTOCOL_SIM_DATA_SHM_ID, SC_API_PROTOCOL_SIM_DATA_SHM_VERSION}};

    std::vector<const SC_API_PROTOCOL_ShmBlockReference_t*> selected_shm_table;
    for (std::size_t req_i = 0; req_i < required_shm_refs.size(); ++req_i) {
        RequiredShmRef req_shm_ref = required_shm_refs[req_i];
        bool           found       = false;
        for (const SC_API_PROTOCOL_ShmBlockReference_t* shm_ref : shm_refs) {
            if (shm_ref->id == req_shm_ref.id &&
                SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(req_shm_ref.version, shm_ref->version)) {
                selected_shm_table.push_back(shm_ref);
                found = true;
                break;
            }
        }

        if (!found) {
            return ResultCode::error_incompatible;
        }
    }

    if (shm_core_ptr_->session_id != session_copy->session_id) {
        // Session changed during initialization
        // Abort this attempt and try again later
        return ResultCode::error_busy;
    }

    std::unique_ptr<Session::Internal> shm_handles = std::make_unique<Session::Internal>();

    shm_handles->session_ptr               = reinterpret_cast<const SC_API_PROTOCOL_Session_t*>(shm_buf.getBuffer());
    shm_handles->session                   = std::move(shm_buf);

    shm_handles->udp_max_plaintext_payload = session_copy->udp_control_max_plaintext_packet_size;
    shm_handles->udp_max_encrypted_payload = session_copy->udp_control_max_encrypted_packet_size;

    if (auto r = resolveSecureSessionOptions(session_copy, shm_handles->secure_session_opts); r != ResultCode::ok) {
        return r;
    }

    std::unordered_map<uint32_t, internal::ShmBlock*> shm_blocks_by_id = {
        {SC_API_PROTOCOL_DEVICE_INFO_SHM_ID, &shm_handles->device_info},
        {SC_API_PROTOCOL_VARIABLE_HEADER_SHM_ID, &shm_handles->variable_header},
        {SC_API_PROTOCOL_VARIABLE_DATA_SHM_ID, &shm_handles->variable_data},
        {SC_API_PROTOCOL_TELEMETRY_DEFINITION_SHM_ID, &shm_handles->telemetry_defs},
        {SC_API_PROTOCOL_SIM_DATA_SHM_ID, &shm_handles->sim_data}};

    bool all_opened = true;
    for (const SC_API_PROTOCOL_ShmBlockReference_t* ref : selected_shm_table) {
        auto block_it = shm_blocks_by_id.find(ref->id);
        if (block_it != shm_blocks_by_id.end()) {
            if (!block_it->second->open(*ref)) {
                all_opened = false;
                break;
            }
        } else {
            all_opened = false;
            break;
        }
    }

    if (!all_opened) {
        return ResultCode::error_cannot_connect;
    }

    shm_handles->api_event_producer_ = event_producer_;
    shm_handles->sim_data_provider.setShmBuffer((const uint8_t*)shm_handles->sim_data.getBuffer(),
                                                shm_handles->sim_data.getSize());

    active_session_                  = api_->constructSession(std::move(shm_handles), session_copy->session_id);

    event_producer_->notifyEvent(
        session_event::SessionStateChanged{{active_session_}, Session::State::connected_monitor, 0, 0});

    return ResultCode::ok;
}

ResultCode ApiCore::Impl::resolveSecureSessionOptions(const SC_API_PROTOCOL_Session_t* session_copy,
                                                      SecureSessionOptions&            opts) {
    auto is_within_session_buffer = [&](uint32_t offset, uint32_t size) {
        return (uint64_t)offset + (uint64_t)size <= session_copy->session_data_size;
    };

    for (int i = 0; i < SC_API_PROTOCOL_MAX_PUBLIC_KEYS; ++i) {
        if (session_copy->public_key_offsets[i] == 0) continue;

        if (!is_within_session_buffer(session_copy->public_key_offsets[i], sizeof(SC_API_PROTOCOL_PublicKeyHeader_t))) {
            return ResultCode::error_protocol;
        }

        const uint8_t* hdr_ptr = reinterpret_cast<const uint8_t*>(session_copy) + session_copy->public_key_offsets[i];
        const SC_API_PROTOCOL_PublicKeyHeader_t& hdr =
            *reinterpret_cast<const SC_API_PROTOCOL_PublicKeyHeader_t*>(hdr_ptr);

        // Bounds check offsets and sizes
        if (!is_within_session_buffer(session_copy->public_key_offsets[i] + hdr.key_offset, hdr.key_size) ||
            !is_within_session_buffer(session_copy->public_key_offsets[i] + hdr.signature_offset, hdr.signature_size)) {
            return ResultCode::error_protocol;
        }

        SecureSessionOptions::Method m;
        m.method = (SC_API_PROTOCOL_SecurityMethod_t)hdr.security_method;
        m.public_key.resize(hdr.key_size);
        m.signature.resize(hdr.signature_size);
        std::memcpy(m.public_key.data(), hdr_ptr + hdr.key_offset, hdr.key_size);
        std::memcpy(m.signature.data(), hdr_ptr + hdr.signature_offset, hdr.signature_size);
        opts.options.push_back(std::move(m));
    }
    opts.session_id = session_copy->session_id;
    return ResultCode::ok;
}

std::unique_ptr<util::EventQueue<Event>> ApiCore::Impl::createEventQueue() {
    auto eq = std::make_unique<util::EventQueue<Event>>();

    {
        std::lock_guard lock(m_);
        // If session is currently established add session state event so that everything stays in sync
        if (active_session_) {
            std::lock_guard lock(active_session_->m_);

            session_event::SessionStateChanged ev{{active_session_},
                                                  active_session_->state_,
                                                  active_session_->controller_id_,
                                                  active_session_->control_flags_};
            event_producer_->pushInitialEvent(eq.get(), std::move(ev));
        }
        event_producer_->addEventQueue(eq.get());
    }

    return eq;
}

ApiCore::ApiCore() : p_(std::make_unique<Impl>(this)) { initOnlyOnce(); }

ApiCore::~ApiCore() {}

ResultCode ApiCore::openSession(std::shared_ptr<Session>& session_handle_out) {
    ResultCode r = p_->openSession();
    if (r == ResultCode::ok) {
        session_handle_out = p_->getSession();
    }
    return r;
}

std::shared_ptr<Session> ApiCore::getOpenSession() const { return p_->getSession(); }

std::unique_ptr<util::EventQueue<Event>> ApiCore::createEventQueue() { return p_->createEventQueue(); }

std::shared_ptr<Session> ApiCore::constructSession(std::unique_ptr<Session::Internal> shm_handles, uint32_t session_id) {
    // make_shared requires public constructor so we need to do a little hidden wrapper
    struct make_shared_enabler : Session {
        make_shared_enabler(ApiCore* api, std::unique_ptr<Internal> handles, uint32_t session_id)
            : Session(api, std::move(handles), session_id) {}
    };

    return std::make_shared<make_shared_enabler>(this, std::move(shm_handles), session_id);
}

}  // namespace sc_api::core
