#include <nanobind/nanobind.h>

namespace nb = nanobind;

void bind_enums(nb::module_& m);
void bind_exceptions(nb::module_& m);
void bind_events(nb::module_& m);
void bind_sim_data(nb::module_& m);
void bind_api(nb::module_& m);
void bind_device_info(nb::module_& m);
void bind_variables(nb::module_& m);
void bind_telemetry(nb::module_& m);
void bind_ffb(nb::module_& m);
void bind_led_control(nb::module_& m);

NB_MODULE(_native, m) {
    m.doc()               = "Simucube API native bindings";
    m.attr("__version__") = SC_API_VERSION;

    bind_enums(m);
    bind_exceptions(m);
    bind_events(m);
    bind_sim_data(m);
    bind_api(m);
    bind_device_info(m);
    bind_variables(m);
    bind_telemetry(m);
    bind_ffb(m);
    bind_led_control(m);
}
