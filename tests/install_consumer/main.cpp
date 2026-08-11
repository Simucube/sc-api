/* Uses parts of the API that the install tree must supply:
 * - classes with member functions that live in the library,
 * - global data (sim_data property references), which needs SC_API_EXPORT,
 * - header only constexpr references,
 * - generated headers (sim_data, telemetry_references).
 */
#include <sc-api/api.h>
#include <sc-api/events.h>
#include <sc-api/session.h>
#include <sc-api/sim_data.h>
#include <sc-api/sim_data/vehicle.h>
#include <sc-api/telemetry_references.h>
#include <sc-api/variable_references.h>

#include <cstdio>

int main() {
    sc_api::Api api;
    auto        queue   = api.createEventQueue();
    auto        session = api.getSession();

    std::printf("vehicle property: %.*s\n", (int)sc_api::sim_data::vehicle::engine_idle_rpm.name.size(),
                sc_api::sim_data::vehicle::engine_idle_rpm.name.data());
    std::printf("telemetry: %.*s\n", (int)sc_api::telemetry::engine_rpm.name.size(),
                sc_api::telemetry::engine_rpm.name.data());
    std::printf("variable: %.*s\n", (int)sc_api::variable::activepedal::force.name.size(),
                sc_api::variable::activepedal::force.name.data());
    std::printf("session: %s\n", session ? "present" : "none");
    std::printf("queue: %s\n", queue ? "created" : "null");
    return 0;
}
