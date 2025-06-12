#include <sc-api/api.h>
#include <sc-api/core/command.h>
#include <sc-api/core/util/bson_builder.h>
#include <sc-api/core/util/bson_reader.h>
#include <sc-api/events.h>
#include <sc-api/sim_data.h>
#include <sc-api/telemetry.h>

#include <cassert>
#include <iostream>
#include <thread>

// Update groups need an id number to differentiatate them
static constexpr uint16_t k_telemetry_update_group_id = 0;

static void telemetryThread(sc_api::core::Api* api) {
    sc_api::core::TelemetryUpdateGroup        engine_rpm_update_group{0};

    sc_api::core::Telemetry                   physics_running(sc_api::telemetry::physics_running, true);
    sc_api::core::Telemetry                   engine_rpm_telemetry(sc_api::telemetry::engine_rpm, 0.0f);
    std::vector<sc_api::core::TelemetryBase*> telemetries({&engine_rpm_telemetry, &physics_running});

    auto api_event_queue = api->createEventQueue();

    float rpm_change     = 10.0f;
    float cur_rpm        = 1000.0f;
    std::shared_ptr<sc_api::core::Session> session;
    while (true) {
        // Make sure that telemetry system keeps up-to-date with changes to the session state
        // and handle reconnecting correctly
        while (auto event = api_event_queue->tryPop()) {
            if (auto* s = sc_api::event::getIfSessionStateChanged(&*event)) {
                if (s->session && (s->control_flags & sc_api::Session::control_telemetry) != 0u) {
                    if (session != s->session) {
                        session = s->session;

                        // If we get event that informs we may control telemetry
                        engine_rpm_update_group.configure(session->getTelemetries());
                    }
                }
            }
        }

        // lets just generate some data that so it is easy to see that something happens
        cur_rpm += rpm_change;
        if (cur_rpm > 8000.0f) {
            cur_rpm    = 8000.0f;
            rpm_change = -10.0f;
        } else if (cur_rpm < 1000.0f) {
            cur_rpm    = 1000.0f;
            rpm_change = 10.0f;
        }

        engine_rpm_telemetry.setValue(cur_rpm);
        engine_rpm_update_group.send();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

int main(int argc, char* argv[]) {
    // Use helper thread to handle background processing of different data sources
    // We could use sc_api::core::Api directly also
    sc_api::core::Api api;

    // Controlling devices or sending telemetry requires enabling control and "registering" the user of API
    // This information can be shown in Tuner to allow user to handle incompatibilities or prioritization
    sc_api::core::ApiUserInformation user_info;
    user_info.author         = "Simucube";
    user_info.display_name   = "sc-api-example";
    user_info.type           = "tool";
    user_info.version_string = "0.1";

    sc_api::core::NoAuthControlEnabler control_enabler(
        &api, sc_api::core::Session::ControlFlag::control_telemetry | sc_api::core::Session::control_sim_data,
        "sc-api-example", user_info);

    // Demonstrate multi-threading by creating a separate thread that generates telemetry data
    std::thread telemetry_thread([&]() { telemetryThread(&api); });

    auto api_event_queue                           = api.createEventQueue();

    std::shared_ptr<sc_api::core::Session> session = nullptr;
    sc_api::core::SessionState             session_state = sc_api::core::SessionState::invalid;
    while (true) {
        if (auto event = api_event_queue->tryPopFor(std::chrono::milliseconds(200))) {
            if (auto session_state_changed = std::get_if<sc_api::core::session_event::SessionStateChanged>(&*event)) {
                session       = session_state_changed->session;
                session_state = session_state_changed->state;

                if (session_state == sc_api::SessionState::connected_control) {
                    using namespace sc_api::sim_data;
                    UpdateBuilder update("example-sim", true);

                    SimBuilder sim;
                    sim.set(sc_api::core::sim_data::sim::name, "Example Sim");
                    update.buildAndSet(sim);

                    VehiclesBuilder vehicles;
                    VehicleBuilder  v_b;

                    namespace p = sc_api::core::sim_data::vehicle;
                    v_b.set(p::name, "Example Vehicle 5000");
                    v_b.set(p::engine_idle_rpm, 1000);
                    v_b.set(p::engine_redline_rpm, 9000);
                    vehicles.buildAndAdd("example-vehicle", v_b);

                    update.buildAndSet(vehicles);
                    session->blockingReplaceSimData(update);
                }
            }
        }

        if (session_state == sc_api::core::SessionState::connected_control) {
            // Sending raw commands.

            sc_api::core::CommandRequest req{"core", "echo"};
            req.docAddElement("some_really_important_data", "that we get back in the result");
            auto cmd_start_time   = std::chrono::high_resolution_clock::now();
            bool command_sent     = session->asyncCommand(std::move(req), [cmd_start_time](
                                                                          const sc_api::core::AsyncCommandResult& result) {
                auto cmd_reply_time = std::chrono::high_resolution_clock::now();
                assert(result.getResultCode() == sc_api::ResultCode::ok);
                assert(result.getPayload());

                sc_api::core::util::BsonReader r(result.getPayload());
                assert(r.next() == sc_api::core::util::BsonReader::ELEMENT_STR);
                assert(r.key() == "some_really_important_data");
                assert(r.stringValue() == "that we get back in the result");

                std::cerr
                    << "Received reply in "
                    << std::chrono::duration_cast<std::chrono::microseconds>(cmd_reply_time - cmd_start_time).count()
                    << "us" << std::endl;
            });

            if (!command_sent) {
                std::cerr << "Failed to send echo command" << std::endl;
            }
        }
    }

    return 0;
}
