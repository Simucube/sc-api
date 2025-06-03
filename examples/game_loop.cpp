#include <sc-api/api.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/ffb.h>
#include <sc-api/sim_data.h>
#include <sc-api/telemetry.h>
#include <sc-api/variables.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

using namespace sc_api::core;

/** Struct that store ActivePedal related state information and handles */
struct ApState {
    void initialize(const std::shared_ptr<Session>& session) {
        std::cout << "Initializing brake pedal. Device session id=" << device_session_id.id << std::endl;
        sc_api::VariableDefinitions variables = session->getVariables();

        // Find pointers to state variables
        vars.force_N     = variables.findValuePointer(variable::activepedal::force, device_session_id);
        vars.travel_mm   = variables.findValuePointer(variable::activepedal::pedal_face_travel_mm, device_session_id);
        vars.position_mm = variables.findValuePointer(variable::activepedal::pedal_face_pos_mm, device_session_id);
        vars.absolute_position_mm =
            variables.findValuePointer(variable::activepedal::abs_pedal_face_pos_mm, device_session_id);
        vars.input = variables.findValuePointer(variable::activepedal::primary_input, device_session_id);

        // Create pipelines for controlling force offset
        if (!force_pipeline) {
            force_pipeline = std::make_unique<sc_api::FfbPipeline>(session, device_session_id);

            PipelineConfig config;
            config.offset_type = OffsetType::force_N;
            force_pipeline->configure(config);
        }

        if (!relative_force_pipeline) {
            relative_force_pipeline = std::make_unique<sc_api::FfbPipeline>(session, device_session_id);

            PipelineConfig config;
            config.offset_type = OffsetType::force_relative;
            relative_force_pipeline->configure(config);
        }
    }

    DeviceSessionId device_session_id;

    struct Variables {
        const float* force_N              = nullptr;
        const float* position_mm          = nullptr;
        const float* absolute_position_mm = nullptr;
        const float* travel_mm            = nullptr;
        const float* input                = nullptr;
    } vars;

    std::unique_ptr<FfbPipeline> force_pipeline;
    std::unique_ptr<FfbPipeline> relative_force_pipeline;
};

struct ApiState {
    // EventQueue that is used to detect changes in the session state
    std::unique_ptr<sc_api::Api::EventQueue> event_queue;

    // Currently active session
    std::shared_ptr<sc_api::Session> session;

    // Brake pedal state
    std::optional<ApState>   brake;

    // Do we need to re-initialize brake
    bool                     init_needed = false;

    void reset() {
        brake.reset();
        init_needed = true;
    }

    /** Connected devices have changed so we should recheck that our session specific handles point to correct type
     *  of devices.
     */
    void remapDevices() {
        if (!session) return;

        std::cout << "Checking what session id the brake pedal has" << std::endl;
        auto device_info = session->getDeviceInfo();

        DeviceSessionId brake_ap =
            device_info->findFirstSessionIdByFilter([](const sc_api::device_info::DeviceInfo& device) {
                return device.getRole() == sc_api::device_info::DeviceRole::brake_pedal &&
                       device.hasFeedbackType(sc_api::device_info::FeedbackType::active_pedal);
            });

        if (brake && brake->device_session_id != brake_ap) {
            // Brake pedal has different device id -> Clear brake state and reinitialize it
            brake.reset();
            std::cout << "Brake pedal changed!" << std::endl;
        }

        if (!brake && brake_ap) {
            // We have found a brake pedal that support ActivePedal feedback
            brake       = ApState{brake_ap};
            init_needed = true;
        }
    }

    /** Do periodic update to process events and detect when we need to reinitialize things */
    void update() {
        bool session_changed     = false;
        bool device_info_changed = false;
        while (auto opt_event = event_queue->tryPop()) {
            if (auto* s = sc_api::event::getIfSessionStateChanged(&*opt_event)) {
                if (s->session && (s->control_flags & Session::control_ffb_effects) != 0u) {
                    if (session != s->session) {
                        session         = s->session;
                        session_changed = true;
                        init_needed     = true;
                    }
                }
            }

            device_info_changed |= sc_api::event::hasDeviceInfoChanged(*opt_event);
        }

        // If session changes, we need to reinitialize everything
        if (session_changed) {
            std::cout << "Session changed" << std::endl;
            reset();
            device_info_changed = true;
        }

        // If device info changes, brake pedal maybe now some different pedal (roles can be changed) so we need to
        // potentially remap pipelines to different pedals
        if (device_info_changed) {
            remapDevices();
        }

        if (init_needed && session) {
            init_needed = false;

            if (brake) {
                brake->initialize(session);
            }
        }
    }
};

int main(int argc, char* argv[]) {
    Api api;

    ApiUserInformation apiUserInformation;
    apiUserInformation.display_name   = "example game loop";
    apiUserInformation.type           = "";
    apiUserInformation.path           = "";
    apiUserInformation.author         = "Simucube";
    apiUserInformation.version_string = "";

    NoAuthControlEnabler control_enabler(&api, Session::control_ffb_effects, "example3", apiUserInformation);

    auto start_time              = sc_api::Clock::now();

    // Give 4ms time for the samples to reach the pedal and to give some time for pedal to interpolate between
    // separate samples. If samples arrive too late compared to their start time, glitches may occur in signal,
    // when some samples are dropped
    auto sample_time_offset      = std::chrono::milliseconds(4);

    auto update_rate             = std::chrono::milliseconds(1);

    unsigned debug_print_counter = 0;

    // Use a class instance to store Api related data
    ApiState api_state{api.createEventQueue()};

    while (true) {
        api_state.update();

        auto    cur_time     = sc_api::Clock::now();
        float   freq         = 20.0f;
        double  seconds_from_start =
            std::chrono::duration_cast<std::chrono::duration<double>>(cur_time - start_time).count();
        float v = (float)std::sin(seconds_from_start * freq * 3.1415 * 2);

        ++debug_print_counter;

        if (api_state.brake) {
            ApState& brake = *api_state.brake;

            if (brake.relative_force_pipeline && brake.relative_force_pipeline->isActive()) {
                // Add reduce the force target by half of the current pedal face force so that pedal is 50% softer
                float relative_force_offset = -0.5f;

                // It is possible to use force pipeline and brake.force_N variable to implement this but relative force
                // pipeline will handle offset calculation on device so it is faster responding to pedal force changes

                // Add +-10% sine wave just for fun
                relative_force_offset += v * 0.1f;

                // Use two samples because using only one sample would produce sawtooth pattern if the next sample
                // arrives after this sample start time Linear interpolation will try to interpolate samples so that the
                // first sample value will be reached at exactly the given start timestamp (so the interpolation starts
                // before the start timestamp) and after last samples timestamp offset will be interpolated towards 0
                // offset if there are no more samples available
                static constexpr uint32_t k_sample_count          = 2;
                float                     samples[k_sample_count] = {relative_force_offset, relative_force_offset};

                if (!brake.relative_force_pipeline->generateEffect(cur_time + sample_time_offset, update_rate * 2,
                                                                   samples, k_sample_count)) {
                    std::cerr << "Sending effect failed\n";
                }

                if (debug_print_counter % 1000 == 0) {
                    std::cout << "Brake force: " << *brake.vars.force_N << "N" << std::endl;
                }
            }
        }

        // Do some busy looping while we wait for the next update time
        // this will cause some scheduling jitter
        while (sc_api::Clock::now() < cur_time + update_rate) {
            std::this_thread::yield();
        }
    }
}
