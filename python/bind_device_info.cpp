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

    nb::class_<VariableRef>(m, "VariableRef",
                             "Reference to a variable on a specific device.")
        .def_prop_ro("device_session_id",
                      [](const VariableRef& self) { return self.device_session_id; },
                      "Session ID of the device that owns this variable.")
        .def_prop_ro("id",
                      [](const VariableRef& self) { return std::string(self.id); },
                      "Variable identifier string.")
        .def("__repr__", [](const VariableRef& self) {
            return "<VariableRef device=" + std::to_string(self.device_session_id.id) +
                   " id='" + std::string(self.id) + "'>";
        });

    // --- InputMapping ---

    nb::class_<InputMapping>(m, "InputMapping",
                              "Maps an input channel to a specific device.")
        .def_prop_ro("device_id",
                      [](const InputMapping& self) { return self.device_id; },
                      "Session ID of the device this input is mapped to.")
        .def_prop_ro("input_id",
                      [](const InputMapping& self) { return std::string(self.input_id); },
                      "Identifier of the input on the target device.")
        .def("__repr__", [](const InputMapping& self) {
            return "<InputMapping device=" + std::to_string(self.device_id.id) +
                   " input='" + std::string(self.input_id) + "'>";
        });

    // --- Control ---

    nb::class_<Control>(m, "Control",
                         "A physical control on a device, such as a pedal, button, or wheel.")
        .def_prop_ro("id",
                      [](const Control& self) { return std::string(self.id); },
                      "Unique identifier for this control.")
        .def_prop_ro("parent_id",
                      [](const Control& self) { return std::string(self.parent_id); },
                      "Identifier of the parent control, or an empty string if this is a top-level control.")
        .def_prop_ro("name",
                      [](const Control& self) { return std::string(self.name); },
                      "Human-readable name of this control.")
        .def_prop_ro("type", [](const Control& self) { return self.type; },
                      "Type classification of this control.")
        .def("__repr__", [](const Control& self) {
            return "<Control id='" + std::string(self.id) + "' name='" +
                   std::string(self.name) + "'>";
        });

    // --- Input ---

    nb::class_<Input>(m, "Input",
                       "An input channel of a control, representing one measurable signal.")
        .def_prop_ro("id",
                      [](const Input& self) { return std::string(self.id); },
                      "Unique identifier for this input.")
        .def_prop_ro("control",
                      [](const Input& self) { return std::string(self.control); },
                      "Identifier of the parent control that owns this input.")
        .def_prop_ro("type", [](const Input& self) { return self.type; },
                      "Type classification of this input.")
        .def_prop_ro("role", [](const Input& self) { return self.role; },
                      "Functional role of this input (e.g. throttle, brake).")
        .def_prop_ro("variable", [](const Input& self) { return self.variable; },
                      "Reference to the variable that carries this input's real-time value.")
        .def_prop_ro("range_begin", [](const Input& self) { return self.range_begin; },
                      "Start of the raw value range reported by this input.")
        .def_prop_ro("range_end", [](const Input& self) { return self.range_end; },
                      "End of the raw value range reported by this input.")
        .def("__repr__", [](const Input& self) {
            return "<Input id='" + std::string(self.id) + "'>";
        });

    // --- Feedback ---

    nb::class_<Feedback>(m, "Feedback",
                          "An output or feedback channel on a control.")
        .def_prop_ro("id",
                      [](const Feedback& self) { return std::string(self.id); },
                      "Unique identifier for this feedback.")
        .def_prop_ro("control",
                      [](const Feedback& self) { return std::string(self.control); },
                      "Identifier of the parent control that owns this feedback.")
        .def_prop_ro("type", [](const Feedback& self) { return self.type; },
                      "Type classification of this feedback.")
        .def("__repr__", [](const Feedback& self) {
            return "<Feedback id='" + std::string(self.id) + "'>";
        });

    // --- RgbLightFeedback ---

    nb::class_<RgbLightFeedback>(m, "RgbLightFeedback",
                                   "Specialized feedback descriptor for an RGB LED on a device.")
        .def_prop_ro("id",
                      [](const RgbLightFeedback& self) { return std::string(self.id); },
                      "Unique identifier for this RGB light feedback.")
        .def_prop_ro("control",
                      [](const RgbLightFeedback& self) { return std::string(self.control); },
                      "Identifier of the parent control that owns this RGB light.")
        .def_prop_ro("index", [](const RgbLightFeedback& self) { return self.index; },
                      "Zero-based index of this LED among all RGB lights on the device.")
        .def_prop_ro("is_valid", [](const RgbLightFeedback& self) { return self.isValid(); },
                      "True if this object was successfully constructed from a compatible Feedback.")
        .def_static("from_feedback", &RgbLightFeedback::fromFeedback, nb::arg("feedback"),
                    "Construct an RgbLightFeedback from a generic Feedback.\n\n"
                    "Always check ``is_valid`` on the returned object; it will be False if\n"
                    "the given feedback is not an RGB light feedback.")
        .def("__repr__", [](const RgbLightFeedback& self) {
            return "<RgbLightFeedback id='" + std::string(self.id) +
                   "' index=" + std::to_string(self.index) + ">";
        });

    // --- UsbDeviceInfo ---

    nb::class_<UsbDeviceInfo>(m, "UsbDeviceInfo",
                               "USB HID information for a connected device.")
        .def_prop_ro("hid_device_path",
                      [](const UsbDeviceInfo& self) { return self.hid_device_path; },
                      "System path to the HID device.")
        .def_prop_ro("pid", [](const UsbDeviceInfo& self) { return self.pid; },
                      "USB product ID.")
        .def_prop_ro("vid", [](const UsbDeviceInfo& self) { return self.vid; },
                      "USB vendor ID.")
        .def("__repr__", [](const UsbDeviceInfo& self) {
            return "<UsbDeviceInfo pid=" + std::to_string(self.pid) +
                   " vid=" + std::to_string(self.vid) + ">";
        });

    // --- HidAxisInput ---

    nb::class_<HidAxisInput>(m, "HidAxisInput",
                              "HID axis mapping information for one axis exposed by the device.")
        .def_prop_ro("role", [](const HidAxisInput& self) { return self.role; },
                      "Functional role of this HID axis.")
        .def_prop_ro("range_low", [](const HidAxisInput& self) { return self.range_low; },
                      "Minimum raw HID value of this axis.")
        .def_prop_ro("range_high", [](const HidAxisInput& self) { return self.range_high; },
                      "Maximum raw HID value of this axis.")
        .def_prop_ro("mappings",
                      [](const HidAxisInput& self) { return self.mappings; },
                      "List of InputMapping entries that feed into this HID axis.")
        .def("__repr__", [](const HidAxisInput& self) {
            return "<HidAxisInput mappings=" + std::to_string(self.mappings.size()) + ">";
        });

    // --- HidButtonInput ---

    nb::class_<HidButtonInput>(m, "HidButtonInput",
                                "HID button mapping information for one button exposed by the device.")
        .def_prop_ro("role", [](const HidButtonInput& self) { return self.role; },
                      "Functional role of this HID button.")
        .def_prop_ro("mappings",
                      [](const HidButtonInput& self) { return self.mappings; },
                      "List of InputMapping entries that feed into this HID button.")
        .def("__repr__", [](const HidButtonInput& self) {
            return "<HidButtonInput mappings=" + std::to_string(self.mappings.size()) + ">";
        });

    // --- DeviceInfo (held via shared_ptr<const DeviceInfo>) ---

    nb::class_<DeviceInfo>(m, "DeviceInfo",
                            "Complete device information for one connected Simucube device.")
        .def_prop_ro("uid",
                      [](const DeviceInfo& self) { return std::string(self.getUid()); },
                      "Persistent unique identifier; survives reconnects and session restarts.")
        .def_prop_ro("session_id",
                      [](const DeviceInfo& self) { return self.getSessionId(); },
                      "Per-session device identifier; may change between sessions.")
        .def_prop_ro("product_id",
                      [](const DeviceInfo& self) { return std::string(self.getProductId()); },
                      "Product identifier string.")
        .def_prop_ro("product_name",
                      [](const DeviceInfo& self) { return std::string(self.getProductName()); },
                      "Human-readable product name.")
        .def_prop_ro("manufacturer_id",
                      [](const DeviceInfo& self) { return std::string(self.getManufacturerId()); },
                      "Manufacturer identifier string.")
        .def_prop_ro("manufacturer_name",
                      [](const DeviceInfo& self) { return std::string(self.getManufacturerName()); },
                      "Human-readable manufacturer name.")
        .def_prop_ro("role", [](const DeviceInfo& self) { return self.getRole(); },
                      "Role of this device in the device hierarchy (e.g. wheelbase, wheel, pedals).")
        .def_prop_ro("parent_session_id",
                      [](const DeviceInfo& self) -> nb::object {
                          auto id = self.getParentSessionId();
                          if (!id) return nb::none();
                          return nb::cast(id);
                      },
                      "Session ID of the parent device, or ``None`` if this is a root device.")
        .def_prop_ro("is_valid", [](const DeviceInfo& self) { return self.isValid(); },
                      "True if this device info was successfully parsed and represents a live device.")
        .def_prop_ro("controls",
                      [](const DeviceInfo& self) {
                          return std::vector<Control>(self.getControls().begin(),
                                                     self.getControls().end());
                      },
                      "List of all physical controls on this device.")
        .def_prop_ro("inputs",
                      [](const DeviceInfo& self) {
                          return std::vector<Input>(self.getInputs().begin(),
                                                   self.getInputs().end());
                      },
                      "List of all input channels on this device.")
        .def_prop_ro("feedbacks",
                      [](const DeviceInfo& self) {
                          return std::vector<Feedback>(self.getFeedbacks().begin(),
                                                      self.getFeedbacks().end());
                      },
                      "List of all feedback/output channels on this device.")
        .def_prop_ro("rgb_lights",
                      [](const DeviceInfo& self) { return self.getRgbLights(); },
                      "List of RGB LED feedback descriptors on this device.")
        .def_prop_ro("hid_axes",
                      [](const DeviceInfo& self) { return self.getHidAxisInput(); },
                      "List of HID axis mappings exposed by this device.")
        .def_prop_ro("hid_buttons",
                      [](const DeviceInfo& self) { return self.getHidButtonInput(); },
                      "List of HID button mappings exposed by this device.")
        .def_prop_ro("usb_info",
                      [](const DeviceInfo& self) { return self.getUsbInfo(); },
                      "USB HID information for this device.")
        .def("__repr__", [](const DeviceInfo& self) {
            return "<DeviceInfo uid='" + std::string(self.getUid()) + "' product_name='" +
                   std::string(self.getProductName()) + "' role=" +
                   std::string(sc_api::core::device_info::toString(self.getRole())) + ">";
        });

    // --- FullInfo (held via shared_ptr<FullInfo>) ---

    nb::class_<FullInfo>(m, "FullInfo",
                          "Snapshot of all connected devices, parsed from shared memory.")
        .def("__len__", &FullInfo::getDeviceCount)
        .def(
            "__iter__",
            [](const std::shared_ptr<FullInfo>& self) {
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
            nb::arg("uid"),
            "Return the DeviceInfo with the given persistent UID, or None if not found.")
        .def(
            "get_by_session_id",
            [](const FullInfo& self, DeviceSessionId id) -> nb::object {
                auto ptr = self.getBySessionId(id);
                if (!ptr) return nb::none();
                return nb::cast(ptr);
            },
            nb::arg("id"),
            "Return the DeviceInfo with the given session ID, or None if not found.")
        .def(
            "find_first",
            [](const FullInfo& self,
               const std::function<bool(const DeviceInfo&)>& predicate) -> nb::object {
                auto ptr = self.findFirstByFilter(predicate);
                if (!ptr) return nb::none();
                return nb::cast(ptr);
            },
            nb::arg("predicate"),
            "Return the first DeviceInfo for which predicate returns True, or None if no match.")
        .def(
            "find_all",
            [](const FullInfo& self,
               const std::function<bool(const DeviceInfo&)>& predicate) {
                return self.findAllByFilter(predicate);
            },
            nb::arg("predicate"),
            "Return a list of all DeviceInfo entries for which predicate returns True.")
        .def_prop_ro("revision", &FullInfo::getRevisionNumber,
                      "Monotonically increasing counter; increments each time the device list changes.")
        .def("__repr__", [](const FullInfo& self) {
            return "<FullInfo devices=" + std::to_string(self.getDeviceCount()) +
                   " revision=" + std::to_string(self.getRevisionNumber()) + ">";
        });
}
