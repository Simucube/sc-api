#pragma once

#include <sc-api/session_fwd.h>

inline const char* session_state_str(sc_api::SessionState s) {
    switch (s) {
        case sc_api::SessionState::invalid:
            return "invalid";
        case sc_api::SessionState::connected_monitor:
            return "connected_monitor";
        case sc_api::SessionState::connected_control:
            return "connected_control";
        case sc_api::SessionState::session_lost:
            return "session_lost";
        default:
            return "unknown";
    }
}
