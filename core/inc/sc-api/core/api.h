/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_API_H_
#define SC_API_CORE_API_H_

#include <thread>

#include "api_core.h"
#include "telemetry.h"
#include "variables.h"

namespace sc_api::core {

/** Wrapper around ApiCore class that handles session management and providers in a background thread
 *
 * This avoids need to manually attempt reconnecting and calling Session::runUntilStateChanges.
 *
 * If more manual and synchronized handling of the session is required, then ApiCore class should
 * be used directly instead.
 *
 * Background thread runs as long as this object is alive and will call Session::runUntilStateChanges
 * for the active session to handle its state, command processing and other actions.
 */
class Api {
public:
    using EventQueue = ApiCore::EventQueue;

    /** Listener hook that is executed in the background thread of Api
     *
     * Prefer using EventQueue for most purposes instead of hooking to background thread as it allows more flexibility
     */
    class ListenerInterface {
    public:
        virtual ~ListenerInterface();

        /** Called from background thread when listener is added
         *
         * This is only called once and before any sessionStateChanged calls.
         *
         * @param api Pointer to Api which this listerner was added
         * @param active_session Currently active session, may be nullptr if no session is established
         */
        virtual void listenerAdded(Api* api, const std::shared_ptr<Session>& active_session)            = 0;

        /** Called from background thread when listener is removed.
         *
         * After this sessionStateChanged won't be called.
         * Can be called without matching listenerAdded if listener is removed before background thread has change to
         * process adding the listener
         *
         * @param api Pointer to Api which from this listerner was removed
         */
        virtual void listenerRemoved(Api* api)                                                          = 0;

        /** Called from background thread when session state changes
         *
         * Called twice for each established session: once when session is connected and then second time when it is
         * lost or Api is being destroyed.
         */
        virtual void sessionStateChanged(const std::shared_ptr<Session>& session, Session::State state) = 0;

        /** */
        virtual void controlFlagsChanged(const std::shared_ptr<Session>& session, uint32_t flags)       = 0;
    };

    Api();
    ~Api();

    /** Get ApiCore */
    ApiCore& getCore() { return api_; }

    /** Get session */
    std::shared_ptr<Session> getSession() { return api_.getOpenSession(); }

    void addListener(ListenerInterface* listener);
    void removeListener(ListenerInterface* listener);

    /**
     *
     *
     */
    std::unique_ptr<EventQueue> createEventQueue() { return api_.createEventQueue(); }

private:
    void threadFunc();

    std::mutex              m_;
    std::condition_variable cv_;
    bool                    running_ = false;

    std::shared_ptr<Session> active_session_;

    struct SyncState {
        std::mutex              m;
        std::condition_variable cv;
    } sync_state_;

    struct ListenerAction {
        enum Action { add, remove };
        ListenerInterface* listener;
        Action             type;
        bool*              sync_var = nullptr;
    };

    std::vector<ListenerAction> listener_action_queue_;

    ApiCore      api_;
    std::thread thread_;
};

/** Handler that automatically attempts to register this handler as controller when session is established
 *
 * Doesn't attempt to negotiate any encryption method and all API calls are unencrypted.
 */
class NoAuthControlEnabler : protected Api::ListenerInterface {
public:
    NoAuthControlEnabler(Api* api, uint32_t control_flags, std::string id_name, ApiUserInformation user_info);
    ~NoAuthControlEnabler();

protected:
    void listenerAdded(Api* api, const std::shared_ptr<Session>& active_session) override;
    void listenerRemoved(Api* api) override;
    void sessionStateChanged(const std::shared_ptr<Session>& session, Session::State state) override;
    void controlFlagsChanged(const std::shared_ptr<Session>& session, uint32_t flags) override;

private:
    Api*               api_;
    uint32_t           control_flags_;
    std::string        id_name_;
    ApiUserInformation user_info_;
};

class SecureControlEnabler : protected Api::ListenerInterface {
public:
    SecureControlEnabler(Api* api, uint32_t control_flags, std::string id_name, ApiUserInformation user_info,
                         std::vector<uint8_t> public_key, std::vector<uint8_t> private_key);
    ~SecureControlEnabler();

protected:
    void listenerAdded(Api* api, const std::shared_ptr<Session>& active_session) override;
    void listenerRemoved(Api* api) override;
    void sessionStateChanged(const std::shared_ptr<Session>& session, Session::State state) override;
    void controlFlagsChanged(const std::shared_ptr<Session>& session, uint32_t flags) override;

private:
    void setupSecureSession(const std::shared_ptr<Session>& session) const;

    Api*               api_;
    uint32_t           control_flags_;
    std::string        id_name_;
    ApiUserInformation user_info_;

    std::vector<uint8_t> public_key_;
    std::vector<uint8_t> private_key_;
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_API_H_
