/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SESSION_FWD_H_
#define SC_API_CORE_SESSION_FWD_H_
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "result.h"

namespace sc_api::core {

class Session;

/** Result of async command. Only valid within the result callback */
struct AsyncCommandResult {
    static AsyncCommandResult createSuccess(const uint8_t* payload) {
        return AsyncCommandResult(ResultCode::ok, payload);
    }
    static AsyncCommandResult createFailure(ResultCode error_code, std::string_view message) {
        return AsyncCommandResult(error_code, (const uint8_t*)message.data());
    }

    AsyncCommandResult()                          = default;
    AsyncCommandResult(const AsyncCommandResult&) = delete;
    AsyncCommandResult(AsyncCommandResult&&)      = delete;

    bool       isSuccess() const { return result_code == ResultCode::ok; }
    ResultCode getResultCode() const { return result_code; }

    /**
     * \brief Error code string
     *
     * \return Error message if result is failure, default constructed std::string_view otherwise
     *         Error message is guaranteed to be null-terminated
     */
    std::string_view getErrorMessage() const {
        if (isSuccess()) return {};
        return std::string_view((const char*)payload_bson);
    }

    /** \brief Raw pointer to BSON containing successful command result
     *
     * \return Pointer to valid BSON document containing result data, if result is a success
     *         nullptr, if result is error
     */
    const uint8_t* getPayload() const {
        if (result_code == ResultCode::ok) return payload_bson;
        return nullptr;
    }

private:
    AsyncCommandResult(ResultCode result, const uint8_t* payload) : result_code(result), payload_bson(payload) {}

    ResultCode     result_code  = ResultCode::ok;
    const uint8_t* payload_bson = nullptr;
};

/** Command result that owns the buffer that contains result payload */
struct CommandResult {
public:
    static CommandResult createSuccess(std::vector<uint8_t> payload) {
        return CommandResult(ResultCode::ok, std::move(payload));
    }
    static CommandResult createFailure(ResultCode error_code, std::string_view message) {
        std::vector<uint8_t> msg_buf(message.size() + 1);
        std::memcpy(msg_buf.data(), message.data(), message.size());
        return CommandResult(error_code, std::move(msg_buf));
    }
    static CommandResult createFromAsync(const AsyncCommandResult& r) {
        if (r.getResultCode() != ResultCode::ok) return createFailure(r.getResultCode(), r.getErrorMessage());
        const uint8_t* payload_bson = r.getPayload();
        assert(payload_bson != nullptr);
        int32_t bson_data_size = *(const int32_t*)payload_bson;
        return createSuccess(std::vector<uint8_t>(payload_bson, payload_bson + bson_data_size));
    }

    CommandResult() : result_code(ResultCode::ok), payload_bson() {}

    bool isSuccess() const { return result_code == ResultCode::ok; }

    ResultCode getResultCode() const { return result_code; }

    std::string_view getErrorMessage() const {
        if (isSuccess()) return {};
        return std::string_view((const char*)payload_bson.data(), payload_bson.size() - 1);
    }

    const std::vector<uint8_t>& getPayload() const { return payload_bson; }

private:
    CommandResult(ResultCode result, std::vector<uint8_t> payload)
        : result_code(result), payload_bson(std::move(payload)) {}

    ResultCode           result_code = ResultCode::ok;
    std::vector<uint8_t> payload_bson;
};

/** */
enum class SessionState : uint8_t {
    /** Session isn't currently connected to the backend */
    invalid,

    /** Session is active in monitor mode and reading data from shared memory is possible, but registerToControl
     *  must be called to establish command channel if that is required  */
    connected_monitor,

    /** Controller is registered and command channel is established */
    connected_control,

    /** Connection to the backend that handles API, was lost.
     *
     * Handles to the shared data remain usable, but their data won't be updated anymore.
     * Commands don't anymore work.
     */
    session_lost,
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_SESSION_FWD_H_
