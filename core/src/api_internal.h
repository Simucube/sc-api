#ifndef SC_API_SESSION_INTERNAL_H
#define SC_API_SESSION_INTERNAL_H
#include <asio.hpp>
#include <asio/ip/tcp.hpp>
#include <asio/ip/udp.hpp>
#include <chrono>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "compatibility.h"
#include "device_info_internal.h"
#include "sc-api/core/api_core.h"
#include "sc-api/core/protocol/core.h"
#include "sc-api/core/variables.h"
#include "sim_data_internal.h"
#include "telemetry_internal.h"
#include "variables_internal.h"

namespace sc_api::core {

struct Session::Internal {
    Session*                         p_;
    const SC_API_PROTOCOL_Session_t* session_ptr;
    SecureSessionOptions             secure_session_opts;

    internal::SharedMemory session;
    internal::ShmBlock     device_info;
    internal::ShmBlock     variable_header;
    internal::ShmBlock     variable_data;
    internal::ShmBlock     telemetry_defs;
    internal::ShmBlock     sim_data;

    unsigned udp_max_encrypted_payload = 0;
    unsigned udp_max_plaintext_payload = 0;

    asio::io_context        io_ctx;
    std::mutex              high_prio_mutex;
    std::condition_variable high_prio_sync_cv;
    asio::ip::udp::endpoint high_priority_socket_target;
    asio::ip::udp::socket   high_priority_socket{io_ctx};

    asio::ip::tcp::socket                                                   main_socket{io_ctx};
    std::vector<std::vector<uint8_t>>                                       main_socket_tx_queue;
    std::unordered_map<int, std::function<void(const AsyncCommandResult&)>> command_result_handlers;
    std::atomic_int command_id_counter;  // Used to connect request to response

    std::vector<uint8_t> receive_buffer;
    uint32_t             receive_buffer_used   = 0;
    bool                 receiving_in_progress = false;

    std::mutex main_socket_mutex;
    bool       send_in_progress = false;

    asio::steady_timer periodic_update_timer{io_ctx};

    std::unordered_map<int32_t, asio::steady_timer> periodic_timers;
    int32_t                                         periodic_timer_id_counter = 0;

    std::shared_ptr<util::EventProducer<Event>> api_event_producer_;

    internal::SimDataProvider sim_data_provider;
    internal::TelemetrySystem    telemetry_;
    internal::VariableProvider   var_provider_;
    internal::DeviceInfoProvider dev_info_provider_;

    bool sendHighPrio(const char* raw_data, std::size_t length);

    bool startSendCommand(std::vector<uint8_t> tx_data, int32_t cmd_id,
                          std::function<void(const AsyncCommandResult&)> cb);
    void startNextSend();
    void startNextSendNoLock();
    void startReceive();
    void tryParsePacket();
    void parsePacket(const uint8_t* data, int32_t size);

    void startPeriodicTimer(asio::steady_timer& timer, std::chrono::milliseconds period, std::function<void()>&& cb);
};

class ApiCore::Impl {
    friend class Session;

    struct SessionRef {
        uint32_t id;
        uint32_t version;
        uint32_t size;
        char     path[64];
    };

    struct PublicKeyData {
        SC_API_PROTOCOL_PublicKeyHeader_t header;
        std::vector<uint8_t>              key;
        std::vector<uint8_t>              signature;
    };

public:
    Impl(ApiCore* api) : api_(api), event_producer_(std::make_shared<util::EventProducer<Event>>()) {}
    ~Impl();

    ResultCode openSession();

    std::shared_ptr<Session> getSession() const {
        std::lock_guard lock(m_);
        return active_session_;
    }

    std::unique_ptr<util::EventQueue<Event>> createEventQueue();

private:
    void sessionClosed(Session* session);

    ResultCode openCoreShmHandle();

    ResultCode tryCopySessionRef(SessionRef& session);

    /** Tries contructing */
    ResultCode tryOpeningSession(const SC_API_PROTOCOL_Session_t* session_copy, internal::SharedMemory&& shm_buf);

    static ResultCode resolveSecureSessionOptions(const SC_API_PROTOCOL_Session_t* session_copy,
                                                  SecureSessionOptions&            opts);

    static constexpr std::chrono::milliseconds k_keep_alive_timeout{200};

    mutable std::mutex       m_;
    Session::State           session_state_ = Session::State::invalid;
    std::shared_ptr<Session> active_session_;

    const SC_API_PROTOCOL_Core_t* shm_core_ptr_ = nullptr;
    internal::SharedMemory        shm_core_;
    ApiCore*                      api_;

    std::shared_ptr<util::EventProducer<Event>> event_producer_;
};

}  // namespace sc_api::core

#endif  // SC_API_SESSION_INTERNAL_H
