#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <sc-api/events.h>
#include <sc-api/session.h>

#include "bind_util.h"

namespace nb = nanobind;

using sc_api::Session;
using sc_api::SessionState;
using sc_api::event::DeviceInfoChanged;
using sc_api::event::SessionStateChanged;
using sc_api::event::SimDataChanged;
using sc_api::event::TelemetryDefinitionsChanged;
using sc_api::event::VariableDefinitionsChanged;

void bind_events(nb::module_& m) {
    nb::class_<SessionStateChanged>(m, "SessionStateChanged", "Emitted when a session connects or disconnects.")
        .def_prop_ro(
            "session", [](const SessionStateChanged& e) { return e.session; },
            "The Session object, or None if the session was lost.")
        .def_prop_ro(
            "state", [](const SessionStateChanged& e) { return e.state; }, "The new SessionState value.")
        .def_prop_ro(
            "controller_id", [](const SessionStateChanged& e) { return e.controller_id; },
            "Identifier of the controller that triggered the state change.")
        .def_prop_ro(
            "control_flags",
            [](const SessionStateChanged& e) { return static_cast<Session::ControlFlag>(e.control_flags); },
            "Bitmask of ``ControlFlag`` values granted for this session state.")
        .def("__repr__", [](const SessionStateChanged& e) {
            return std::string("<SessionStateChanged state=") + session_state_str(e.state) +
                   " controller_id=" + std::to_string(e.controller_id) +
                   " control_flags=" + std::to_string(e.control_flags) + ">";
        });

    nb::class_<DeviceInfoChanged>(m, "DeviceInfoChanged",
                                  "Emitted when the device tree changes (devices added, removed, or updated).")
        .def_prop_ro(
            "session", [](const DeviceInfoChanged& e) { return e.session; },
            "The Session in which the device change occurred.")
        .def("__repr__", [](const DeviceInfoChanged&) { return "<DeviceInfoChanged>"; });

    nb::class_<VariableDefinitionsChanged>(m, "VariableDefinitionsChanged",
                                           "Emitted when the available variable definitions change.")
        .def_prop_ro(
            "session", [](const VariableDefinitionsChanged& e) { return e.session; },
            "The Session whose variable definitions changed.")
        .def("__repr__", [](const VariableDefinitionsChanged&) { return "<VariableDefinitionsChanged>"; });

    nb::class_<TelemetryDefinitionsChanged>(m, "TelemetryDefinitionsChanged",
                                            "Emitted when the available telemetry definitions change.")
        .def_prop_ro(
            "session", [](const TelemetryDefinitionsChanged& e) { return e.session; },
            "The Session whose telemetry definitions changed.")
        .def("__repr__", [](const TelemetryDefinitionsChanged&) { return "<TelemetryDefinitionsChanged>"; });

    nb::class_<SimDataChanged>(m, "SimDataChanged", "Emitted when simulator state data is updated.")
        .def_prop_ro(
            "session", [](const SimDataChanged& e) { return e.session; },
            "The Session in which the simulator data was updated.")
        .def("__repr__", [](const SimDataChanged&) { return "<SimDataChanged>"; });
}
