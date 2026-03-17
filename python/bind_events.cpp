#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <sc-api/core/events.h>
#include <sc-api/core/session.h>

namespace nb = nanobind;

using sc_api::core::Session;
using sc_api::core::SessionState;
using sc_api::core::session_event::DeviceInfoChanged;
using sc_api::core::session_event::SessionStateChanged;
using sc_api::core::session_event::SimDataChanged;
using sc_api::core::session_event::TelemetryDefinitionsChanged;
using sc_api::core::session_event::VariableDefinitionsChanged;

static const char* session_state_str(SessionState s) {
    switch (s) {
        case SessionState::invalid:
            return "invalid";
        case SessionState::connected_monitor:
            return "connected_monitor";
        case SessionState::connected_control:
            return "connected_control";
        case SessionState::session_lost:
            return "session_lost";
        default:
            return "unknown";
    }
}

void bind_events(nb::module_& m) {
    nb::class_<SessionStateChanged>(m, "SessionStateChanged")
        .def_prop_ro(
            "session", [](const SessionStateChanged& e) { return e.session; })
        .def_prop_ro(
            "state", [](const SessionStateChanged& e) { return e.state; })
        .def_prop_ro(
            "controller_id",
            [](const SessionStateChanged& e) { return e.controller_id; })
        .def_prop_ro(
            "control_flags",
            [](const SessionStateChanged& e) { return e.control_flags; })
        .def("__repr__", [](const SessionStateChanged& e) {
            return std::string("<SessionStateChanged state=") +
                   session_state_str(e.state) +
                   " controller_id=" + std::to_string(e.controller_id) +
                   " control_flags=" + std::to_string(e.control_flags) + ">";
        });

    nb::class_<DeviceInfoChanged>(m, "DeviceInfoChanged")
        .def_prop_ro(
            "session", [](const DeviceInfoChanged& e) { return e.session; })
        .def("__repr__",
             [](const DeviceInfoChanged&) { return "<DeviceInfoChanged>"; });

    nb::class_<VariableDefinitionsChanged>(m, "VariableDefinitionsChanged")
        .def_prop_ro(
            "session",
            [](const VariableDefinitionsChanged& e) { return e.session; })
        .def("__repr__", [](const VariableDefinitionsChanged&) {
            return "<VariableDefinitionsChanged>";
        });

    nb::class_<TelemetryDefinitionsChanged>(m, "TelemetryDefinitionsChanged")
        .def_prop_ro(
            "session",
            [](const TelemetryDefinitionsChanged& e) { return e.session; })
        .def("__repr__", [](const TelemetryDefinitionsChanged&) {
            return "<TelemetryDefinitionsChanged>";
        });

    nb::class_<SimDataChanged>(m, "SimDataChanged")
        .def_prop_ro(
            "session", [](const SimDataChanged& e) { return e.session; })
        .def("__repr__",
             [](const SimDataChanged&) { return "<SimDataChanged>"; });
}
