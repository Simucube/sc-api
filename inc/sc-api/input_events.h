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
 * An event reports a change of one input. To know the state of every input, read the baseline:
 * the `digital_inputs0` to `digital_inputs3` variables that
 * @ref sc_api::device_info::Input::event_id "Input::event_id" describes.
 * @ref sc_api::device_info::DeviceInfo::getInputByEventId "DeviceInfo::getInputByEventId" finds
 * the input of an event. A release event can also mean that the device became unavailable. The
 * `hid_index` of an event is a snapshot from the time of the event and needs no lookup. A remap
 * between a press and its release changes the `hid_index` of the release, so key press actions
 * by `input_id`.
 *
 * A reader that does not read for a long time loses events. @ref read then reports how many and
 * returns no events. Read `lost` before `count`.
 *
 * @note Do not use one reader from two threads at the same time.
 * @note A reader delivers no history. @ref open starts after the newest event.
 *
 * Rules for every application:
 *
 * 1. **Read the baseline** after @ref open, when a device appears in device info, and after a
 *    read that reports a loss, before the next read. Take a new
 *    @ref sc_api::Session::getVariables "Session::getVariables" snapshot each time. If the
 *    variables of a new device are not in it yet, read its baseline when
 *    `VariableDefinitionsChanged` arrives.
 * 2. **Resolve events against device info that you take after the read call.** If an event does
 *    not resolve, refresh device info once more. Ignore the event if it still does not resolve.
 *    An event carries no device info revision. If an `event_id` moves to another input, an older
 *    event can resolve to the new input.
 *
 * Rules for an application that starts press and release actions:
 *
 * 3. **Only an event starts a press action.** A baseline can hold a transition that a later event
 *    repeats.
 * 4. **End the press action of every input that the baseline shows released.**
 * 5. **A press event for an input whose press action is active ends that action first.**
 * 6. **Track the actions that you started apart from the input state.** End only an action that
 *    you started.
 * 7. **End all actions of a device when it leaves device info.** End the action of an input when
 *    the input leaves the device or another input takes its `event_id`. Do not apply pending
 *    events to a removed input. When @ref isValid turns false, discard all pending events, then end
 *    all remaining actions.
 *
 * `examples/input_events.cpp` shows action handling and recovery.
 */
class InputEventReader {
public:
    /** Result of one @ref read call. */
    using ReadResult = InputEventReadResult;

    /**
     * @brief Construct a reader for a session.
     *
     * Construction does not open the stream. Call @ref open.
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
     * @brief Open the stream and start after the newest event.
     *
     * Read the baseline of every device after a successful call. A call on a reader that is
     * already open changes nothing and returns true. Construct a new reader to start again from
     * the newest event.
     *
     * @return true if @ref read can deliver events. False when the session is lost or the stream
     *         is not available.
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
     * The call does not block. It returns the events oldest first. A call that reports a loss
     * returns no events, and each event of a later call is newer than each lost event.
     *
     * Events of a type that this version of the API does not know are filtered out. They increase
     * neither `count` nor `lost`. A call can therefore return no events while more wait. Poll
     * again. A reader that is not valid returns no events and no loss, so check @ref isValid when
     * the stream stays silent.
     *
     * @param out Array that receives the events. It must hold `max_events` events.
     * @param max_events Size of `out` in events.
     * @return `count` events written to `out`, and `lost` events that this reader skipped because
     *         it fell too far behind. Lost events are never delivered and are counted whatever
     *         their type. The lost count stops at its maximum value, so use it as an indication,
     *         not as an exact number.
     */
    [[nodiscard]] ReadResult read(InputEvent* out, uint32_t max_events);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace sc_api

#endif  // SC_API_INPUT_EVENTS_H_
