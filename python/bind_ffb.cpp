#include <nanobind/nanobind.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <sc-api/device.h>
#include <sc-api/ffb.h>
#include <sc-api/session.h>
#include <sc-api/time.h>

#include <string>

namespace nb = nanobind;

using sc_api::Clock;
using sc_api::DeviceSessionId;
using sc_api::FfbPipeline;
using sc_api::FilterType;
using sc_api::InterpolationType;
using sc_api::OffsetType;
using sc_api::PipelineConfig;
using sc_api::Session;

// Proxy type for Clock static methods
struct PyClock {};

void bind_ffb(nb::module_& m) {
    // --- PipelineConfig ---

    nb::class_<PipelineConfig>(m, "PipelineConfig",
                               "Configuration for an FFB pipeline. "
                               "Set offset_type to match the device (torque_Nm/torque_relative for wheelbases, "
                               "force_N/force_relative/position_mm for pedals). "
                               "Must be applied via FfbPipeline.configure() before generating effects.")
        .def(
            "__init__",
            [](PipelineConfig* self, OffsetType offset_type, InterpolationType interpolation_type, float gain,
               FilterType filter_type, float filter_parameter) {
                new (self) PipelineConfig{offset_type, interpolation_type, gain, filter_type, filter_parameter};
            },
            nb::arg("offset_type"), nb::arg("interpolation_type") = InterpolationType::linear, nb::arg("gain") = 1.0f,
            nb::arg("filter_type") = FilterType::none, nb::arg("filter_parameter") = 1.0f,
            "Create a pipeline configuration.\n\n"
            ":param offset_type: Output unit type. Must match the target device category.\n"
            ":param interpolation_type: Sample interpolation mode (default: linear).\n"
            ":param gain: Output multiplier applied after interpolation (default: 1.0).\n"
            ":param filter_type: Optional output filter (default: none).\n"
            ":param filter_parameter: Filter parameter: cutoff frequency in Hz for low_pass, "
            "max change per second for slew_rate_limit (default: 1.0).")
        .def_prop_ro(
            "offset_type", [](const PipelineConfig& self) { return self.offset_type; },
            "Output unit type for this pipeline.")
        .def_prop_ro(
            "interpolation_type", [](const PipelineConfig& self) { return self.interpolation_type; },
            "Sample interpolation mode.")
        .def_prop_ro(
            "gain", [](const PipelineConfig& self) { return self.gain; },
            "Output multiplier applied to all effect samples.")
        .def_prop_ro(
            "filter_type", [](const PipelineConfig& self) { return self.filter_type; }, "Output filter type.")
        .def_prop_ro(
            "filter_parameter", [](const PipelineConfig& self) { return self.filter_parameter; },
            "Filter parameter. Meaning depends on filter_type: "
            "cutoff frequency in Hz for low_pass, max change per second for slew_rate_limit.")
        .def(
            "__eq__", [](const PipelineConfig& a, const PipelineConfig& b) { return a == b; }, nb::is_operator())
        .def("__repr__", [](const PipelineConfig& self) {
            auto enum_name = [](auto val) { return nb::cast<std::string>(nb::repr(nb::cast(val))); };
            return "<PipelineConfig offset_type=" + enum_name(self.offset_type) +
                   " interpolation_type=" + enum_name(self.interpolation_type) + " gain=" + std::to_string(self.gain) +
                   " filter_type=" + enum_name(self.filter_type) +
                   " filter_parameter=" + std::to_string(self.filter_parameter) + ">";
        });

    // --- FfbPipeline ---

    nb::class_<FfbPipeline>(m, "FfbPipeline",
                            "One FFB effect pipeline on a device. "
                            "Up to 4 pipelines can run concurrently per device. "
                            "Call configure() before generate_effect(). "
                            "Use as a context manager to automatically stop and remove the pipeline on exit.")
        .def(
            "__init__",
            [](FfbPipeline* self, const std::shared_ptr<Session>& session, DeviceSessionId device) {
                new (self) FfbPipeline(session, device);
            },
            nb::arg("session"), nb::arg("device"), "Create an FFB pipeline for the given session and device.")
        .def(
            "configure",
            [](FfbPipeline& self, const PipelineConfig& config) {
                nb::gil_scoped_release release;
                return self.configure(config);
            },
            nb::arg("config"),
            "Apply a pipeline configuration. Must be called before generate_effect(). "
            "Can be called again to reconfigure a running pipeline. Returns ActionResult.")
        .def(
            "generate_effect",
            [](FfbPipeline& self, int64_t start_ns, int64_t sample_duration_ns,
               const nb::ndarray<const float, nb::ndim<1>, nb::c_contig>& samples) {
                auto start       = Clock::time_point(Clock::duration(start_ns));
                auto sample_time = Clock::duration(sample_duration_ns);
                return self.generateEffect(start, sample_time, samples.data(), static_cast<unsigned>(samples.shape(0)));
            },
            nb::arg("start_ns"), nb::arg("sample_duration_ns"), nb::arg("samples"),
            "Schedule effect playback from a float32 sample array.\n\n"
            ":param start_ns: Absolute start time in nanoseconds (use Clock.now_ns() for immediate playback).\n"
            ":param sample_duration_ns: Duration of each sample in nanoseconds (e.g. 50000 for 20 kHz).\n"
            ":param samples: Contiguous 1D numpy float32 array of output values in the\n"
            "    pipeline's offset_type units. A strided array, such as a slice\n"
            "    with a step or a column of a 2D array, is rejected. Copy it first\n"
            "    with numpy.ascontiguousarray.\n"
            ":returns: ActionResult. configure() must have been called first.")
        .def("stop", &FfbPipeline::stop,
             "Stop the current effect immediately. The pipeline remains configured and can play a new effect.")
        .def(
            "remove",
            [](FfbPipeline& self) {
                nb::gil_scoped_release release;
                return self.remove();
            },
            "Remove this pipeline from the device (blocking). "
            "Call stop() first if an effect is playing. "
            "After removal the pipeline object must not be reused.")
        .def_prop_ro("is_active", &FfbPipeline::isActive, "True if an effect is currently playing on this pipeline.")
        .def_prop_ro("config", &FfbPipeline::getConfig,
                     "The PipelineConfig last applied via configure(), or None if not yet configured.")
        .def_prop_ro("device", &FfbPipeline::getDevice, "The device session ID this pipeline is attached to.")
        .def(
            "__enter__", [](FfbPipeline& self) -> FfbPipeline& { return self; }, nb::rv_policy::none)
        .def("__exit__",
             [](FfbPipeline& self, nb::args /*unused*/) {  // NOLINT(performance-unnecessary-value-param)
                 self.stop();
                 nb::gil_scoped_release release;
                 self.remove();
             })
        .def("__repr__", [](FfbPipeline& self) {
            return "<FfbPipeline device=" + std::to_string(self.getDevice().id) +
                   " active=" + (self.isActive() ? "True" : "False") + ">";
        });

    // --- Clock ---

    nb::class_<PyClock>(m, "Clock",
                        "Static utility for FFB timing. Provides the same time base used by the effect pipeline.")
        .def_static(
            "now_ns", []() -> int64_t { return Clock::now().time_since_epoch().count(); },
            "Return the current time in nanoseconds. "
            "Pass the result directly to FfbPipeline.generate_effect() as start_ns for immediate playback.");
}
