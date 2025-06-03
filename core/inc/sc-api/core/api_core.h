/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_APICORE_H_
#define SC_API_CORE_APICORE_H_
#include <memory>

#include "events.h"
#include "session.h"
#include "util/event_queue.h"

namespace sc_api::core {

/** API main handle that initializes common data
 *
 * Must be alive whole duration that any session is open.
 * Can be used to attempt opening a session.
 *
 * Usually it is better to use Api class instead as that handles creation of the session and updating its state in
 * background thread. If ApiRaw is used to open Session, it is the caller responsibility to call Session::run() or
 * other updated functions that handle communications.
 */
class ApiCore {
    friend class Session;

    class Impl;

public:
    using EventQueue = util::EventQueue<sc_api::core::Event>;

    ApiCore();
    ~ApiCore();

    /** Try to initialize and connect to the Simucube API
     *
     * This initializes resources and registers itself to the backend. Most of the time this
     * should be fairly immediate (<50ms), but may block up-to 500ms, if backend is still initializing
     * when this function is called.
     *
     * This must be called before using any other function in this API.
     *
     * @param session_handle_out Handle to the opened session
     *
     * @return Result::ok, if initialization and connecting succeeded
     *         Result::error_argument, if id_name is more than 16 characters or display_name_utf8 is more than 64 bytes
     *                                 characters.
     *         Result::error_state, if called when session state isn't State::disconnected
     *         Result::error_not_available, if API backend isn't running and no data is available
     *         Result::error_incompatible, if the backend isn't compatible with this API implementation
     */
    ResultCode openSession(std::shared_ptr<Session>& session_handle_out);

    /** @brief Get currently open session.
     *  @return Currently active session, returns nullptr if there isn't open session active */
    std::shared_ptr<Session> getOpenSession() const;

    /** */
    std::unique_ptr<EventQueue> createEventQueue();
private:
    std::unique_ptr<Impl> p_;

    std::shared_ptr<Session> constructSession(std::unique_ptr<Session::Internal> shm_handles, uint32_t session_id);
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_APICORE_H_
