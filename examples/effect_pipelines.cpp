#include <sc-api/api.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/ffb.h>
#include <sc-api/sim_data.h>
#include <sc-api/telemetry.h>
#include <sc-api/time.h>

#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

int main(int argc, char* argv[]) {
    sc_api::Api                              api_thread;
    std::unique_ptr<sc_api::Api::EventQueue> eventQueue = api_thread.createEventQueue();

    sc_api::ApiUserInformation apiUserInformation;
    apiUserInformation.display_name   = "example2";
    apiUserInformation.type           = "";
    apiUserInformation.path           = "";
    apiUserInformation.author         = "Simucube";
    apiUserInformation.version_string = "";

    sc_api::NoAuthControlEnabler control_enabler(&api_thread, sc_api::Session::control_ffb_effects, "example2",
                                                 apiUserInformation);

    std::shared_ptr<sc_api::Session> session;
    sc_api::DeviceSessionId          brake_ap;
    sc_api::DeviceSessionId          throttle_ap;
    auto                     timeout     = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::cout << "Wait 10s for AP brake and throttle to connect" << std::endl;
    while (auto opt_event = eventQueue->tryPopUntil(timeout)) {
        const sc_api::Event event = *opt_event;

        // Wait for session to connect and control to be available
        if (auto* s = sc_api::event::getIfSessionStateChanged(&event)) {
            if (s->session && (s->control_flags & sc_api::Session::control_ffb_effects) != 0u) {
                session = s->session;
            }
        };

        if (session) {
            auto device_info = session->getDeviceInfo();
            for (const sc_api::device_info::DeviceInfo& device : *device_info) {
                if (device.hasFeedbackType(sc_api::device_info::FeedbackType::active_pedal)) {
                    if (device.getRole() == sc_api::device_info::DeviceRole::brake_pedal) {
                        brake_ap = device.getSessionId();
                    } else if (device.getRole() == sc_api::device_info::DeviceRole::throttle_pedal) {
                        throttle_ap = device.getSessionId();
                    }
                }
            }

            if (brake_ap && throttle_ap) {
                break;
            }
        }
    }

    if (!session) {
        std::cout << "Could not form SC-API session within 10s" << std::endl;
        return 1;
    }

    std::shared_ptr<sc_api::device_info::FullInfo> device_info = session->getDeviceInfo();
    for (const sc_api::device_info::DeviceInfo& device : *device_info) {
        std::cout << "Device UID: " << device.getUid() << " Session id: " << device.getSessionId().id
                  << " role: " << toString(device.getRole());
    }

    if (!brake_ap && !throttle_ap) {
        std::cout << "Could not find ActivePedal brake and throttle within 10s" << std::endl;
        return 2;
    }

    using sc_api::device_info::DeviceRole;
    using sc_api::device_info::FeedbackType;

    std::unique_ptr<sc_api::FfbPipeline> pipelineT;
    if (throttle_ap) {
        sc_api::PipelineConfig configT;
        configT.offset_type = sc_api::OffsetType::force_N;
        pipelineT           = std::make_unique<sc_api::FfbPipeline>(session, throttle_ap);
        pipelineT->configure(configT);
    }

    std::unique_ptr<sc_api::FfbPipeline> pipelineB;
    if (brake_ap) {
        sc_api::PipelineConfig configB;
        configB.offset_type = sc_api::OffsetType::force_N;
        pipelineB           = std::make_unique<sc_api::FfbPipeline>(session, brake_ap);
        pipelineB->configure(configB);
    }

    auto start_time       = sc_api::Clock::now();

    // 1000Hz update rate
    auto update_rate      = std::chrono::milliseconds(1);

    // Give 3ms time for the samples to reach the pedal and to give some time for pedal to interpolate between
    // separate samples.
    auto sampleTimeOffset = std::chrono::milliseconds(3);

    std::this_thread::sleep_for(std::chrono::seconds(1));

    float force_N = 2;
    while (true) {
        while (auto event = eventQueue->tryPop()) {
            if (auto* s = sc_api::event::getIfSessionStateChanged(&event)) {
                if (s->state != sc_api::SessionState::connected_control) {
                    std::cerr << "Session was disconnected. Closing example." << std::endl;
                    return 0;
                }
            }
        }

        auto    cur_time     = sc_api::Clock::now();
        float   freq         = 20.0f;

        double seconds_from_start =
            std::chrono::duration_cast<std::chrono::duration<double>>(cur_time - start_time).count();
        float v                                           = (float)std::sin(seconds_from_start * freq * 3.1415 * 2);

        // Use two samples because using only one sample would produce sawtooth pattern if the next sample arrives after
        // this sample start time Linear interpolation will try to interpolate samples so that the first sample value
        // will be reached at exactly the given start timestamp (so the interpolation starts before the start timestamp)
        // and after last samples timestamp offset will be interpolated towards 0 offset if there are no more samples
        // available
        static constexpr uint32_t k_sample_count          = 2;
        float                     samples[k_sample_count] = {v * force_N, v * force_N};

        if (pipelineT &&
            !pipelineT->generateEffect(cur_time + sampleTimeOffset, update_rate * 2, samples, k_sample_count)) {
            std::cerr << "T failed\n";
        }
        if (pipelineB &&
            !pipelineB->generateEffect(cur_time + sampleTimeOffset, update_rate * 2, samples, k_sample_count)) {
            std::cerr << "B failed\n";
        }

        // Do some busy looping while we wait for the next update time
        // this will cause some scheduling jitter
        while (sc_api::Clock::now() < cur_time + update_rate) {
            std::this_thread::yield();
        }
    }
}
