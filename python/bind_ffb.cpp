#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>

#include <string>

#include <sc-api/core/device.h>
#include <sc-api/core/ffb.h>
#include <sc-api/core/session.h>
#include <sc-api/core/time.h>

namespace nb = nanobind;

using sc_api::core::Clock;
using sc_api::core::DeviceSessionId;
using sc_api::core::FfbPipeline;
using sc_api::core::FilterType;
using sc_api::core::InterpolationType;
using sc_api::core::OffsetType;
using sc_api::core::PipelineConfig;
using sc_api::core::Session;

// Proxy type for Clock static methods
struct PyClock {};

void bind_ffb(nb::module_& m) {
    // --- PipelineConfig ---

    nb::class_<PipelineConfig>(m, "PipelineConfig")
        .def(
            "__init__",
            [](PipelineConfig* self, OffsetType offset_type,
               InterpolationType interpolation_type, float gain,
               FilterType filter_type, float filter_parameter) {
                new (self) PipelineConfig{offset_type, interpolation_type,
                                          gain, filter_type, filter_parameter};
            },
            nb::arg("offset_type"),
            nb::arg("interpolation_type") = InterpolationType::linear,
            nb::arg("gain") = 1.0f,
            nb::arg("filter_type") = FilterType::none,
            nb::arg("filter_parameter") = 1.0f)
        .def_prop_ro("offset_type",
                      [](const PipelineConfig& self) { return self.offset_type; })
        .def_prop_ro("interpolation_type",
                      [](const PipelineConfig& self) { return self.interpolation_type; })
        .def_prop_ro("gain",
                      [](const PipelineConfig& self) { return self.gain; })
        .def_prop_ro("filter_type",
                      [](const PipelineConfig& self) { return self.filter_type; })
        .def_prop_ro("filter_parameter",
                      [](const PipelineConfig& self) { return self.filter_parameter; })
        .def("__eq__",
             [](const PipelineConfig& a, const PipelineConfig& b) {
                 return a == b;
             })
        .def("__repr__", [](const PipelineConfig& self) {
            auto enum_name = [](auto val) {
                return nb::cast<std::string>(nb::repr(nb::cast(val)));
            };
            return "<PipelineConfig offset_type=" + enum_name(self.offset_type) +
                   " interpolation_type=" + enum_name(self.interpolation_type) +
                   " gain=" + std::to_string(self.gain) +
                   " filter_type=" + enum_name(self.filter_type) +
                   " filter_parameter=" + std::to_string(self.filter_parameter) + ">";
        });

    // --- FfbPipeline ---

    nb::class_<FfbPipeline>(m, "FfbPipeline")
        .def(
            "__init__",
            [](FfbPipeline* self, const std::shared_ptr<Session>& session,
               DeviceSessionId device) {
                new (self) FfbPipeline(session, device);
            },
            nb::arg("session"), nb::arg("device"))
        .def(
            "configure",
            [](FfbPipeline& self, const PipelineConfig& config) {
                nb::gil_scoped_release release;
                return self.configure(config);
            },
            nb::arg("config"))
        .def(
            "generate_effect",
            [](FfbPipeline& self, int64_t start_ns, int64_t sample_duration_ns,
               const nb::ndarray<float, nb::ndim<1>>& samples) {
                auto start = Clock::time_point(Clock::duration(start_ns));
                auto sample_time = Clock::duration(sample_duration_ns);
                return self.generateEffect(start, sample_time,
                                           samples.data(), static_cast<unsigned>(samples.shape(0)));
            },
            nb::arg("start_ns"), nb::arg("sample_duration_ns"),
            nb::arg("samples"))
        .def("stop", &FfbPipeline::stop)
        .def(
            "remove",
            [](FfbPipeline& self) {
                nb::gil_scoped_release release;
                return self.remove();
            })
        .def_prop_ro("is_active", &FfbPipeline::isActive)
        .def_prop_ro("config", &FfbPipeline::getConfig)
        .def_prop_ro("device", &FfbPipeline::getDevice)
        .def(
            "__enter__",
            [](FfbPipeline& self) -> FfbPipeline& { return self; },
            nb::rv_policy::none)
        .def("__exit__",
             [](FfbPipeline& self, nb::args /*unused*/) {  // NOLINT(performance-unnecessary-value-param)
                 self.stop();
                 nb::gil_scoped_release release;
                 self.remove();
             })
        .def("__repr__", [](FfbPipeline& self) {
            return "<FfbPipeline device=" +
                   std::to_string(self.getDevice().id) +
                   " active=" + (self.isActive() ? "True" : "False") + ">";
        });

    // --- Clock ---

    nb::class_<PyClock>(m, "Clock")
        .def_static("now_ns", []() -> int64_t {
            return Clock::now().time_since_epoch().count();
        });
}
