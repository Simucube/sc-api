/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SESSION_H_
#define SC_API_CORE_SESSION_H_

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "device_info_fwd.h"
#include "protocol/security.h"
#include "result.h"
#include "session_fwd.h"

namespace sc_api::core {

class ApiCore;
class VariableDefinitions;
class TelemetryDefinitions;

namespace sim_data {
class SimData;
class SimDataUpdateBuilder;
}

class FfbPipeline;

/** Different options for establishing secure session that are supported by the current version of the backend
 */
struct SecureSessionOptions {
    uint32_t session_id = 0;

    struct Method {
        SC_API_PROTOCOL_SecurityMethod_t method;
        std::vector<uint8_t>             public_key;
        std::vector<uint8_t>             signature;
    };

    std::vector<Method> options;

    bool isValid() const { return !options.empty(); }
};

enum class SecureSessionKeyExchangeResult {
    ok,

    /** Public key signature wasn't correct */
    error_signature_verification_failed,

    /** Provided private key wasn't correctly constructed for the key exchange method */
    error_invalid_private_key,

    /** Provided public key wasn't correctly constructed for the key exchange method */
    error_invalid_public_key,

    /** Method isn't supported by this implementation */
    error_not_supported
};

/**
 * @todo Partial implementation. DO NOT USE
 */
struct SecureSessionParameters {
    /** Try to generate secure session parameters by doing key exchange
     *
     *  Verifies API backend public key by the signature and does key exchange defined by method by using
     *  provided private and public keys
     */
    SecureSessionKeyExchangeResult tryKeyExchange(uint32_t session_id, const SecureSessionOptions::Method& method,
                                                  const std::vector<uint8_t>& api_user_private_key,
                                                  const std::vector<uint8_t>& api_user_public_key);

    SC_API_PROTOCOL_SecurityMethod_t method = SC_API_PROTOCOL_SECURITY_METHOD_NONE;

    /** Sanity check to make sure that SecureSessionParameters are generated based on the correct session options */
    uint32_t session_id                     = 0;

    /** Public key of the API user that is used to generate shared_secret
     *
     * This is sent to the API backend so that it can also generate the shared secret
     * based on the selected security method
     */
    std::vector<uint8_t> controller_public_key;

    /** Shared secret that is generated from API users private key and API backends public key
     *
     * This is used to derive the symmetric encryption key that is used to encrypt commands.
     */
    std::vector<uint8_t> shared_secret;
};

class SecureSessionInterface {
public:
    virtual ~SecureSessionInterface();

    virtual void generateSymmetricEncryptionKey(const std::string& controller_id_name) = 0;

    virtual void encrypt(uint8_t* iv, uint8_t* aad, uint32_t aad_len, uint8_t* data, uint32_t data_len,
                         uint8_t* tag)                                                 = 0;

    SecureSessionParameters& getSecureSessionParameters() { return secure_session_params_; }

protected:
    SecureSessionParameters secure_session_params_;
};

/** Information about the software that uses the API
 *
 * This can be used by UI to resolve resource or compatibility issues
 */
struct ApiUserInformation {
    /** Display name of this software that can be shown in UI */
    std::string display_name;

    /** Type of the software that uses API */
    std::string type;

    /** Path of the program that uses */
    std::string path;

    /** Who made this soft ware*/
    std::string author;

    /** Application specific version string that can be shown in UI for debug purposes */
    std::string version_string;
};

class Session;
class CommandRequest;

/** Session that has the ownership to handles related to the API
 *
 * Ownership is managed through std::shared_ptr, when the last pointer to the session
 * is destroyed, session is finally completely closed and freed. Session may be lost before
 * this which causes the session object to contain handles to data that won't be anymore updated
 * but the data is still available until the session object is actually destroyed. In these cases
 * the new session should be formed by requesting that from API and handles to the previous session
 * should be discarded and replaced with the new session.
 */
class Session : public std::enable_shared_from_this<Session> {
    friend class ApiCore;

public:
    struct Internal;
    friend struct Internal;

    ~Session();

    using State = SessionState;

    /** Areas of functionality that API user would like or is allowed to control */
    enum ControlFlag : uint32_t {
        /** Allow directly creating telemetry effects
         *
         * Allows accessing capabilities to generate ffb effects
         */
        control_ffb_effects = 1 << 0,

        /** Allow providing telemetry data
         *
         * Access to low latency path to provide information to generate effects and update dashes
         */
        control_telemetry   = 1 << 1,

        /** Provide simulator related status data
         *
         * Player name, car name, track name etc.
         * This is required to use sim_data_builder
         */
        control_sim_data    = 1 << 2,
    };

    /** Get list of currently supported secure session options for session
     */
    SecureSessionOptions getSecureSessionOptions() const;

    SecureSessionInterface* getSecureSession() const;

    /** Registers this API user as a controller and allows executing commands
     *
     * Will block until backend responds to the request, or timeout after 200ms.
     *
     * @param control_flags Combination of ControlFlags which specified which parts of the API
     * @param id_name Identifying name of this application. Shouldn't not change to allow reusing
     *                data and keeping state consistent. Max 16 characters + null-termination.
     * @param user_info Metadata about this application/plugin that interfaces with the API
     * @return ResultCode::ok, if registering was sent successfully
     *         ResultCode::error_invalid_argument, if arguments don't fill requirements
     *         ResultCode::error_busy, if there is already one registration in progress
     *         ResultCode::error_invalid_session_state, if called when session state is SessionState::disconnected
     *         ResultCode::error_cannot_connect, if connection to backend cannot be created
     */
    ResultCode registerToControl(uint32_t control_flags, const std::string& id_name,
                                 const ApiUserInformation&               user_info,
                                 std::unique_ptr<SecureSessionInterface> secure_session = nullptr);

    /** Get currently active control flags
     *
     * Prefer listening event queue and SessionStateChanged event instead.
     *
     * @note thread-safe
     *
     */
    uint32_t getControlFlags() const;

    /** Process functions handle session management and IO to the API
     *
     * Will handle commands and callbacks if new results are received, and update the state.
     * Blocks until session state changed for any reason.
     *
     * Thread-safe
     *
     * \return State current state of the session
     */
    State runUntilStateChanges();

    /** Handles commands and callbacks that are currently ready to run
     *
     * Will handle commands and callbacks if new results are received, and update the state.
     * Returns when the processing loop would have to wait for more data.
     *
     * @note Thread-safe
     *
     * \return State current state of the session
     */
    State poll();

    /** Will cause on going run call to exit
     *
     *  Thread-safe
     */
    void stop();

    /** Return the current state of the session */
    State getState() const;

    /** Closes the session
     *
     * All communication sockets are closed
     *
     * Shared memory resources stay valid until this Session instance is freed, when the shared pointer reference count
     * goes to zero.
     */
    ResultCode close();

    /** Id number of this API user for this session.
     *
     * Used by API backend to identify different API users and only valid after controller has been registered
     */
    uint16_t getControllerId() const { return controller_id_; }

    /** Pointer to ApiCore instance that established this session */
    ApiCore* getApiCore() const { return api_; }

    /** Numeric id of this session */
    uint32_t getId() const { return session_id_; }

    /** Get parsed sim data from the most recent refreshSimData call */
    std::shared_ptr<sim_data::SimData> getSimData();

    std::shared_ptr<core::device_info::FullInfo> getDeviceInfo();
    VariableDefinitions                          getVariables();
    TelemetryDefinitions                         getTelemetries();

    /** Tries to send command to the backend and calls callback asynchronously when result is received
     *
     * Requires that session has been registered. If session isn't registered or has closed, the function
     * does nothing and returns false.
     *
     * @param req Request data that is sent to the backend. Request data is consumed and the request must be
     *            reinitialized and rebuilt if the same instance is reused
     * @param result_cb Callback function that is called when the command result is received.
     *                  Callback will be called from the thread that has called one of the run/poll functions.
     * @return true, if command could be sent and the callback will be called.
     */
    bool asyncCommand(CommandRequest&& req, std::function<void(const AsyncCommandResult&)> result_cb);

    /** Executes command and blocks until respon */
    CommandResult blockingCommand(CommandRequest&& req);

    /** Executes command and blocks until reply is received or connection is lost */
    ResultCode blockingSimpleCommand(CommandRequest&& req);

    /** Execute command to replace Sim data based on the given build
     */
    bool blockingReplaceSimData(sim_data::SimDataUpdateBuilder& builder);

    bool asyncReplaceSimData(sim_data::SimDataUpdateBuilder&                builder,
                             std::function<void(const AsyncCommandResult&)> result_cb);

    /** Execute command to update Sim data based on the given builder.
     *
     * Values are replaced but any unreferenced data is left to previous values.
     */
    bool blockingUpdateSimData(sim_data::SimDataUpdateBuilder& builder);
    bool asyncUpdateSimData(sim_data::SimDataUpdateBuilder&                builder,
                            std::function<void(const AsyncCommandResult&)> result_cb);

    class PeriodicTimerHandle {
    public:
        PeriodicTimerHandle() = default;
        ~PeriodicTimerHandle() { destroy(); }
        PeriodicTimerHandle(PeriodicTimerHandle&& h) noexcept {
            std::swap(h.handle_, handle_);
            h.destroy();
        }
        PeriodicTimerHandle(const PeriodicTimerHandle& h) = delete;

        PeriodicTimerHandle& operator=(PeriodicTimerHandle&& h) noexcept {
            if (&h == this) {
                return *this;
            }

            std::swap(h.handle_, handle_);
            return *this;
        }
        PeriodicTimerHandle& operator=(const PeriodicTimerHandle&) = delete;

        void destroy() noexcept;

    private:
        friend class Session;
        PeriodicTimerHandle(Session* session, int32_t handle);

        std::weak_ptr<Session> session_;
        int32_t                handle_ = -1;
    };

    PeriodicTimerHandle createPeriodicTimer(std::chrono::milliseconds period, std::function<void()> callback);

    Internal& getInternal() { return *p_; }

private:
    Session(ApiCore* api, std::unique_ptr<Internal> handles, uint32_t session_id);

    void startPeriodicUpdateTimer();
    bool periodicUpdate();

    void disconnected();

    void checkDefinitions();

    ApiCore* api_;

    mutable std::mutex m_;

    std::unique_ptr<Internal> p_;
    uint32_t                  control_flags_       = 0;
    State                     state_               = State::connected_monitor;

    std::chrono::steady_clock::time_point prev_keep_alive_;
    uint32_t                              session_id_            = 0;
    uint32_t                              prev_keep_alive_value_ = 0;

    std::string control_id_name_;
    uint16_t    controller_id_ = 0;

    bool is_running_           = false;

    std::unique_ptr<SecureSessionInterface>          secure_session_;
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_SESSION_H_
