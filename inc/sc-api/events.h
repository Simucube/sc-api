/**
 * @file
 * @brief Event types that the API delivers through an EventQueue.
 *
 * sc_api::Event is a std::variant of all event types. Create the queue with
 * Api::createEventQueue or ApiCore::createEventQueue.
 *
 * Two ways to read an event are available. The getIf* functions return a typed pointer or
 * nullptr. The has* functions return a bool. The getIf* functions also have an overload that
 * accepts the std::optional that the queue tryPop functions return. The has* functions accept
 * only a const Event&.
 */

#ifndef SC_API_EVENTS_H_
#define SC_API_EVENTS_H_
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

#include "session_fwd.h"

namespace sc_api {

class Session;

namespace event {

struct SessionEvent {
    /** Currently active Session */
    std::shared_ptr<Session> session;
};

/** State of the session has changed or it has been registered again with different control flags */
struct SessionStateChanged : SessionEvent {
    /** Current state of the session */
    SessionState state     = SessionState::invalid;

    /** Controller id of this API user. 0 if session hasn't yet been registered to control */
    uint16_t controller_id = 0;

    /** Combination of Session::ControlFlag that signify types of control that are allowed within the current session
     */
    uint32_t control_flags = 0;
};

/** Device information data has changed */
struct DeviceInfoChanged : SessionEvent {};

/** Variable definitions have changed
 *
 * During a session, only new variables can be added. Previous definitions are never modified nor removed
 */
struct VariableDefinitionsChanged : SessionEvent {};

/** Telemetry definitions have changed*/
struct TelemetryDefinitionsChanged : SessionEvent {};

/** Simulator data has been updated */
struct SimDataChanged : SessionEvent {};

}  // namespace event

using NoEvent = std::monostate;
using Event =
    std::variant<NoEvent, event::SessionStateChanged, event::DeviceInfoChanged, event::VariableDefinitionsChanged,
                 event::TelemetryDefinitionsChanged, event::SimDataChanged>;

namespace event {

/** Return pointer to SessionStateChanged event if given event contains that */
const SessionStateChanged* getIfSessionStateChanged(const Event* e);

/** Return pointer to VariableDefinitionsChanged event if given event contains that */
const VariableDefinitionsChanged* getIfVariableDefinitionsChanged(const Event* e);

/** Return pointer to DeviceInfoChanged event if given event contains that */
const DeviceInfoChanged* getIfDeviceInfoChanged(const Event* e);

/** Return pointer to TelemetryDefinitionsChanged event if given event contains that */
const TelemetryDefinitionsChanged* getIfTelemetryDefinitionsChanged(const Event* e);

/** Return pointer to SimDataChanged event if given event contains that */
const SimDataChanged* getIfSimDataChanged(const Event* e);

/** Return true, if given event is SessionStateChanged */
bool hasSessionStateChanged(const Event& e);

/** Return true, if given event is DeviceInfoChanged */
bool hasDeviceInfoChanged(const Event& e);

/** Return true, if given event is TelemetryDefinitionsChanged */
bool hasTelemetryDefinitionsChanged(const Event& e);

/** Return true, if given event is VariableDefinitionsChanged */
bool hasVariableDefinitionsChanged(const Event& e);

/** Return true, if given event is SimDataChanged */
bool hasSimDataChanged(const Event& e);

/** Return pointer to SessionStateChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const SessionStateChanged* getIfSessionStateChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfSessionStateChanged(&e->value());
}

/** Return pointer to VariableDefinitionsChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const VariableDefinitionsChanged* getIfVariableDefinitionsChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfVariableDefinitionsChanged(&e->value());
}

/** Return pointer to DeviceInfoChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const DeviceInfoChanged* getIfDeviceInfoChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfDeviceInfoChanged(&e->value());
}

/** Return pointer to TelemetryDefinitionsChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const TelemetryDefinitionsChanged* getIfTelemetryDefinitionsChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfTelemetryDefinitionsChanged(&e->value());
}

/** Return pointer to SimDataChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const SimDataChanged* getIfSimDataChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfSimDataChanged(&e->value());
}

}  // namespace event

}  // namespace sc_api

#endif  // SC_API_EVENTS_H_
