#include <sc-api/api.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/variables.h>

#include <cassert>
#include <iostream>

using namespace sc_api;

struct PedalData {
    std::string             uid;
    device_info::DeviceRole role;
    const float*            force    = nullptr;
    const float*            position = nullptr;
};

int main(int argc, char* argv[]) {
    Api                                  api_thread;
    std::unique_ptr<Api::EventQueue>     eventQueue = api_thread.createEventQueue();

    std::vector<PedalData>      pedals;
    sc_api::VariableDefinitions variables;
    bool                        devices_changed = false;
    while (true) {
        // Check events to know when we potentially have some new pedal state available
        while (auto opt_event = eventQueue->tryPop()) {
            if (auto* event = sc_api::event::getIfDeviceInfoChanged(&opt_event)) {
                variables       = event->session->getVariables();
                devices_changed = true;
            }
        }

        if (devices_changed) {
            devices_changed = false;
            std::cout << "Devices changed" << std::endl;

            // Find all connected ActivePedals by filtering for devices that support active_pedal feedback
            // We could also try searching the brake pedal by checking that the device role is brake,
            // but that will also find passive pedals
            auto device_info             = variables.getSession()->getDeviceInfo();
            auto connected_active_pedals = device_info->findAllByFilter([](const device_info::DeviceInfo& device) {
                return device.hasFeedbackType(device_info::FeedbackType::active_pedal);
            });

            // Fill vector that contains information about all connected ActivePedals
            pedals.clear();
            for (auto& ap : connected_active_pedals) {
                PedalData data;
                data.uid   = ap->getUid();
                data.role  = ap->getRole();

                // Find pointers to variables with pedal state information
                // These pointers will stay valid as long as sc_api::Session object exists. VariableDefinitions has
                // handle to the Session object, so these pointers are valid at least until we call getVariables again
                // and refresh variables
                //
                // In more complex usage, it is recommended to pass std::shared_ptr<sc_api::Session> with the variable
                // pointers so that there is never case where sc_api::Session is destroyed early and variable pointers
                // are left dangling
                data.force = variables.findValuePointer(sc_api::core::variable::activepedal::force, ap->getSessionId());
                data.position = variables.findValuePointer(sc_api::core::variable::activepedal::pedal_face_pos_mm,
                                                           ap->getSessionId());

                assert(data.force && data.position);
                pedals.push_back(data);
            }
        }

        // Print current status of pedals
        std::cout << "ActivePedals:\n";
        for (auto& pedal : pedals) {
            std::cout << "  " << toString(pedal.role) << ", uid=" << pedal.uid << ", position:" << *pedal.position
                      << " mm, force: " << *pedal.force << " N\n";
        }
        std::cout << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
