#include "sc-api/led_control.h"

#include <algorithm>
#include <iostream>

#include "sc-api/api.h"
#include "sc-api/device_info.h"
#include "sc-api/events.h"

int main(int argc, char* argv[]) {
    sc_api::Api                              api;
    std::unique_ptr<sc_api::Api::EventQueue> event_queue = api.createEventQueue();

    sc_api::ApiUserInformation api_user_information;
    api_user_information.display_name   = "example2";
    api_user_information.type           = "";
    api_user_information.path           = "";
    api_user_information.author         = "Simucube";
    api_user_information.version_string = "";

    sc_api::NoAuthControlEnabler control_enabler(&api, sc_api::Session::control_ffb_effects, "example2",
                                                 api_user_information);

    auto session = api.getSession();

    sc_api::device_info::DeviceInfoPtr device_with_leds;

    auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    std::cout << "Wait 5s for AP brake and throttle to connect" << std::endl;
    while (auto opt_event = event_queue->tryPopUntil(timeout)) {
        const sc_api::Event event = *opt_event;

        // Wait for session to connect and control to be available
        if (auto* s = sc_api::event::getIfSessionStateChanged(&event)) {
            if (s->session && (s->control_flags & sc_api::Session::control_ffb_effects) != 0u) {
                session = s->session;
            }
        };

        if (session) {
            auto device_info = session->getDeviceInfo();
            device_with_leds = device_info->findFirstByFilter([](const sc_api::device_info::DeviceInfo& info) {
                return info.hasFeedbackType(sc_api::device_info::FeedbackType::rgb_light);
            });
            if (device_with_leds) {
                break;
            }
        }
    }

    if (device_with_leds) {
        std::cout << "Blinking all leds of device " << device_with_leds->getUid() << " to red for 10s" << std::endl;

        sc_api::LedControl led_control(session, device_with_leds->getSessionId());
        // Get list of all RGB lights on the device
        auto                  rgb_lights = device_with_leds->getRgbLights();
        std::vector<uint32_t> controlled_led_index;

        std::cout << "Controlling lights: ";
        for (const sc_api::device_info::RgbLightFeedback& rgb_light : rgb_lights) {
            std::cout << rgb_light.index << ", ";
            controlled_led_index.push_back(rgb_light.index);
        }

        // Led index order should be logical
        std::sort(controlled_led_index.begin(), controlled_led_index.end());
        std::cout << std::endl;

        // Define which leds we want to control
        led_control.setControlledLeds(controlled_led_index.data(), controlled_led_index.size());

        /*std::vector<sc_api::RgbColor> colors{rgb_lights.size(), sc_api::RgbColor{255, 0, 0}};
        for (size_t i = 0; i < colors.size(); ++i) {
            colors[i] = sc_api::RgbColor{(uint8_t)(255 - (i * 255 / (colors.size() - 1))),
                                         (uint8_t)(i * 255 / (colors.size() - 1)), 0};
        }

        int r = 0;
        for (size_t i = 0; i < 1000; ++i) {
            int d = (int)(i % 50);
            if (d > 25) {
                r = 255 - (d * 10);
            } else {
                r = d * 10;
            }
            for (size_t j = 0; j < colors.size(); ++j) {
                colors[j] = sc_api::RgbColor{(uint8_t)r, (uint8_t)(j * 255 / (colors.size() - 1)), 0};
            }
            led_control.setColors(colors.data(), colors.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(15));
        }*/
        std::vector<sc_api::RgbColor> colors{rgb_lights.size(), sc_api::RgbColor{255, 0, 0}};
        for (int i = 0; i < 1000; ++i) {
            led_control.setColors(colors.data(), colors.size());
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            led_control.clearLeds();
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

    return 0;
}
