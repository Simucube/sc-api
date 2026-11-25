/**
 * @file
 * @brief ApiCore class that opens sessions without a background thread.
 *
 * Use this class when the application must control when session state changes and when
 * communication runs. The caller is responsible for the Session::runUntilStateChanges or
 * Session::poll calls.
 *
 * @see Api for the variant that does this in a background thread.
 */

#ifndef SC_API_APICORE_H_
#define SC_API_APICORE_H_
#include <memory>
#include <optional>

#include "events.h"
#include "session.h"
#include "util/event_queue.h"

namespace sc_api {

/** API main handle that initializes common data
 *
 * Must be alive whole duration that any session is open.
 * Can be used to attempt opening a session.
 *
 * Usually it is better to use Api class instead as that handles creation of the session and updating its state in
 * background thread. If ApiCore is used to open Session, it is the caller responsibility to call
 * Session::runUntilStateChanges or Session::poll to handle communications.
 */
class ApiCore {
    friend class Session;

    class Impl;

public:
    using EventQueue = util::EventQueue<sc_api::Event>;

    ApiCore();
    ~ApiCore();

    /** Try to initialize and connect to the Simucube API
     *
     * This initializes resources and registers itself to the backend. Most of the time this
     * should be fairly immediate (<50ms), but may block up-to 500ms, if backend is still initializing
     * when this function is called.
     *
     * @param session_handle_out Handle to the opened session
     *
     * @return ResultCode::ok, if initialization and connecting succeeded
     *         ResultCode::error_invalid_session_state, if a session is already open
     *         ResultCode::error_cannot_connect, if the API backend isn't running and no data is available
     *         ResultCode::error_busy, if the backend changed the session during the attempt. Try again.
     *         ResultCode::error_incompatible, if the backend isn't compatible with this API implementation
     *         ResultCode::error_timeout, if the backend did not give valid session data within 500ms
     *         ResultCode::error_protocol, if the session data from the backend cannot be parsed
     */
    ResultCode openSession(std::shared_ptr<Session>& session_handle_out);

    /** @brief Get currently open session.
     *  @return Currently active session, returns nullptr if there isn't open session active */
    std::shared_ptr<Session> getOpenSession() const;

    /** Get the version of the running Simucube Tuner
     *
     * Needs no open session. A Tuner older than k_min_backend_version can still open a session, but
     * features of this API version can be missing. Check for empty before you compare the result with
     * k_min_backend_version: an empty std::optional compares less than every version. Returns at once
     * and is safe to call from any thread, also while openSession runs. With an open session,
     * Session::getBackendVersion gives the version of the Tuner that opened it.
     *
     * @return The version of the running Tuner; 0.0.0 if Tuner does not report its version.
     *         Empty if no version is available now: Tuner does not run, it is starting or
     *         stopping, or this API cannot read its version. The value is what Tuner reports,
     *         not proof that it still runs.
     */
    static std::optional<BackendVersion> getBackendVersion();

    /** Create an EventQueue that receives session and data events
     *
     * Every queue receives its own copy of each event.
     *
     * @return Unique pointer to the created EventQueue
     */
    std::unique_ptr<EventQueue> createEventQueue();

private:
    std::unique_ptr<Impl> p_;

    std::shared_ptr<Session> constructSession(std::unique_ptr<Session::Internal> shm_handles, uint32_t session_id);
};

}  // namespace sc_api

#endif  // SC_API_APICORE_H_
