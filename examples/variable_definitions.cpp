/**
 * @file
 *
 * This example shows how variable definitions can be dynamically used to fetch all available quickly refreshing data
 * like device state and telemetry values.
 */

#include <sc-api/api.h>
#include <sc-api/events.h>
#include <sc-api/variables.h>

#include <iostream>

static void printVariableValue(const sc_api::VariableDefinition& def) {
    // Definition contains const void* pointer directly into the shared memory
    // We want to cast it to correct type based on def.
    sc_api::invokeWithValueType(def.type, def.value_ptr, [](auto value) {
        if constexpr (sc_api::isRevisionCountedArrayRef(value)) {
            // Don't bother to print arrays
            std::cout << " array value";
        } else {
            if constexpr (sizeof(value) == 1) {
                // avoid printing char as a letter
                std::cout << (int)value;
            } else {
                std::cout << value;
            }
        }
    });
}

static void printVariableDefinitions(const sc_api::VariableDefinitions& defs) {
    for (const sc_api::VariableDefinition& def : defs) {
        std::cout << def.name << " " << def.type.toString() << " value_ptr: 0x" << def.value_ptr << " value: ";
        printVariableValue(def);
        std::cout << '\n';
    }
    std::cout << std::endl;
}

static void printVariableValues(const sc_api::VariableDefinitions& defs) {
    for (const sc_api::VariableDefinition& def : defs) {
        std::cout << def.name << ": ";
        printVariableValue(def);
        std::cout << '\n';
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
    sc_api::Api api;

    auto                             event_queue = api.createEventQueue();

    // Currently active session. nullptr when there is no connection
    std::shared_ptr<sc_api::Session> session;

    // List of variable definitions that are currently available from the session
    sc_api::VariableDefinitions      variables;
    while (true) {
        // Get events from API to monitor session state and detect when variable definitions change
        sc_api::core::Event event;
        if (!session) {
            // This call will block until some event occurs
            event = event_queue->pop();
        } else {
            // tryPop won't block and return std::nullopt if there are no events in the queue
            // convert std::nullopt to std::monostate to unwrap std::optional
            event = event_queue->tryPop().value_or(std::monostate{});
        }

        bool update_definitions = false;
        if (auto* ev = std::get_if<sc_api::event::SessionStateChanged>(&event)) {
            // SessionStateChanged event occurs when session is connected, disconnected or its state changes because
            // registration for controlling completed.
            // We are not trying to control anything in this example so we only wait for connected_monitor state
            if (ev->state == sc_api::core::SessionState::connected_monitor) {
                session            = ev->session;
                update_definitions = true;
            } else {
                session   = nullptr;

                // Clear variable definitions. Definitions stay valid even when the session is lost, but values stay
                // in their last value forever.
                variables = {};
            }

        } else if (sc_api::event::hasVariableDefinitionsChanged(event)) {
            // VariableDefinitionsChanged event occurs when new variable definitions are added
            // This can occur right after a session is connected or when a new device connects
            update_definitions = true;
        }

        if (update_definitions) {
            // Get current list of variable definitions
            variables = session->getVariables();

            std::cout << "\n\nVariable definitions changed:\n";
            printVariableDefinitions(variables);
        } else {
            // If definitions have not changed, just print values every 5s
            std::this_thread::sleep_for(std::chrono::seconds(5));
            printVariableValues(variables);
        }
    }
}
