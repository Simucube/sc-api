#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <string>
#include <vector>

#include <sc-api/core/device.h>
#include <sc-api/core/led_control.h>
#include <sc-api/core/session.h>

namespace nb = nanobind;

using sc_api::core::DeviceSessionId;
using sc_api::core::LedControl;
using sc_api::core::RgbColor;
using sc_api::core::Session;

void bind_led_control(nb::module_& m) {
    nb::class_<LedControl>(m, "LedControl")
        .def(
            "__init__",
            [](LedControl* self, const std::shared_ptr<Session>& session,
               DeviceSessionId device) {
                new (self) LedControl(session, device);
            },
            nb::arg("session"), nb::arg("device"))
        .def(
            "set_leds",
            [](LedControl& self, const nb::list& indices_list,
               const nb::list& colors_list) -> bool {
                if (nb::len(indices_list) != nb::len(colors_list)) {
                    throw nb::value_error(
                        "indices and colors must have equal length");
                }
                std::vector<uint32_t> indices;
                indices.reserve(nb::len(indices_list));
                for (nb::handle h : indices_list) {
                    indices.push_back(nb::cast<uint32_t>(h));
                }
                std::vector<RgbColor> colors;
                colors.reserve(nb::len(colors_list));
                for (nb::handle h : colors_list) {
                    colors.push_back(nb::cast<RgbColor>(h));
                }
                return self.setControlledLedsAndColors(
                    indices.data(), colors.data(),
                    static_cast<unsigned>(indices.size()));
            },
            nb::arg("indices"), nb::arg("colors"))
        .def("clear", &LedControl::clearLeds)
        .def("release", &LedControl::releaseControl)
        .def_prop_ro("device", &LedControl::getDevice)
        .def_prop_ro("session", &LedControl::getSession)
        .def(
            "__enter__",
            [](LedControl& self) -> LedControl& { return self; },
            nb::rv_policy::none)
        .def("__exit__",
             [](LedControl& self, nb::args /*unused*/) { self.releaseControl(); })  // NOLINT(performance-unnecessary-value-param)
        .def("__repr__", [](const LedControl& self) {
            return "<LedControl device=" +
                   std::to_string(self.getDevice().id) + ">";
        });
}
