#include <nanobind/nanobind.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>
#include <sc-api/device.h>
#include <sc-api/led_control.h>
#include <sc-api/session.h>

#include <string>
#include <vector>

namespace nb = nanobind;

using sc_api::DeviceSessionId;
using sc_api::LedControl;
using sc_api::RgbColor;
using sc_api::Session;

void bind_led_control(nb::module_& m) {
    nb::class_<LedControl>(m, "LedControl",
                           "Controls RGB LEDs on a device. Requires control session state. "
                           "Use as a context manager to automatically release LED control on exit.")
        .def(
            "__init__",
            [](LedControl* self, const std::shared_ptr<Session>& session, DeviceSessionId device) {
                new (self) LedControl(session, device);
            },
            nb::arg("session"), nb::arg("device"), "Acquire LED control for the given session and device.")
        .def(
            "set_leds",
            [](LedControl& self, const nb::list& indices_list, const nb::list& colors_list) -> bool {
                if (nb::len(indices_list) != nb::len(colors_list)) {
                    throw nb::value_error("indices and colors must have equal length");
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
                return self.setControlledLedsAndColors(indices.data(), colors.data(),
                                                       static_cast<unsigned>(indices.size()));
            },
            nb::arg("indices"), nb::arg("colors"),
            "Set colors for a list of LEDs.\n\n"
            ":param indices: List of LED indices (from RgbLightFeedback.index).\n"
            ":param colors: List of RgbColor values, one per index. Must be the same length as indices.\n"
            ":returns: True on success. Raises ValueError if lengths differ.")
        .def("clear", &LedControl::clearLeds, "Turn off all LEDs currently controlled by this instance.")
        .def("release", &LedControl::releaseControl,
             "Release LED control, allowing other controllers to take over. "
             "LEDs are not explicitly cleared; call clear() first if needed.")
        .def_prop_ro("device", &LedControl::getDevice, "The device session ID this controller is attached to.")
        .def_prop_ro("session", &LedControl::getSession, "The session this controller belongs to.")
        .def(
            "__enter__", [](LedControl& self) -> LedControl& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](LedControl& self, nb::args /*unused*/) {
                 self.releaseControl();
             })  // NOLINT(performance-unnecessary-value-param)
        .def("__repr__",
             [](const LedControl& self) { return "<LedControl device=" + std::to_string(self.getDevice().id) + ">"; });
}
