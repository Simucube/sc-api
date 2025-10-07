#include <sc-api/api.h>
#include <sc-api/device_info.h>
#include <sc-api/events.h>
#include <sc-api/ffb.h>
#include <sc-api/sim_data.h>
#include <sc-api/telemetry.h>
#include <sc-api/time.h>

#include <cassert>
#include <iostream>

void printDeviceInfo(const std::shared_ptr<sc_api::device_info::FullInfo> info) {
    std::cout << "Connected devices:\n";
    for (const sc_api::device_info::DeviceInfo& device : *info) {
        std::cout << "UID: " << device.getUid() << " Session id: " << device.getSessionId().id
                  << " role: " << toString(device.getRole()) << std::endl;

        if (device.getParentSessionId()) {
            std::cout << "\tParent: " << info->getBySessionId(device.getParentSessionId())->getUid() << "\n";
        }

        if (!device.getFeedbacks().empty()) {
            std::cout << "\tFeedback types:\n";
            for (const auto& ffb : device.getFeedbacks()) {
                std::cout << "\t\t" << ffb.id << " type: " << toString(ffb.type) << "\n";
            }
        }

        std::cout << std::endl;
    }
}

int main(int argc, char* argv[]) {
    sc_api::Api                              api;
    std::unique_ptr<sc_api::Api::EventQueue> event_queue = api.createEventQueue();

    static constexpr float force_output_N                = 5;
    while (true) {
        auto event = event_queue->pop();
        if (auto* s = sc_api::event::getIfDeviceInfoChanged(&event)) {
            std::cout << "Device info changed\n\n";

            if (s->session) {
                auto device_info = s->session->getDeviceInfo();
                if (device_info) {
                    printDeviceInfo(device_info);
                }
            }
        }
    }
}
