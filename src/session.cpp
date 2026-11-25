#include "sc-api/session.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "api_internal.h"
#include "command_parsing.h"
#include "compatibility.h"
#include "device_info_internal.h"
#include "sc-api/command.h"
#include "sc-api/events.h"
#include "sc-api/protocol/core.h"
#include "sc-api/result.h"
#include "sc-api/sim_data_builder.h"
#include "sc-api/util/bson_builder.h"
#include "sc-api/util/bson_reader.h"
#include "sc-api/version.h"

namespace sc_api {

namespace {

/** Per-call synchronization state for a blocking command. Heap-allocated and co-owned (shared_ptr)
 *  by both the waiting thread and the result callback, so it outlives a timed-out wait: a late
 *  response then writes into live state instead of a dangling stack pointer. Per-call (not a shared
 *  global) so concurrent or reentrant blocking commands never clash. The result callback runs on
 *  the session io thread, a different thread from the caller - everything it touches goes through
 *  the captured shared_ptr, never thread-local storage. */
template <typename T>
struct BlockingCall {
    std::mutex              mutex;
    std::condition_variable cv;
    bool                    done = false;
    T                       result{};
};

}  // namespace

namespace {

constexpr uint32_t k_rx_buffer_size                                      = 0x10000;

/** Upper bound for the response size the backend declares in the session block */
constexpr uint32_t k_max_rx_document_size_limit                          = 16 * 1024 * 1024;

constexpr uint32_t k_max_id_name_size                                    = 64;

constexpr auto k_keep_alive_timeout                                      = std::chrono::milliseconds(1000);

static constexpr std::pair<uint32_t, const char*> k_control_flag_names[] = {{Session::control_ffb_effects, "ffb"},
                                                                            {Session::control_telemetry, "telemetry"},
                                                                            {Session::control_sim_data, "sim_data"},
                                                                            {Session::control_display, "display"}};

}  // namespace

ResultCode Session::close() {
    {
        std::lock_guard lock(m_);
        if (state_ == State::invalid) return ResultCode::error_invalid_session_state;
        closeNoLock();
    }

    // Callbacks may call back into this session, so they run without the session lock
    if (p_) {
        p_->pending_commands.close(ResultCode::error_invalid_session_state, "Session closed");
    }
    return ResultCode::ok;
}

void Session::closeNoLock() {
    if (state_ == State::connected_control) {
        // TODO: Send controller disconnect command?
    }

    // Close socket if it is open, but don't destroy shared memory objects
    if (p_) {
        asio::error_code ec;
        try {
            p_->periodic_update_timer.cancel();
        } catch (const std::exception& ex) {
            // Ignore exceptions when we are cancelling timers
            std::cerr << "Exception during periodic timer cancellation: " << ex.what() << std::endl;
        }

        for (auto& [id, timer] : p_->periodic_timers) {
            try {
                timer.cancel();
            } catch (const std::exception& ex) {
                // Ignore any exceptions when we are cancelling timers
                std::cerr << "Exception during periodic timer cancellation: " << ex.what() << std::endl;
            }
        }

        if (p_->main_socket.is_open()) {
            p_->main_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            p_->main_socket.close(ec);
        }
        p_->high_priority_socket.close(ec);
        p_->io_ctx.stop();
    }

    state_ = State::invalid;

    if (api_) {
        api_->p_->sessionClosed(this);
        api_ = nullptr;
    }
}

std::shared_ptr<sim_data::SimData> Session::getSimData() {
    if (!p_) return nullptr;

    p_->sim_data_provider.update();
    return p_->sim_data_provider.parseSimData();
}

bool Session::asyncCommand(CommandRequest&& req, std::function<void(const AsyncCommandResult&)> result_cb) {
    if (!p_) return false;

    int32_t              cmd_id        = p_->command_id_counter.fetch_add(1, std::memory_order_relaxed);
    std::vector<uint8_t> packet_buffer = req.finalize(cmd_id);
    return p_->startSendCommand(packet_buffer, cmd_id, std::move(result_cb)) == ResultCode::ok;
}

CommandResult Session::blockingCommand(CommandRequest&& req, std::chrono::milliseconds timeout) {
    if (!p_) {
        return CommandResult::createFailure(ResultCode::error_invalid_state, "Session is closed");
    }

    int32_t              cmd_id        = p_->command_id_counter.fetch_add(1, std::memory_order_relaxed);
    std::vector<uint8_t> packet_buffer = req.finalize(cmd_id);

    auto state                         = std::make_shared<BlockingCall<CommandResult>>();
    auto result_cb                     = [state](const AsyncCommandResult& r) {
        {
            std::lock_guard lock(state->mutex);
            state->result = CommandResult::createFromAsync(r);
            state->done   = true;
        }
        state->cv.notify_all();
    };

    if (ResultCode r = p_->startSendCommand(packet_buffer, cmd_id, result_cb); r != ResultCode::ok) {
        if (r == ResultCode::error_busy) {
            return CommandResult::createFailure(r, "Too many pending commands");
        }
        return CommandResult::createFailure(ResultCode::error_no_control, "Not registered to control");
    }

    std::unique_lock lock(state->mutex);
    if (!state->cv.wait_for(lock, timeout, [&] { return state->done; })) {
        lock.unlock();
        p_->pending_commands.remove(cmd_id);
        return CommandResult::createFailure(ResultCode::error_timeout, "Didn't receive response within timeout");
    }

    return state->result;
}

ResultCode Session::blockingSimpleCommand(CommandRequest&& req, std::chrono::milliseconds timeout) {
    if (!p_) {
        return ResultCode::error_invalid_state;
    }

    int32_t              cmd_id        = p_->command_id_counter.fetch_add(1, std::memory_order_relaxed);
    std::vector<uint8_t> packet_buffer = req.finalize(cmd_id);

    auto state                         = std::make_shared<BlockingCall<ResultCode>>();
    auto result_cb                     = [state](const AsyncCommandResult& r) {
        {
            std::lock_guard lock(state->mutex);
            state->result = r.getResultCode();
            state->done   = true;
        }
        state->cv.notify_all();
    };

    if (ResultCode r = p_->startSendCommand(packet_buffer, cmd_id, result_cb); r != ResultCode::ok) {
        return r == ResultCode::error_busy ? r : ResultCode::error_no_control;
    }

    std::unique_lock lock(state->mutex);
    if (!state->cv.wait_for(lock, timeout, [&] { return state->done; })) {
        lock.unlock();
        p_->pending_commands.remove(cmd_id);
        return ResultCode::error_timeout;
    }

    return state->result;
}

bool Session::blockingReplaceSimData(sim_data::SimDataUpdateBuilder& builder) {
    CommandRequest req;
    req.initializeFrom("sim_data", "replace", builder.finish().first);
    return blockingSimpleCommand(std::move(req)) == ResultCode::ok;
}

bool Session::asyncReplaceSimData(sim_data::SimDataUpdateBuilder&                builder,
                                  std::function<void(const AsyncCommandResult&)> result_cb) {
    CommandRequest req;
    req.initializeFrom("sim_data", "replace", builder.finish().first);
    return asyncCommand(std::move(req), std::move(result_cb));
}

bool Session::blockingUpdateSimData(sim_data::SimDataUpdateBuilder& builder) {
    CommandRequest req;
    req.initializeFrom("sim_data", "update", builder.finish().first);
    return blockingSimpleCommand(std::move(req)) == ResultCode::ok;
}

bool Session::asyncUpdateSimData(sim_data::SimDataUpdateBuilder&                builder,
                                 std::function<void(const AsyncCommandResult&)> result_cb) {
    CommandRequest req;
    req.initializeFrom("sim_data", "update", builder.finish().first);
    return asyncCommand(std::move(req), std::move(result_cb));
}

std::shared_ptr<device_info::FullInfo> Session::getDeviceInfo() {
    if (!p_) return nullptr;

    p_->dev_info_provider_.update();
    return p_->dev_info_provider_.parseDeviceInfo();
}

VariableDefinitions Session::getVariables() { return p_->var_provider_.definitions(); }

TelemetryDefinitions Session::getTelemetries() {
    if (!p_) return {};

    p_->telemetry_.updateDefinitions();
    return p_->telemetry_.getDefinitions(shared_from_this());
}

Session::PeriodicTimerHandle Session::createPeriodicTimer(std::chrono::milliseconds period,
                                                          std::function<void()>     callback) {
    std::lock_guard lock(m_);
    int32_t         id             = ++p_->periodic_timer_id_counter;
    auto            emplace_result = p_->periodic_timers.emplace(id, asio::steady_timer(p_->io_ctx));
    assert(emplace_result.second);
    auto& timer = emplace_result.first->second;

    p_->startPeriodicTimer(timer, period, std::move(callback));
    return PeriodicTimerHandle(this, id);
}

Session::Session(ApiCore* api, std::unique_ptr<Internal> handles, uint32_t session_id)
    : api_(api), p_(std::move(handles)), control_flags_(0), session_id_(session_id) {
    startPeriodicUpdateTimer();
    p_->p_ = this;
    p_->telemetry_.initialize(p_->telemetry_defs.getBuffer(), p_->telemetry_defs.getSize());

    p_->var_provider_.initialize(this, p_->variable_header.getBuffer(), p_->variable_header.getSize(),
                                 p_->variable_data.getBuffer(), p_->variable_data.getSize());
    p_->dev_info_provider_.initialize(p_->device_info.getBuffer(), p_->device_info.getSize());

    // Immediately check definitions instead of waiting for periodic timer to do it
    asio::post(p_->io_ctx, [&]() { checkDefinitions(); });
}

void Session::startPeriodicUpdateTimer() {
    p_->periodic_update_timer.expires_after(std::chrono::milliseconds(500));
    p_->periodic_update_timer.async_wait([this](asio::error_code ec) {
        if (!ec) {
            if (periodicUpdate()) {
                startPeriodicUpdateTimer();
            }
        }
    });
}

bool Session::periodicUpdate() {
    std::unique_lock lock(m_);

    uint32_t keep_alive = p_->session_ptr->keep_alive_counter;
    if (keep_alive != prev_keep_alive_value_) {
        prev_keep_alive_value_ = keep_alive;
        prev_keep_alive_       = std::chrono::steady_clock::now();
    }
    switch (state_) {
        case State::connected_monitor:
            if (prev_keep_alive_ + k_keep_alive_timeout < std::chrono::steady_clock::now()) {
                lock.unlock();
                disconnected();
                return false;
            }
            break;
        case State::connected_control:
            // We have TCP connection to the backend so we can just assume we have valid session as long as that
            // connection stays alive
            break;
        case State::session_lost:
        case State::invalid:
            return false;
    }

    checkDefinitions();
    return true;
}

void Session::disconnected() {
    std::lock_guard lock(m_);
    if (state_ == State::invalid) return;

    p_->periodic_update_timer.cancel();
    state_   = State::session_lost;

    auto ptr = shared_from_this();
    p_->api_event_producer_->notifyEvent(event::SessionStateChanged{{ptr}, state_, 0, 0u});

    if (is_running_) {
        p_->io_ctx.stop();
    }
}

void Session::checkDefinitions() {
    bool dev_info_changed       = p_->dev_info_provider_.update() == detail::DeviceInfoProvider::UpdateResult::new_data;
    bool var_defs_changed       = p_->var_provider_.updateDefinitions();
    bool telemetry_defs_changed = p_->telemetry_.updateDefinitions();
    bool sim_data_changed       = p_->sim_data_provider.update() == detail::SimDataProvider::UpdateResult::new_data;

    if (dev_info_changed) {
        p_->api_event_producer_->notifyEvent(event::DeviceInfoChanged{{shared_from_this()}});
    }
    if (var_defs_changed) {
        p_->api_event_producer_->notifyEvent(event::VariableDefinitionsChanged{shared_from_this()});
    }
    if (telemetry_defs_changed) {
        p_->api_event_producer_->notifyEvent(event::TelemetryDefinitionsChanged{shared_from_this()});
    }
    if (sim_data_changed) {
        p_->api_event_producer_->notifyEvent(event::SimDataChanged{shared_from_this()});
    }
}

Session::~Session() {
    try {
        close();
    } catch (std::exception& ex) {
        std::cerr << "Exception during sc_api::Session::~Session: " << ex.what() << std::endl;
    }
}

ResultCode Session::registerToControl(uint32_t control_flags, const std::string& id_name,
                                      const ApiUserInformation&       user_info,
                                      const std::vector<std::string>& extra_controls) {
    std::lock_guard lock(m_);

    if (id_name.size() > k_max_id_name_size || (control_flags == 0 && extra_controls.empty())) {
        return ResultCode::error_invalid_argument;
    }

    std::vector<uint8_t> packet_buffer(1024);
    util::BsonBuilder    builder(&packet_buffer);
    builder.docAddElement("00type", 1);
    builder.docAddElement("service", "core");
    {
        builder.docBeginSubDoc("cmd");
        {
            builder.docBeginSubDoc("register");
            builder.docAddElement("id", id_name);
            builder.docAddElement("name", user_info.display_name);
            builder.docAddElement("protocol_version", (int64_t)SC_API_PROTOCOL_TCP_CORE_VERSION);
            builder.docAddElement("core_version_major", SC_API_CORE_VERSION_MAJOR);
            builder.docAddElement("core_version_minor", SC_API_CORE_VERSION_MINOR);
            builder.docAddElement("core_version_patch", SC_API_CORE_VERSION_PATCH);

            builder.docBeginSubDoc("metadata");
            if (!user_info.version_string.empty()) builder.docAddElement("version", user_info.version_string);
            if (!user_info.author.empty()) builder.docAddElement("author", user_info.author);
            if (!user_info.path.empty()) builder.docAddElement("filepath", user_info.path);
            if (!user_info.type.empty()) builder.docAddElement("type", user_info.type);
            builder.endDocument();

            builder.docBeginSubArray("control");
            for (auto [flag, name] : k_control_flag_names) {
                if ((control_flags & flag) != 0) {
                    builder.arrayAddElement(name);
                }
            }
            for (const std::string& name : extra_controls) {
                builder.arrayAddElement(name);
            }
            builder.endArray();

            builder.endDocument();
        }
        builder.endDocument();
    }
    auto [ptr, size] = builder.finish();

    asio::error_code       ec;
    std::array<uint8_t, 4> udp_ipv4_address = {
        p_->session_ptr->udp_control_address[0], p_->session_ptr->udp_control_address[1],
        p_->session_ptr->udp_control_address[2], p_->session_ptr->udp_control_address[3]};
    std::array<uint8_t, 4> tcp_ipv4_address = {
        p_->session_ptr->tcp_core_address[0], p_->session_ptr->tcp_core_address[1],
        p_->session_ptr->tcp_core_address[2], p_->session_ptr->tcp_core_address[3]};

    p_->high_priority_socket_target =
        asio::ip::udp::endpoint(asio::ip::make_address_v4(udp_ipv4_address), p_->session_ptr->udp_control_port);
    p_->high_priority_socket.open(asio::ip::udp::v4(), ec);
    p_->high_priority_socket.non_blocking(true);
    if (ec) {
        return ResultCode::error_cannot_connect;
    }

    p_->main_socket.connect(
        asio::ip::tcp::endpoint(asio::ip::make_address_v4(tcp_ipv4_address), p_->session_ptr->tcp_core_port), ec);
    if (ec) {
        return ResultCode::error_cannot_connect;
    }

    p_->max_rx_document_size =
        std::clamp(p_->session_ptr->tcp_core_max_packet_size, k_rx_buffer_size, k_max_rx_document_size_limit);
    p_->receive_buffer.resize(k_rx_buffer_size);

    std::size_t written_bytes = asio::write(p_->main_socket, asio::buffer(ptr, size));
    if (static_cast<std::size_t>(size) != written_bytes) {
        p_->main_socket.close();
        return ResultCode::error_cannot_connect;
    }

    std::size_t read_bytes   = 0;
    std::size_t needed_bytes = 5;
    while (true) {
        read_bytes +=
            p_->main_socket.read_some(asio::buffer(&p_->receive_buffer[read_bytes], needed_bytes - read_bytes), ec);
        if (ec) {
            p_->main_socket.close();
            return ResultCode::error_cannot_connect;
        }

        if (read_bytes == needed_bytes) {
            int32_t total_document_size = util::BsonReader::getTotalDocumentSize(p_->receive_buffer.data());
            if (total_document_size < 5 || static_cast<std::size_t>(total_document_size) > k_rx_buffer_size) {
                p_->main_socket.close();
                return ResultCode::error_protocol;
            }
            needed_bytes = total_document_size;
            if (read_bytes == static_cast<std::size_t>(total_document_size)) {
                break;
            }
        }
    }

    util::BsonReader reader(p_->receive_buffer.data(), read_bytes);

    std::string_view command_name;
    int32_t          result = parseCommandResultHeader(reader, command_name);
    if (result != 0) {
        p_->main_socket.close();
        return ResultCode::error_protocol;
    }

    if (command_name != "register") {
        p_->main_socket.close();
        return ResultCode::error_protocol;
    }

    int32_t ctrl_id = 0;
    if (!reader.tryFindAndGet("controller_id", ctrl_id) || ctrl_id <= 0 ||
        ctrl_id > std::numeric_limits<uint16_t>::max()) {
        p_->main_socket.close();
        return ResultCode::error_protocol;
    }

    uint32_t                 received_control_flags = 0;
    std::vector<std::string> received_extra_controls;
    {
        if (reader.seekKey("control") != util::BsonReader::ELEMENT_ARRAY || !reader.beginSub()) {
            p_->main_socket.close();
            return ResultCode::error_protocol;
        }

        util::BsonReader::ElementType e;
        while (!util::BsonReader::isEndOrError(e = reader.next())) {
            if (e == util::BsonReader::ELEMENT_STR) {
                const std::string_view granted = reader.stringValue();
                const auto* flag_it = std::find_if(std::begin(k_control_flag_names), std::end(k_control_flag_names),
                                                   [&](const auto& flag_name) { return granted == flag_name.second; });
                if (flag_it != std::end(k_control_flag_names)) {
                    received_control_flags |= flag_it->first;
                } else {
                    received_extra_controls.emplace_back(granted);
                }
            }
        }
        reader.endSub();

        control_flags_  = received_control_flags;
        extra_controls_ = std::move(received_extra_controls);
    }

    control_id_name_ = id_name;
    controller_id_   = static_cast<uint16_t>(ctrl_id);
    p_->startReceive();

    state_ = State::connected_control;
    if (is_running_) {
        p_->io_ctx.stop();
    }

    p_->api_event_producer_->notifyEvent(
        event::SessionStateChanged{{shared_from_this()}, state_, controller_id_, received_control_flags});

    return ResultCode::ok;
}

uint32_t Session::getControlFlags() const {
    std::lock_guard lock(m_);
    return control_flags_;
}

std::vector<std::string> Session::getExtraControls() const {
    std::lock_guard lock(m_);
    return extra_controls_;
}

Session::State Session::poll() {
    if (!p_ || !api_) return State::invalid;

    p_->io_ctx.poll();
    return state_;
}

Session::State Session::runUntilStateChanges() {
    assert(!is_running_);
    if (!p_ || !api_) return State::invalid;

    if (p_->io_ctx.stopped()) {
        p_->io_ctx.restart();
    }
    is_running_ = true;
    p_->io_ctx.run();
    is_running_ = false;
    return state_;
}

void Session::stop() {
    if (!p_) return;
    try {
        p_->io_ctx.stop();
    } catch (const asio::system_error& e) {
        // Shouldn't happen. This is just to clean clang-tidy warning
        std::cerr << "SC-API: exception during Session::stop: " << e.what() << std::endl;
    }
}

Session::State Session::getState() const { return state_.load(); }

BackendVersion Session::getBackendVersion() const {
    return BackendVersion{p_->session_ptr->backend_version_major, p_->session_ptr->backend_version_minor,
                          p_->session_ptr->backend_version_patch};
}

bool Session::Internal::sendHighPrio(const char* raw_data, std::size_t length) {
    std::lock_guard  lock(high_prio_mutex);
    asio::error_code ec;
    high_priority_socket.send_to(asio::buffer(raw_data, length), high_priority_socket_target, 0, ec);
    return !ec;
}

ResultCode Session::Internal::startSendCommand(std::vector<uint8_t> tx_data, int32_t cmd_id,
                                               std::function<void(const AsyncCommandResult&)> cb) {
    std::lock_guard lock(main_socket_mutex);
    if (!main_socket.is_open()) return ResultCode::error_invalid_session_state;

    if (ResultCode r = pending_commands.add(cmd_id, std::move(cb)); r != ResultCode::ok) return r;
    main_socket_tx_queue.push_back(std::move(tx_data));
    startNextSendNoLock();
    return ResultCode::ok;
}

bool Session::Internal::sendWithoutReply(CommandRequest&& req) {
    // No callback waits for this id, so parsePacket drops the reply
    std::vector<uint8_t> tx_data = req.finalize(command_id_counter.fetch_add(1, std::memory_order_relaxed));

    std::lock_guard lock(main_socket_mutex);
    if (!main_socket.is_open()) return false;

    main_socket_tx_queue.push_back(std::move(tx_data));
    startNextSendNoLock();
    return true;
}

void Session::Internal::startNextSend() {
    std::lock_guard lock(main_socket_mutex);
    startNextSendNoLock();
}

void Session::Internal::startNextSendNoLock() {
    if (send_in_progress || main_socket_tx_queue.empty()) {
        return;
    }

    auto tx_data = std::move(main_socket_tx_queue.front());
    main_socket_tx_queue.erase(main_socket_tx_queue.begin());

    auto tx_buf      = asio::buffer(tx_data);
    send_in_progress = true;
    asio::async_write(main_socket, tx_buf,
                      [tx_data = std::move(tx_data), this](asio::error_code ec, std::size_t written) {
                          send_in_progress = false;
                          if (!ec) {
                              startNextSend();
                          } else {
                              std::cerr << "asio::async_write failed: " << ec.message() << std::endl;
                          }
                      });
}

void Session::Internal::startReceive() {
    if (receiving_in_progress || !main_socket.is_open()) return;

    receiving_in_progress = true;
    main_socket.async_read_some(
        asio::buffer(&receive_buffer[receive_buffer_used], receive_buffer.size() - receive_buffer_used),
        [this](asio::error_code ec, size_t rx_size) {
            receiving_in_progress = false;
            if (!ec) {
                receive_buffer_used += (uint32_t)rx_size;

                if (tryParsePacket()) {
                    startReceive();
                }
            } else if (ec != asio::error::operation_aborted) {
                if ((asio::error::eof != ec) && (asio::error::connection_reset != ec)) {
                    std::cerr << "async_read_some failed: " << ec.message() << std::endl;
                }
                connectionLost("Connection to the backend was lost");
            }
        });
}

void Session::Internal::connectionLost(std::string_view reason) {
    {
        std::lock_guard  lock(main_socket_mutex);
        asio::error_code ec;
        main_socket.shutdown(asio::ip::tcp::socket::shutdown_both, ec);
        main_socket.close(ec);
    }

    // The state changes first, so a throwing callback cannot leave the session looking connected
    p_->disconnected();
    pending_commands.close(ResultCode::error_invalid_session_state, reason);
}

bool Session::Internal::tryParsePacket() {
    detail::FramingResult r =
        detail::consumeDocuments(receive_buffer, receive_buffer_used, max_rx_document_size,
                                 [this](const uint8_t* data, int32_t size) { parsePacket(data, size); });
    if (r == detail::FramingResult::invalid) {
        std::cerr << "SC-API: invalid response document from the backend" << std::endl;
        connectionLost("Invalid response from the backend");
        return false;
    }
    return true;
}

void Session::Internal::parsePacket(const uint8_t* data, int32_t size) {
    using util::BsonReader;
    using E = BsonReader::ElementType;
    BsonReader r(data, size);

    E first = r.next();
    if (first != E::ELEMENT_I32 || r.key() != "00type") {
        // First element should always be "00type" that defines the content. Currently we only support type==1 which
        // means command response
        return;
    }

    if (r.int32Value() != 1) return;

    // This is command data
    // Each command always produces exactly one response document

    int32_t          cmd_id         = -1;
    ResultCode       result_code    = ResultCode::ok;
    const uint8_t*   result_payload = nullptr;
    std::string_view error_message{};
    E                e;
    while (true) {
        e = r.next();
        if (r.atEnd()) break;
        if (r.key() == "user-data" && e == E::ELEMENT_I32) {
            cmd_id = r.int32Value();
        } else if (r.key() == "service" && e == E::ELEMENT_STR) {
            // r.stringValue();
        } else if (r.key() == "data" && e == E::ELEMENT_DOC) {
            r.beginSub();
            if (r.next() == E::ELEMENT_DOC) {
                result_payload = r.subdocument().first;
            }
            r.endSub();
        } else if (r.key() == "result" && e == E::ELEMENT_I32) {
            result_code = (ResultCode)r.int32Value();
        } else if (r.key() == "error_message" && e == E::ELEMENT_STR) {
            error_message = r.stringValue();
        } else {
            std::cerr << "Unexpected data: " << (int)e << std::endl;
        }
    }

    if (cmd_id != -1 && !BsonReader::isError(e)) {
        if (detail::CommandCallback callback = pending_commands.take(cmd_id)) {
            if (result_code == ResultCode::ok) {
                callback(AsyncCommandResult::createSuccess(result_payload));
            } else {
                callback(AsyncCommandResult::createFailure(result_code, error_message));
            }
        }
    }
}

void Session::Internal::startPeriodicTimer(asio::steady_timer& timer, std::chrono::milliseconds period,
                                           std::function<void()>&& cb) {
    timer.expires_after(period);
    timer.async_wait([this, &timer, period, cb = std::move(cb)](asio::error_code ec) mutable {
        if (!ec) {
            cb();
            startPeriodicTimer(timer, period, std::move(cb));
        }
    });
}

void Session::PeriodicTimerHandle::destroy() noexcept {
    if (handle_ < 0) return;
    auto s = session_.lock();

    if (s) {
        std::lock_guard lock(s->m_);
        auto&           timer_map = s->p_->periodic_timers;
        auto            it        = timer_map.find(handle_);
        if (it != timer_map.end()) {
            it->second.cancel();
            timer_map.erase(it);
        }
    }

    session_.reset();
    handle_ = -1;
}

Session::PeriodicTimerHandle::PeriodicTimerHandle(Session* session, int32_t handle)
    : session_(session->weak_from_this()), handle_(handle) {}

}  // namespace sc_api
