/**
 * @file
 * @brief Input event stream.
 *
 * The backend reports every button transition that it observes on a connected device. A consumer
 * that only polls the input variables misses a press and a release that occur between two reads.
 * This stream does not.
 *
 * @see examples/input_events.cpp for a complete program.
 */

#ifndef SC_API_INPUT_EVENTS_H_
#define SC_API_INPUT_EVENTS_H_

#include <cstdint>
#include <memory>

#include "sc-api/protocol/input_events.h"
#include "sc-api/session.h"

namespace sc_api {

/**
 * @brief Reads the input events of a session.
 *
 * The backend writes every event into one ring buffer in shared memory. Each reader has its own
 * cursor, and no reader delays the backend. A reader that does not read for a long time falls
 * behind the ring and loses events. @ref read then reports how many events it skipped.
 *
 * @ref sc_api::InputEvent "InputEvent" and @ref sc_api::InputEventType "InputEventType" come
 * with this header.
 *
 * @note One reader per thread. The class is not thread-safe.
 *
 * @note A reader delivers no history. @ref open sets the cursor after the newest event, so events
 *       from before the call are not delivered and are not counted as lost.
 *
 * Events tell what changed. They do not tell the state of an input that never changes. Read the
 * state of every input from the `digital_inputs0` to `digital_inputs3` variables of the device
 * (`ww.digital_inputs0` to `ww.digital_inputs3` for a wireless wheel). These four words are the
 * baseline. Bit `N % 32` of word `N / 32` is the input whose
 * @ref sc_api::device_info::Input::event_id "Input::event_id" is N. For a wheel behind a wireless
 * hub, these variables belong to the hub, and
 * @ref sc_api::device_info::Input::variable "Input::variable" gives the device that holds them.
 *
 * Follow these rules to keep your own state correct:
 *
 * - **Read the baseline** after @ref open, when a device appears in device info, and after every
 *   read that reports a loss. A call that reports a loss returns no events, so read the baseline
 *   after that call and before the next read call. Call
 *   @ref sc_api::Session::getVariables "Session::getVariables" each time, because an older
 *   snapshot holds no variables of a device that arrived after it. The session refreshes the
 *   definitions on its own schedule and then sends `VariableDefinitionsChanged`. A device that
 *   arrived before its variables gets its baseline when that event arrives.
 * - **Check `lost` before `count`.** A call that reports a loss always returns no events.
 * - **Resolve the events of a read call against a device info snapshot that you take after that
 *   call.** The backend lists a device and its inputs before it sends their events, and a fresh
 *   snapshot drops the old name of an input whose `event_id` another input took over. Events
 *   written before that change resolve to the new input. If an event does not resolve, refresh the
 *   snapshot once more and try again: @ref sc_api::Session::getDeviceInfo "Session::getDeviceInfo"
 *   waits about a millisecond for a publication in progress and returns the previous snapshot on
 *   timeout, so an old name can survive one read call. Ignore the event only when it still does
 *   not resolve. Three limits apply to `input_id`. The stream reports all 128 button bits of a
 *   wheel, so an id can match no input. If more than one input uses one bit, only one of them has an
 *   `event_id`, and which one is not defined. A wheel that supplies no input variables has no
 *   `event_id` at all, and its events are ignored.
 * - **An event starts an action. A baseline never starts a press action.** A baseline can already
 *   contain a transition that a later event repeats, so a press action that a baseline starts
 *   occurs twice.
 * - **A baseline does start a release action.** An input whose press action you started and that
 *   the baseline shows clear was released while the events were lost.
 * - **A press event for an input whose press action you already started means that its release
 *   was lost.** The baseline you read after the loss already held the new press. Start the release
 *   action first, then the press action.
 * - **Start a release action only for an input whose press action you started.** Keep the two
 *   records apart: the state of the input, and what you have started. A release for an input that
 *   you never pressed is a correction, not an action of the user.
 * - **Release the inputs of a device** when the device leaves device info, release an input when
 *   it leaves the input list of its device or when another input takes its `event_id`, and
 *   release the inputs of every device when
 *   @ref isValid becomes false. Discard the events that you did not apply
 *   before you release. The backend also sends a release for each pressed input when a device
 *   disconnects. The rule above makes the second release a no operation.
 */
class InputEventReader {
public:
    /** Result of one @ref read call. */
    using ReadResult = InputEventReadResult;

    /**
     * @brief Construct a reader for a session.
     *
     * Construction does not open the ring. Call @ref open.
     *
     * @param session Active API session.
     */
    explicit InputEventReader(std::shared_ptr<sc_api::Session> session);

    ~InputEventReader();

    InputEventReader(const InputEventReader&)            = delete;
    InputEventReader& operator=(const InputEventReader&) = delete;

    InputEventReader(InputEventReader&& other) noexcept;
    InputEventReader& operator=(InputEventReader&& other) noexcept;

    /**
     * @brief Open the ring and set the cursor after the newest event.
     *
     * Read the baseline of every device after a successful call.
     *
     * This version of the API needs a backend that supplies the event ring. A backend without it
     * gives no session at all, so the call fails only when the ring header does not pass its
     * checks, or when the session is already lost.
     *
     * A call on a reader that is already open changes nothing and returns true. Construct a new
     * reader to start again from the newest event.
     *
     * @return true if the ring is open and @ref read can deliver events.
     */
    bool open();

    /**
     * @brief Whether this reader can deliver events.
     *
     * False before a successful @ref open, and false after the session is lost. A lost session
     * never recovers. Open a new session and construct a new reader.
     */
    bool isValid() const;

    /**
     * @brief Copy the events that this reader has not read yet.
     *
     * The call does not block. It returns the events in the order that the backend wrote them,
     * oldest first.
     *
     * Each returned event is newer than each skipped event. The call that reports a loss returns no
     * events. A call can return the events it copied before it met an overrun and leave the loss for a
     * later call to report.
     *
     * Events of a type that this version of the API does not know are dropped. A dropped event
     * increases neither `count` nor `lost`. A call can therefore return no events while the ring
     * holds more. Poll again. A reader that is not valid also returns no events and no loss, so
     * check @ref isValid when the stream stays silent.
     *
     * @param out Array that receives the events. It must hold `max_events` events.
     * @param max_events Size of `out` in events.
     * @return `count` events written to `out`, and `lost` records that this reader skipped because
     *         it fell a full ring behind the backend. Skipped records are never delivered and are
     *         counted whatever their type. A record that this call dropped because it does not
     *         know the type is not one of them. The count of lost records stops at its maximum
     *         value, so use it as an indication, not as an exact number.
     */
    [[nodiscard]] ReadResult read(InputEvent* out, uint32_t max_events);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sc_api

#endif  // SC_API_INPUT_EVENTS_H_
