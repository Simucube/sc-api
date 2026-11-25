#ifndef SC_API_COMMAND_CHANNEL_H_
#define SC_API_COMMAND_CHANNEL_H_
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "sc-api/result.h"
#include "sc-api/session_fwd.h"

namespace sc_api::detail {

using CommandCallback = std::function<void(const AsyncCommandResult&)>;

/** @brief Result callbacks of the commands that wait for a response. Thread-safe. */
class PendingCommands {
public:
    explicit PendingCommands(std::size_t max_pending) : max_pending_(max_pending) {}

    /** @brief Adds the callback of `cmd_id`. Returns error_busy when the table is full or the id is in use, and
     *  error_invalid_session_state when the table is closed. */
    ResultCode add(int32_t cmd_id, CommandCallback cb);

    /** @brief Removes and returns the callback of `cmd_id`; empty if there is none. */
    CommandCallback take(int32_t cmd_id);

    /** @brief Removes the callback of `cmd_id` without calling it. */
    void remove(int32_t cmd_id);

    /** @brief Rejects further adds and fails every pending callback with `code`. Callbacks run on this thread. */
    void close(ResultCode code, std::string_view message);

    std::size_t size() const;

private:
    mutable std::mutex                           m_;
    std::unordered_map<int32_t, CommandCallback> callbacks_;
    std::size_t                                  max_pending_;
    bool                                         closed_ = false;
};

enum class FramingResult {
    ok,       ///< Every complete document was consumed; the buffer has room for more data
    invalid,  ///< A document header is corrupt or larger than the limit; the stream cannot be resynchronised
};

/**
 * @brief Calls `on_document` for each complete BSON document at the start of `buffer[0, used)` and moves the
 *        remaining bytes to the front.
 *
 * Grows `buffer` up to `max_document_size` when a partial document does not fit, so on `ok` the buffer always has
 * room for the next read.
 */
FramingResult consumeDocuments(std::vector<uint8_t>& buffer, uint32_t& used, uint32_t max_document_size,
                               const std::function<void(const uint8_t* data, int32_t size)>& on_document);

}  // namespace sc_api::detail

#endif  // SC_API_COMMAND_CHANNEL_H_
