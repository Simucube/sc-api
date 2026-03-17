#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/function.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <string>
#include <vector>

#include <sc-api/core/device_info.h>

namespace nb = nanobind;

using sc_api::core::DeviceSessionId;
using sc_api::core::device_info::Control;
using sc_api::core::device_info::DeviceInfo;
using sc_api::core::device_info::DeviceInfoPtr;
using sc_api::core::device_info::Feedback;
using sc_api::core::device_info::FullInfo;
using sc_api::core::device_info::HidAxisInput;
using sc_api::core::device_info::HidButtonInput;
using sc_api::core::device_info::Input;
using sc_api::core::device_info::InputMapping;
using sc_api::core::device_info::RgbLightFeedback;
using sc_api::core::device_info::UsbDeviceInfo;
using sc_api::core::device_info::VariableRef;

void bind_device_info(nb::module_& m) {
    // --- VariableRef ---

    nb::class_<VariableRef>(m, "VariableRef")
        .def_prop_ro("device_session_id",
                      [](const VariableRef& self) { return self.device_session_id; })
        .def_prop_ro("id",
                      [](const VariableRef& self) { return std::string(self.id); })
        .def("__repr__", [](const VariableRef& self) {
            return "<VariableRef device=" + std::to_string(self.device_session_id.id) +
                   " id='" + std::string(self.id) + "'>";
        });

    // --- InputMapping ---

    nb::class_<InputMapping>(m, "InputMapping")
        .def_prop_ro("device_id",
                      [](const InputMapping& self) { return self.device_id; })
        .def_prop_ro("input_id",
                      [](const InputMapping& self) { return std::string(self.input_id); })
        .def("__repr__", [](const InputMapping& self) {
            return "<InputMapping device=" + std::to_string(self.device_id.id) +
                   " input='" + std::string(self.input_id) + "'>";
        });

    // --- Control ---

    nb::class_<Control>(m, "Control")
        .def_prop_ro("id",
                      [](const Control& self) { return std::string(self.id); })
        .def_prop_ro("parent_id",
                      [](const Control& self) { return std::string(self.parent_id); })
        .def_prop_ro("name",
                      [](const Control& self) { return std::string(self.name); })
        .def_prop_ro("type", [](const Control& self) { return self.type; })
        .def("__repr__", [](const Control& self) {
            return "<Control id='" + std::string(self.id) + "' name='" +
                   std::string(self.name) + "'>";
        });

    // --- Input ---

    nb::class_<Input>(m, "Input")
        .def_prop_ro("id",
                      [](const Input& self) { return std::string(self.id); })
        .def_prop_ro("control",
                      [](const Input& self) { return std::string(self.control); })
        .def_prop_ro("type", [](const Input& self) { return self.type; })
        .def_prop_ro("role", [](const Input& self) { return self.role; })
        .def_prop_ro("variable", [](const Input& self) { return self.variable; })
        .def_prop_ro("range_begin", [](const Input& self) { return self.range_begin; })
        .def_prop_ro("range_end", [](const Input& self) { return self.range_end; })
        .def("__repr__", [](const Input& self) {
            return "<Input id='" + std::string(self.id) + "'>";
        });

    // --- Feedback ---

    nb::class_<Feedback>(m, "Feedback")
        .def_prop_ro("id",
                      [](const Feedback& self) { return std::string(self.id); })
        .def_prop_ro("control",
                      [](const Feedback& self) { return std::string(self.control); })
        .def_prop_ro("type", [](const Feedback& self) { return self.type; })
        .def("__repr__", [](const Feedback& self) {
            return "<Feedback id='" + std::string(self.id) + "'>";
        });

    // --- RgbLightFeedback ---

    nb::class_<RgbLightFeedback>(m, "RgbLightFeedback")
        .def_prop_ro("id",
                      [](const RgbLightFeedback& self) { return std::string(self.id); })
        .def_prop_ro("control",
                      [](const RgbLightFeedback& self) { return std::string(self.control); })
        .def_prop_ro("index", [](const RgbLightFeedback& self) { return self.index; })
        .def_prop_ro("is_valid", [](const RgbLightFeedback& self) { return self.isValid(); })
        .def_static("from_feedback", &RgbLightFeedback::fromFeedback, nb::arg("feedback"))
        .def("__repr__", [](const RgbLightFeedback& self) {
            return "<RgbLightFeedback id='" + std::string(self.id) +
                   "' index=" + std::to_string(self.index) + ">";
        });

    // --- UsbDeviceInfo ---

    nb::class_<UsbDeviceInfo>(m, "UsbDeviceInfo")
        .def_prop_ro("hid_device_path",
                      [](const UsbDeviceInfo& self) { return self.hid_device_path; })
        .def_prop_ro("pid", [](const UsbDeviceInfo& self) { return self.pid; })
        .def_prop_ro("vid", [](const UsbDeviceInfo& self) { return self.vid; })
        .def("__repr__", [](const UsbDeviceInfo& self) {
            return "<UsbDeviceInfo pid=" + std::to_string(self.pid) +
                   " vid=" + std::to_string(self.vid) + ">";
        });

    // --- HidAxisInput ---

    nb::class_<HidAxisInput>(m, "HidAxisInput")
        .def_prop_ro("role", [](const HidAxisInput& self) { return self.role; })
        .def_prop_ro("range_low", [](const HidAxisInput& self) { return self.range_low; })
        .def_prop_ro("range_high", [](const HidAxisInput& self) { return self.range_high; })
        .def_prop_ro("mappings",
                      [](const HidAxisInput& self) { return self.mappings; })
        .def("__repr__", [](const HidAxisInput& self) {
            return "<HidAxisInput mappings=" + std::to_string(self.mappings.size()) + ">";
        });

    // --- HidButtonInput ---

    nb::class_<HidButtonInput>(m, "HidButtonInput")
        .def_prop_ro("role", [](const HidButtonInput& self) { return self.role; })
        .def_prop_ro("mappings",
                      [](const HidButtonInput& self) { return self.mappings; })
        .def("__repr__", [](const HidButtonInput& self) {
            return "<HidButtonInput mappings=" + std::to_string(self.mappings.size()) + ">";
        });

    // --- DeviceInfo (held via shared_ptr<const DeviceInfo>) ---

    nb::class_<DeviceInfo>(m, "DeviceInfo")
        .def_prop_ro("uid",
                      [](const DeviceInfo& self) { return std::string(self.getUid()); })
        .def_prop_ro("session_id",
                      [](const DeviceInfo& self) { return self.getSessionId(); })
        .def_prop_ro("product_id",
                      [](const DeviceInfo& self) { return std::string(self.getProductId()); })
        .def_prop_ro("product_name",
                      [](const DeviceInfo& self) { return std::string(self.getProductName()); })
        .def_prop_ro("manufacturer_id",
                      [](const DeviceInfo& self) { return std::string(self.getManufacturerId()); })
        .def_prop_ro("manufacturer_name",
                      [](const DeviceInfo& self) { return std::string(self.getManufacturerName()); })
        .def_prop_ro("role", [](const DeviceInfo& self) { return self.getRole(); })
        .def_prop_ro("parent_session_id",
                      [](const DeviceInfo& self) { return self.getParentSessionId(); })
        .def_prop_ro("is_valid", [](const DeviceInfo& self) { return self.isValid(); })
        .def_prop_ro("controls",
                      [](const DeviceInfo& self) {
                          return std::vector<Control>(self.getControls().begin(),
                                                     self.getControls().end());
                      })
        .def_prop_ro("inputs",
                      [](const DeviceInfo& self) {
                          return std::vector<Input>(self.getInputs().begin(),
                                                   self.getInputs().end());
                      })
        .def_prop_ro("feedbacks",
                      [](const DeviceInfo& self) {
                          return std::vector<Feedback>(self.getFeedbacks().begin(),
                                                      self.getFeedbacks().end());
                      })
        .def_prop_ro("rgb_lights",
                      [](const DeviceInfo& self) { return self.getRgbLights(); })
        .def_prop_ro("hid_axes",
                      [](const DeviceInfo& self) { return self.getHidAxisInput(); })
        .def_prop_ro("hid_buttons",
                      [](const DeviceInfo& self) { return self.getHidButtonInput(); })
        .def_prop_ro("usb_info",
                      [](const DeviceInfo& self) { return self.getUsbInfo(); })
        .def("__repr__", [](const DeviceInfo& self) {
            return "<DeviceInfo uid='" + std::string(self.getUid()) + "' product_name='" +
                   std::string(self.getProductName()) + "' role=" +
                   std::string(sc_api::core::device_info::toString(self.getRole())) + ">";
        });

    // --- FullInfo (held via shared_ptr<FullInfo>) ---

    nb::class_<FullInfo>(m, "FullInfo")
        .def("__len__", &FullInfo::getDeviceCount)
        .def(
            "__iter__",
            [](std::shared_ptr<FullInfo> self) {
                return nb::make_iterator(nb::type<FullInfo>(), "FullInfoIterator",
                                         self->begin(), self->end());
            },
            nb::keep_alive<0, 1>())
        .def(
            "__getitem__",
            [](const FullInfo& self, int index) -> DeviceInfoPtr {
                int size = static_cast<int>(self.getDeviceCount());
                if (index < 0) index += size;
                if (index < 0 || index >= size) {
                    throw nb::index_error("FullInfo index out of range");
                }
                return self.getByIndex(static_cast<std::size_t>(index));
            },
            nb::arg("index"))
        .def(
            "get_by_uid",
            [](const FullInfo& self, const std::string& uid) -> nb::object {
                auto ptr = self.getByUid(uid);
                if (!ptr) return nb::none();
                return nb::cast(ptr);
            },
            nb::arg("uid"))
        .def(
            "get_by_session_id",
            [](const FullInfo& self, DeviceSessionId id) -> nb::object {
                auto ptr = self.getBySessionId(id);
                if (!ptr) return nb::none();
                return nb::cast(ptr);
            },
            nb::arg("id"))
        .def(
            "find_first",
            [](const FullInfo& self,
               const std::function<bool(const DeviceInfo&)>& predicate) -> nb::object {
                auto ptr = self.findFirstByFilter(predicate);
                if (!ptr) return nb::none();
                return nb::cast(ptr);
            },
            nb::arg("predicate"))
        .def(
            "find_all",
            [](const FullInfo& self,
               const std::function<bool(const DeviceInfo&)>& predicate) {
                return self.findAllByFilter(predicate);
            },
            nb::arg("predicate"))
        .def_prop_ro("revision", &FullInfo::getRevisionNumber)
        .def("__repr__", [](const FullInfo& self) {
            return "<FullInfo devices=" + std::to_string(self.getDeviceCount()) +
                   " revision=" + std::to_string(self.getRevisionNumber()) + ">";
        });
}
