#pragma once

#include <sc-api/core/session_fwd.h>

inline const char* session_state_str(sc_api::core::SessionState s) {
    switch (s) {
        case sc_api::core::SessionState::invalid:
            return "invalid";
        case sc_api::core::SessionState::connected_monitor:
            return "connected_monitor";
        case sc_api::core::SessionState::connected_control:
            return "connected_control";
        case sc_api::core::SessionState::session_lost:
            return "session_lost";
        default:
            return "unknown";
    }
}
