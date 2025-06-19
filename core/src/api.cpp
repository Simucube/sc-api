#include "sc-api/core/api.h"

#include <cassert>
#include <thread>

#include "sc-api/core/session.h"
#include "security_impl.h"

namespace sc_api::core {

static constexpr auto k_disconnected_monitor_period = std::chrono::seconds(4);

Api::Api() : running_(true), thread_([this]() { threadFunc(); }) {}

Api::~Api() {
    {
        std::lock_guard lock(m_);
        running_ = false;

        if (active_session_) active_session_->stop();
    }
    cv_.notify_all();
    thread_.join();
}

void Api::addListener(ListenerInterface* listener) {
    std::unique_lock lock(m_);
    listener_action_queue_.push_back(ListenerAction{listener, ListenerAction::add});

    // Call stop to let worker thread to process this new listener
    if (active_session_) {
        active_session_->stop();
    } else {
        cv_.notify_all();
    }
}

void Api::removeListener(ListenerInterface* listener) {
    bool action_complete = false;
    {
        std::unique_lock lock(m_);
        listener_action_queue_.push_back({listener, ListenerAction::remove, &action_complete});

        if (active_session_) {
            active_session_->stop();
        } else {
            cv_.notify_all();
        }
    }

    std::unique_lock sync_lock(sync_state_.m);
    sync_state_.cv.wait(sync_lock, [&]() { return action_complete; });
}

void Api::threadFunc() {
    bool connected = false;

    std::shared_ptr<Session>        active_session;
    Session::PeriodicTimerHandle    refresh_timer_handle;
    std::vector<ListenerInterface*> listeners;
    std::size_t                     add_notify_index     = 0;
    uint32_t                        active_control_flags = 0;

    Session::State prev_session_state                    = Session::State::invalid;
    while (true) {
        if (connected) {
            Session::State session_state = active_session->runUntilStateChanges();

            if (session_state != prev_session_state) {
                prev_session_state = session_state;
                for (ListenerInterface* listener : listeners) {
                    listener->sessionStateChanged(active_session, session_state);
                }
            }
            if (session_state == Session::State::session_lost) {
                refresh_timer_handle.destroy();
                connected = false;
                active_session->close();

                // Wait a moment to avoid reopening closing session
                std::unique_lock lock(m_);
                active_session_ = nullptr;
                cv_.wait_for(lock, std::chrono::seconds(1), [&]() {
                    // Don't wait if running_ == false
                    return !running_;
                });
                active_session.reset();
                continue;
            }

            uint32_t control_flags = active_session->getControlFlags();
            if (control_flags != active_control_flags) {
                active_control_flags = control_flags;

                for (ListenerInterface* listener : listeners) {
                    listener->controlFlagsChanged(active_session, control_flags);
                }
            }

        } else {
            ResultCode result = api_.openSession(active_session);
            switch (result) {
                case ResultCode::ok: {
                    {
                        std::lock_guard lock(m_);
                        active_session_ = active_session;
                    }

                    active_control_flags = 0;
                    prev_session_state   = Session::State::connected_monitor;
                    connected            = true;

                    for (ListenerInterface* listener : listeners) {
                        listener->sessionStateChanged(active_session, Session::State::connected_monitor);
                    }

                    break;
                }
                case ResultCode::error_busy:
                case ResultCode::error_incompatible:
                case ResultCode::error_timeout:
                case ResultCode::error_protocol:
                case ResultCode::error_cannot_connect:
                    break;
                default:
                    assert(false && "Unexpected error when trying to open API");
                    break;
            }
        }

        std::unique_lock lock(m_);
        if (!connected) {
            cv_.wait_for(lock, k_disconnected_monitor_period, [&]() {
                // Don't wait if running_ == false
                return !running_;
            });
        }

        add_notify_index = listeners.size();
        std::vector<std::pair<ListenerInterface*, bool*>> removed_listeners;
        for (const ListenerAction& action : listener_action_queue_) {
            switch (action.type) {
                case ListenerAction::add:
                    listeners.push_back(action.listener);
                    break;
                case ListenerAction::remove: {
                    auto removed_it = std::remove(listeners.begin(), listeners.end(), action.listener);
                    if (removed_it != listeners.end()) {
                        auto removed_idx = std::size_t(removed_it - listeners.begin());
                        if (removed_idx < add_notify_index) {
                            add_notify_index--;
                        }
                        listeners.erase(removed_it);
                    }
                    removed_listeners.push_back({action.listener, action.sync_var});
                }
            }
        }
        listener_action_queue_.clear();

        bool closing = !running_;

        // Unlock main mutex before calling callbacks to avoid deadlocks if this callbacks want to add new listeners
        lock.unlock();

        if (!removed_listeners.empty()) {
            {
                std::unique_lock notify_lock(sync_state_.m);
                for (auto& listener_and_notify_bool : removed_listeners) {
                    listener_and_notify_bool.first->listenerRemoved(this);
                    *listener_and_notify_bool.second = true;
                }
            }
            sync_state_.cv.notify_all();
        }

        if (add_notify_index < listeners.size()) {
            for (auto i = add_notify_index; i < listeners.size(); ++i) {
                listeners[i]->listenerAdded(this, active_session);
            }
        }

        if (closing) break;
    }

    if (active_session) {
        {
            std::lock_guard lock(m_);
            active_session_ = nullptr;
        }

        for (ListenerInterface* listener : listeners) {
            listener->sessionStateChanged(active_session, Session::State::session_lost);
        }
        active_session->close();
    }

    for (ListenerInterface* listener : listeners) {
        listener->listenerRemoved(this);
    }
}

Api::ListenerInterface::~ListenerInterface() {}

NoAuthControlEnabler::NoAuthControlEnabler(Api* api, uint32_t control_flags, std::string id_name,
                                           ApiUserInformation user_info)
    : api_(api), control_flags_(control_flags), id_name_(std::move(id_name)), user_info_(std::move(user_info)) {
    api->addListener(this);
}

NoAuthControlEnabler::~NoAuthControlEnabler() {
    if (api_) {
        api_->removeListener(this);
    }
}

void NoAuthControlEnabler::listenerAdded(Api* api, const std::shared_ptr<Session>& active_session) {
    if (active_session) {
        active_session->registerToControl(control_flags_, id_name_, user_info_);
    }
}

void NoAuthControlEnabler::listenerRemoved(Api* api) { api_ = nullptr; }

void NoAuthControlEnabler::sessionStateChanged(const std::shared_ptr<Session>& session, Session::State state) {
    if (state == Session::State::connected_monitor) {
        session->registerToControl(control_flags_, id_name_, user_info_);
    }
}

void NoAuthControlEnabler::controlFlagsChanged(const std::shared_ptr<Session>& session, uint32_t control_flags) {}

SecureControlEnabler::SecureControlEnabler(Api* api, uint32_t control_flags, std::string id_name,
                                           ApiUserInformation user_info, std::vector<uint8_t> public_key,
                                           std::vector<uint8_t> private_key)
    : api_(api), control_flags_(control_flags), id_name_(std::move(id_name)), user_info_(std::move(user_info)) {
    public_key_  = std::move(public_key);
    private_key_ = std::move(private_key);

    api->addListener(this);
}

SecureControlEnabler::~SecureControlEnabler() {
    if (api_) {
        api_->removeListener(this);
    }
}
void SecureControlEnabler::listenerAdded(Api* api, const std::shared_ptr<Session>& active_session) {
    if (active_session) {
        setupSecureSession(active_session);
    }
}

void SecureControlEnabler::listenerRemoved(Api* api) { api_ = nullptr; }

void SecureControlEnabler::sessionStateChanged(const std::shared_ptr<Session>& session, Session::State state) {
    if (state == Session::State::connected_monitor) {
        setupSecureSession(session);
    }
}

void SecureControlEnabler::controlFlagsChanged(const std::shared_ptr<Session>& session, uint32_t flags) {}

void SecureControlEnabler::setupSecureSession(const std::shared_ptr<Session>& session) const {
    SecureSessionOptions secure_session_options = api_->getSession()->getSecureSessionOptions();
    assert(secure_session_options.isValid() && "Invalid secure session options");

    auto                           secure_session = std::make_unique<security::SecureSession>();
    SecureSessionKeyExchangeResult r              = secure_session->getSecureSessionParameters().tryKeyExchange(
        secure_session_options.session_id, secure_session_options.options[0], private_key_, public_key_);

    assert(r == SecureSessionKeyExchangeResult::ok && "Secure session key exchange failed");
    if (r == SecureSessionKeyExchangeResult::ok) {
        secure_session->generateSymmetricEncryptionKey(id_name_);

        session->registerToControl(control_flags_, id_name_, user_info_, std::move(secure_session));
    }
}

}  // namespace sc_api::core
