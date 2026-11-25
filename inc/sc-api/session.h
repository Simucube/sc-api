/**
 * @file
 * @brief Session class and control registration.
 *
 * A Session is one connection to Simucube Tuner. It gives access to device info, variables,
 * telemetry definitions and sim data. It also carries the command channel.
 *
 * A new session starts in SessionState::connected_monitor, which is read-only. Call
 * Session::registerToControl to get control access.
 */

#ifndef SC_API_SESSION_H_
#define SC_API_SESSION_H_

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "device.h"
#include "device_info_fwd.h"
#include "result.h"
#include "session_fwd.h"
#include "version.h"

namespace sc_api {

class ApiCore;
class VariableDefinitions;
class TelemetryDefinitions;

namespace sim_data {
class SimData;
class SimDataUpdateBuilder;
}  // namespace sim_data

class FfbPipeline;

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

    /** Who made this software */
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

        /** Control display output: RGB lights and dashboard streams
         *
         * Display output cannot move the device. Request this flag for LedControl and DashStreamer.
         * Tuner does not require it yet, and older Tuner versions do not grant it.
         */
        control_display     = 1 << 3,
    };

    /** Registers this API user as a controller and allows executing commands
     *
     * Will block until backend responds to the request, or timeout after 200ms.
     *
     * @param control_flags Combination of ControlFlag values that selects which parts of the API this user controls
     * @param id_name Identifying name of this application. Should not change to allow reusing
     *                data and keeping state consistent. Max 16 characters + null-termination.
     * @param user_info Metadata about this application/plugin that interfaces with the API
     * @param extra_controls Control names to request in addition to control_flags. The backend grants only the
     *                       names it knows; getExtraControls() lists the granted ones.
     * @return ResultCode::ok, if registering was sent successfully
     *         ResultCode::error_invalid_argument, if arguments don't fill requirements
     *         ResultCode::error_busy, if there is already one registration in progress
     *         ResultCode::error_invalid_session_state, if called when session state is SessionState::disconnected
     *         ResultCode::error_cannot_connect, if connection to backend cannot be created
     */
    ResultCode registerToControl(uint32_t control_flags, const std::string& id_name,
                                 const ApiUserInformation&       user_info,
                                 const std::vector<std::string>& extra_controls = {});

    /** Get currently active control flags
     *
     * Prefer listening event queue and SessionStateChanged event instead.
     *
     * @note thread-safe
     *
     */
    uint32_t getControlFlags() const;

    /** Extra control names that the backend granted in the last registerToControl call
     *
     * @note thread-safe
     */
    std::vector<std::string> getExtraControls() const;

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

    /** Return the current state of the session
     *
     *  Thread-safe. The API thread can change the state right after the call.
     */
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

    /** Version of the Tuner that opened this session; 0.0.0 if Tuner does not report its version */
    BackendVersion getBackendVersion() const;

    /** Get parsed sim data from the most recent refreshSimData call */
    std::shared_ptr<sim_data::SimData> getSimData();

    /** Get a device info snapshot of the connected devices. If a newer snapshot is not readable
     *  yet, the call returns the previous one. */
    std::shared_ptr<device_info::FullInfo> getDeviceInfo();

    /** Get a snapshot of the variable definitions of this session */
    VariableDefinitions getVariables();

    /** Get a snapshot of the telemetry definitions of this session */
    TelemetryDefinitions getTelemetries();

    /** Tries to send command to the backend and calls callback asynchronously when result is received
     *
     * Requires that session has been registered. If session isn't registered or has closed, or too many commands
     * are already waiting for a response, the function does nothing and returns false.
     *
     * @param req Request data that is sent to the backend. Request data is consumed and the request must be
     *            reinitialized and rebuilt if the same instance is reused
     * @param result_cb Callback function that is called exactly once: with the command result, or with
     *                  ResultCode::error_invalid_session_state when the connection is lost or the session closes
     *                  first. It is called from the thread that runs one of the run/poll functions, or from the
     *                  thread that calls close() or destroys the session. Do not use a raw Session pointer in it.
     * @return true, if command could be sent and the callback will be called.
     */
    bool asyncCommand(CommandRequest&& req, std::function<void(const AsyncCommandResult&)> result_cb);

    /** Runs a command and blocks until the reply arrives or the timeout passes
     *
     * Must not be called from the thread that runs the session, because that thread delivers the reply.
     */
    CommandResult blockingCommand(CommandRequest&&          req,
                                  std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});

    /** Executes command and blocks until the reply arrives, the connection is lost or the timeout passes
     *
     * Must not be called from the thread that runs the session, because that thread delivers the reply.
     *
     * @return The result code of the reply, or
     *         ResultCode::error_timeout, if no reply arrived in time
     *         ResultCode::error_busy, if too many commands are waiting for a reply; retrying later can succeed
     *         ResultCode::error_no_control, if the session has no open command channel: it is not registered,
     *         or it is already closed or lost
     *         ResultCode::error_invalid_session_state, if the connection is lost or the session closes before the
     *         reply arrives
     */
    ResultCode blockingSimpleCommand(CommandRequest&&          req,
                                     std::chrono::milliseconds timeout = std::chrono::milliseconds{1000});

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
    void closeNoLock();

    void checkDefinitions();

    ApiCore* api_;

    mutable std::mutex m_;

    std::unique_ptr<Internal> p_;
    uint32_t                  control_flags_ = 0;
    std::atomic<State>        state_{State::connected_monitor};  // read from any thread, written by the API thread

    std::chrono::steady_clock::time_point prev_keep_alive_;
    uint32_t                              session_id_            = 0;
    uint32_t                              prev_keep_alive_value_ = 0;

    std::string              control_id_name_;
    uint16_t                 controller_id_ = 0;
    std::vector<std::string> extra_controls_;

    bool is_running_ = false;
};

}  // namespace sc_api

#endif  // SC_API_SESSION_H_
