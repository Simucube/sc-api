#ifndef SC_API_INTERNAL_SESSION_ALIVE_H_
#define SC_API_INTERNAL_SESSION_ALIVE_H_

#include <memory>

#include "sc-api/session.h"

namespace sc_api::detail {

/** @brief True while the session is connected. A shared memory block stays mapped after the session is lost, and
 *  the backend can still write it, so the session state is what tells a reader or a streamer that it is gone. */
inline bool isSessionAlive(const std::shared_ptr<Session>& session) {
    if (!session) {
        return false;
    }
    const SessionState state = session->getState();
    return state == SessionState::connected_monitor || state == SessionState::connected_control;
}

}  // namespace sc_api::detail

#endif  // SC_API_INTERNAL_SESSION_ALIVE_H_
