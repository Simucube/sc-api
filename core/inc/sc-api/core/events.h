/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_EVENTS_H_
#define SC_API_CORE_EVENTS_H_
#include <cstdint>
#include <memory>
#include <variant>

#include "session_fwd.h"

namespace sc_api::core {

class Session;

namespace session_event {

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
struct DeviceInfoChanged : SessionEvent {
};

/** Variable definitions have changed
 *
 * During a session, only new variables can be added. Previous definitions are never modified nor removed
 */
struct VariableDefinitionsChanged : SessionEvent {};

/** Telemetry definitions have changed*/
struct TelemetryDefinitionsChanged : SessionEvent {};

/** Simulator data has been updated */
struct SimDataChanged : SessionEvent {};

}  // namespace session_event

using NoEvent = std::monostate;
using Event   = std::variant<NoEvent, session_event::SessionStateChanged, session_event::DeviceInfoChanged,
                             session_event::VariableDefinitionsChanged, session_event::TelemetryDefinitionsChanged,
                             session_event::SimDataChanged>;

}  // namespace sc_api::core

#endif  // SC_API_CORE_EVENTS_H_
