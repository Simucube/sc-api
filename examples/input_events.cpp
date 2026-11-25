/**
 * Input event example
 *
 * Prints an action for each button press, for each release of a button whose press it printed, and for each
 * touch contact point that goes down, moves, goes up or is tapped. It also shows recovery after lost events and when a
 * device leaves.
 */

#include <sc-api/api.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/input_events.h>
#include <sc-api/variable_references.h>
#include <sc-api/variables.h>

#include <array>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

using namespace sc_api;

namespace {

std::atomic<bool> g_running{true};

void onSignal(int) { g_running = false; }

/** The stream covers the four digital input words of a wheel, so 128 buttons. */
constexpr uint16_t k_word_count = 4;
constexpr uint16_t k_bit_count  = k_word_count * 32;

using InputWords                = std::array<uint32_t, k_word_count>;

bool testBit(const InputWords& words, uint16_t bit) {
    return bit < k_bit_count && ((words[bit / 32] >> (bit % 32)) & 1u) != 0u;
}

void setBit(InputWords& words, uint16_t bit, bool value) {
    if (bit >= k_bit_count) {
        return;
    }
    const uint32_t mask = 1u << (bit % 32);
    if (value) {
        words[bit / 32] |= mask;
    } else {
        words[bit / 32] &= ~mask;
    }
}

/** One device that reports input events. */
struct DeviceState {
    std::string name;

    /** Input name of each event id that this device uses. */
    std::map<uint16_t, std::string> inputs;

    /** The digital input words of the device in shared memory. A word can be absent. */
    const uint32_t* baseline_words[k_word_count] = {};

    /** The buttons whose press action this program started. A release action needs a press action
     *  of this program first. The button state itself is in the baseline words. */
    InputWords armed                             = {};

    /** The last touch event of each panel, by input_id. */
    std::map<uint16_t, TouchEventPayload> last_touch;
};

using DeviceMap = std::map<uint16_t, DeviceState>;

std::string hidButtonText(const InputEvent& event) {
    return event.hid_index == k_input_event_no_hid_index ? "no HID button"
                                                         : "HID button " + std::to_string(event.hid_index);
}

void printAction(uint16_t device_session_id, const DeviceState& device, uint16_t event_id, std::string_view action) {
    std::cout << device.name << " (" << device_session_id << "): " << device.inputs.at(event_id) << ' ' << action
              << std::endl;
}

/** Does the variable definitions snapshot hold the digital input words of the device? The session refreshes the
 *  snapshot on its own schedule, so the words can arrive after the device info. Until then, the baseline reads 0. */
bool hasBaseline(const DeviceState& device) { return device.baseline_words[0] != nullptr; }

/** Current value of the digital input words. A word that the snapshot does not hold reads zero. */
InputWords readBaseline(const DeviceState& device) {
    InputWords words = {};
    for (uint16_t i = 0; i < k_word_count; ++i) {
        if (device.baseline_words[i] != nullptr) {
            words[i] = *device.baseline_words[i];
        }
    }
    return words;
}

/** Finds the digital input words of this device. A wireless wheel has its own device session id, but its baseline
 *  is in the `ww.digital_inputs0` to `ww.digital_inputs3` variables of the SC-link Hub. Input::variable names the
 *  device that holds the words, so an input whose variable belongs to another device is a wireless wheel input. */
void resolveBaselineWords(VariableDefinitions& variables, const device_info::VariableRef& input_variable,
                          uint16_t device_session_id, DeviceState& device) {
    const bool wireless = input_variable.device_session_id.id != device_session_id;
    for (uint16_t i = 0; i < k_word_count; ++i) {
        const auto& reference =
            wireless ? variable::wirelesswheel::digital_inputs[i] : variable::wheel::digital_inputs[i];
        device.baseline_words[i] = variables.findValuePointer(reference, input_variable.device_session_id);
    }
}

void releaseArmedButtons(uint16_t device_session_id, DeviceState& device, std::string_view reason) {
    for (uint16_t event_id = 0; event_id < k_bit_count; ++event_id) {
        if (testBit(device.armed, event_id)) {
            setBit(device.armed, event_id, false);
            printAction(device_session_id, device, event_id, reason);
        }
    }
}

void printTouch(uint16_t device_session_id, const DeviceState& device, unsigned panel, unsigned point,
                std::string_view state) {
    std::cout << device.name << " (" << device_session_id << "): panel " << panel << " point " << point << ' ' << state
              << std::endl;
}

std::string positionText(const TouchEventPayload& touch, unsigned point) {
    return std::to_string(touch.x[point]) + ',' + std::to_string(touch.y[point]);
}

bool isPointDown(const TouchEventPayload& touch, unsigned point) { return ((touch.points_down >> point) & 1u) != 0u; }

bool isPointLifted(const TouchEventPayload& touch, unsigned point) { return ((touch.points_up >> point) & 1u) != 0u; }

bool isPointTapped(const TouchEventPayload& touch, unsigned point) {
    return ((touch.points_up >> (point + k_touch_event_tap_shift)) & 1u) != 0u;
}

void printCancelledPoints(uint16_t device_session_id, const DeviceState& device, unsigned panel,
                          const TouchEventPayload& touch, std::string_view reason) {
    for (unsigned point = 0; point < k_touch_event_max_points; ++point) {
        if (isPointDown(touch, point)) {
            printTouch(device_session_id, device, panel, point, reason);
        }
    }
}

void cancelTouch(uint16_t device_session_id, DeviceState& device, std::string_view reason) {
    for (const auto& [panel, touch] : device.last_touch) {
        printCancelledPoints(device_session_id, device, panel, touch, reason);
    }
    device.last_touch.clear();
}

void endAllActions(uint16_t device_session_id, DeviceState& device, const std::string& cause) {
    releaseArmedButtons(device_session_id, device, "released (" + cause + ')');
    cancelTouch(device_session_id, device, "cancelled (" + cause + ')');
}

/** Starts a release action for every armed button that the baseline shows clear, because that button was released
 *  while the events were lost. A baseline never starts a press action, because a later event can repeat a transition
 *  that the baseline already contains. */
void applyBaseline(uint16_t device_session_id, DeviceState& device) {
    const InputWords baseline = readBaseline(device);
    for (uint16_t event_id = 0; event_id < k_bit_count; ++event_id) {
        if (testBit(device.armed, event_id) && !testBit(baseline, event_id)) {
            setBit(device.armed, event_id, false);
            printAction(device_session_id, device, event_id, "released (missed)");
        }
    }
}

DeviceState makeDeviceState(const device_info::DeviceInfo& info_entry, VariableDefinitions& variables) {
    const uint16_t device_session_id = info_entry.getSessionId().id;

    DeviceState device;
    device.name = std::string(info_entry.getUid());
    for (const device_info::Input& input : info_entry.getInputs()) {
        if (!input.event_id) {
            continue;
        }
        if (device.inputs.empty()) {
            resolveBaselineWords(variables, input.variable, device_session_id, device);
        }
        device.inputs[*input.event_id] = std::string(input.id);
    }
    return device;
}

/** Releases every armed button whose input left the input list or gave its event id to another
 *  input. The pressed input is gone even though the id stays, so the release event of that button
 *  no longer resolves to it. The action is printed with the old input name. */
void releaseRemovedInputs(uint16_t device_session_id, const DeviceState& previous, DeviceState& current) {
    for (uint16_t event_id = 0; event_id < k_bit_count; ++event_id) {
        if (!testBit(current.armed, event_id)) {
            continue;
        }
        auto now = current.inputs.find(event_id);
        if (now == current.inputs.end() || now->second != previous.inputs.at(event_id)) {
            setBit(current.armed, event_id, false);
            printAction(device_session_id, previous, event_id, "released (input removed)");
        }
    }
}

/** Rebuilds the device table from device info. A device that left releases its armed buttons and cancels its touch
 *  contact points. A device that stays keeps them, except the buttons whose input was removed. */
void refreshDevices(const std::shared_ptr<Session>& session, DeviceMap& devices) {
    auto info = session->getDeviceInfo();
    if (!info) {
        return;
    }
    // A snapshot from before a device arrived holds no variables of that device, so read the
    // definitions on every call and not once at start.
    VariableDefinitions variables = session->getVariables();

    // Every listed device stays in the table: a device without button inputs can still report touch.
    DeviceMap next;
    for (const device_info::DeviceInfo& info_entry : *info) {
        const uint16_t device_session_id = info_entry.getSessionId().id;
        DeviceState    device            = makeDeviceState(info_entry, variables);

        auto previous_entry              = devices.find(device_session_id);
        if (previous_entry != devices.end()) {
            const DeviceState& previous = previous_entry->second;
            device.armed                = previous.armed;
            device.last_touch           = previous.last_touch;
            releaseRemovedInputs(device_session_id, previous, device);
            if (!hasBaseline(previous) && hasBaseline(device)) {
                applyBaseline(device_session_id, device);  // arrived before its variables
            }
        }
        next.emplace(device_session_id, std::move(device));
    }

    for (auto& entry : devices) {
        if (next.count(entry.first) == 0) {
            endAllActions(entry.first, entry.second, "device gone");
        }
    }
    devices = std::move(next);
}

bool resolves(const DeviceMap& devices, const InputEvent& event) {
    auto device_entry = devices.find(event.device_session_id);
    if (device_entry == devices.end()) {
        return false;
    }
    // A touch event names the device and a panel, not an input.
    return event.type == InputEventType::touch || device_entry->second.inputs.count(event.input_id) != 0;
}

/** Prints each contact point that went down, moved, went up or was tapped since the panel's previous event. */
void applyTouch(uint16_t device_session_id, DeviceState& device, uint16_t panel, const TouchEventPayload& touch) {
    TouchEventPayload& previous = device.last_touch[panel];  // a missing panel starts released
    if ((touch.flags & k_touch_event_cancelled) != 0) {
        printCancelledPoints(device_session_id, device, panel, previous, "cancelled (device gone)");
        device.last_touch.erase(panel);
        return;
    }
    const bool suppressed = (touch.flags & k_touch_event_mouse_suppressed) != 0;
    for (unsigned point = 0; point < k_touch_event_max_points; ++point) {
        const bool was_down = isPointDown(previous, point);
        const bool is_down  = isPointDown(touch, point);
        const bool moved    = touch.x[point] != previous.x[point] || touch.y[point] != previous.y[point];
        if (is_down && !was_down) {
            printTouch(device_session_id, device, panel, point,
                       "down at " + positionText(touch, point) + (suppressed ? ", kept from the mouse" : ""));
        } else if (is_down && was_down && moved) {
            printTouch(device_session_id, device, panel, point, "moved to " + positionText(touch, point));
        } else if (isPointTapped(touch, point)) {
            printTouch(device_session_id, device, panel, point, "tapped at " + positionText(touch, point));
        } else if (isPointLifted(touch, point) && was_down) {
            printTouch(device_session_id, device, panel, point, "up at " + positionText(touch, point));
        }
    }
    previous = touch;
}

void applyEvent(DeviceMap& devices, const InputEvent& event) {
    auto device_entry = devices.find(event.device_session_id);
    if (device_entry == devices.end()) {
        return;
    }
    DeviceState& device = device_entry->second;

    // The meaning of input_id depends on the type, so the type decides the path first.
    if (const TouchEventPayload* touch = getEventPayload<TouchEventPayload>(&event)) {
        applyTouch(event.device_session_id, device, event.input_id, *touch);
        if (std::optional<TouchPosition> tap = getSingleFingerTap(event)) {
            std::cout << device.name << " (" << event.device_session_id << "): panel " << event.input_id
                      << " single-finger tap at " << tap->x << ',' << tap->y << std::endl;
        }
        return;
    }
    if (event.type != InputEventType::button_pressed && event.type != InputEventType::button_released) {
        return;
    }
    const bool pressed = event.type == InputEventType::button_pressed;

    // An event can carry an input_id that no input of this device uses.
    if (device.inputs.count(event.input_id) == 0) {
        return;
    }

    if (pressed) {
        // A press for an input that is still armed means its release was lost, and the recovery
        // baseline already held the new press. Close the first action before the second starts.
        if (testBit(device.armed, event.input_id)) {
            printAction(event.device_session_id, device, event.input_id, "released (missed)");
        }
        setBit(device.armed, event.input_id, true);
        printAction(event.device_session_id, device, event.input_id, "pressed, " + hidButtonText(event));
    } else if (testBit(device.armed, event.input_id)) {
        setBit(device.armed, event.input_id, false);
        printAction(event.device_session_id, device, event.input_id, "released, " + hidButtonText(event));
    }
}

}  // namespace

int main() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    Api                              api;
    std::unique_ptr<Api::EventQueue> event_queue = api.createEventQueue();

    constexpr uint32_t k_batch_size              = 64;

    std::unique_ptr<InputEventReader> reader;
    std::shared_ptr<Session>          reader_session;  // device info must come from the session of the reader
    DeviceMap                         devices;
    InputEvent                        events[k_batch_size];
    bool                              device_info_changed = false;

    while (g_running) {
        while (auto event = event_queue->tryPop()) {
            if (event::getIfDeviceInfoChanged(&event) || event::getIfVariableDefinitionsChanged(&event)) {
                device_info_changed = true;
            }
        }

        if (reader && !reader->isValid()) {
            // The session is lost and nothing more arrives. Every event of the last read was
            // already applied, so every device can end its actions.
            reader.reset();
            reader_session.reset();
            for (auto& entry : devices) {
                endAllActions(entry.first, entry.second, "session lost");
            }
            devices.clear();
            std::cout << "Session lost" << std::endl;
        }

        if (!reader) {
            auto session = api.getSession();
            if (session && session->getState() != SessionState::session_lost) {
                auto opening = std::make_unique<InputEventReader>(session);
                if (opening->open()) {
                    reader         = std::move(opening);
                    reader_session = session;
                    std::cout << "Reading input events. Press a wheel button or touch the screen." << std::endl;
                }
            }
            if (!reader) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }
            // The reader delivers no history, so the table starts from the current device info.
            device_info_changed = true;
        }

        if (device_info_changed) {
            device_info_changed = false;
            refreshDevices(reader_session, devices);
        }

        const InputEventReader::ReadResult result = reader->read(events, k_batch_size);

        // A call that reports a loss returns no events. The next call gives the events after the resync point. The
        // baseline can already contain some of them, which is harmless because a baseline starts no press action.
        // A panel has no baseline: it counts as released until its next event.
        if (result.lost != 0) {
            std::cout << "Lost " << result.lost << " records" << std::endl;
            refreshDevices(reader_session, devices);
            for (auto& entry : devices) {
                applyBaseline(entry.first, entry.second);
                cancelTouch(entry.first, entry.second, "cancelled (missed)");
            }
        }

        // Resolve this batch with device info taken after the read call. The refresh also drops
        // the old name of an input whose event id another input took over.
        if (result.count != 0) {
            refreshDevices(reader_session, devices);
        }

        bool refreshed = false;
        for (uint32_t i = 0; i < result.count; ++i) {
            // An event that does not resolve gets one more refresh before it is ignored.
            if (!refreshed && !resolves(devices, events[i])) {
                refreshed = true;
                refreshDevices(reader_session, devices);
            }
            applyEvent(devices, events[i]);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    for (auto& entry : devices) {
        endAllActions(entry.first, entry.second, "exit");
    }
    return 0;
}
