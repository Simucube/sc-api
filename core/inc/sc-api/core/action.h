/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_ACTION_H_
#define SC_API_CORE_ACTION_H_
#include <atomic>
#include <memory>
#include <vector>

#include "protocol/actions.h"

namespace sc_api::core {

class Session;

enum class ActionResult {
    /** Asynchronous operaton started */
    inprogress,

    /** Operation is complete and succeeded */
    complete,
    /** Operation is complete, but it failed */
    failed,

    /** Non-blocking operation was requested but it cannot be completed without blocking */
    would_block
};

/** Helper class for constructing and sending actions
 *
 * Actions are the method for fast one-way communication from API user to the API backend
 * Actions are used for transferring telemetry and effect pipeline data.
 *
 * Multiple actions can be constructed to one buffer before sending.
 */
class ActionBuilder {
public:
    ActionBuilder() = default;
    ActionBuilder(ActionBuilder&& builder) noexcept;
    explicit ActionBuilder(std::shared_ptr<Session> session);
    ~ActionBuilder()                               = default;

    ActionBuilder& operator=(const ActionBuilder&) = default;
    ActionBuilder& operator=(ActionBuilder&&) noexcept;

    void     init(std::shared_ptr<Session> session);
    std::shared_ptr<Session> getSession() const { return session_; }

    /** Clears builder to the empty state */
    void reset();

    /** Builds action directly to the buffer
     */
    bool build(SC_API_PROTOCOL_Action_t id, const uint8_t* payload, std::size_t payload_size, uint16_t flags = 0);

    /** Starts building action and returns pointer to the payload part of the action data */
    uint8_t* startBuilding(SC_API_PROTOCOL_Action_t action_id, std::size_t initial_payload_size, uint16_t flags = 0);

    /** Resize space for the payload
     *
     * Increases size of the current action data by the given number of bytes
     */
    uint8_t* resizePayload(std::size_t s);

    /** Tries to send given data immediately, but if that isn't possible, then falls back to asynchronous operation
     *
     * Resets builder back to the empty state.
     *
     * @param[out] result_status Atomic variable that will be updated with the result once the operation completes
     *                           Variable is set to HighPrioritySendResult::inprogress before operation is started
     */
    void asyncSend(std::atomic<ActionResult>& result_status);

    /** Tries to send data immediately without blocking
     *
     * @returns ActionResult::complete, if operation completed successfully
     *          ActionResult::failed, if operation failed
     *          ActionResult::would_block, if operation cannot be completed without blocking to wait for more
     *          resources to free. DOES NOT RESET THE BUILT PACKAGE
     */
    ActionResult sendNonBlocking();

    /** Tries to send data immediately, but will block if the socket send buffer is full (should be unlikely)
     *
     * Resets builder back to the empty state.
     *
     * @param[in]  data Pointer to the data buffer that should be sent.
     * @param[in]  length length of the data buffer to be sent
     * @returns ActionResult::complete, if operation completed successfully
     *          ActionResult::failed, if operation failed
     */
    ActionResult sendBlocking();

private:
    void finalize();

    std::shared_ptr<Session> session_;
    std::vector<uint8_t>     buffer_;
    uint32_t                 cur_start_idx_ = 0;
};

}  // namespace sc_api::core

#endif  // SC_API_ACTION_H_
