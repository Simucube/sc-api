#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include <sc-api/core/session.h>
#include <sc-api/core/telemetry.h>
#include <sc-api/core/type.h>

#include "bind_exceptions.h"

namespace nb = nanobind;

using sc_api::core::ActionResult;
using sc_api::core::Session;
using sc_api::core::Telemetry;
using sc_api::core::TelemetryBase;
using sc_api::core::TelemetryDefinition;
using sc_api::core::TelemetryDefinitions;
using sc_api::core::TelemetryUpdateGroup;
using sc_api::core::Type;

// --- TelemetryGroupHelper: owns Telemetry<T> handles and wraps TelemetryUpdateGroup ---

class TelemetryGroupHelper {
    using TelemetryVariant = std::variant<
        std::unique_ptr<Telemetry<bool>>,
        std::unique_ptr<Telemetry<int8_t>>,
        std::unique_ptr<Telemetry<uint8_t>>,
        std::unique_ptr<Telemetry<int16_t>>,
        std::unique_ptr<Telemetry<uint16_t>>,
        std::unique_ptr<Telemetry<int32_t>>,
        std::unique_ptr<Telemetry<uint32_t>>,
        std::unique_ptr<Telemetry<int64_t>>,
        std::unique_ptr<Telemetry<float>>,
        std::unique_ptr<Telemetry<double>>>;

    TelemetryDefinitions definitions_;
    std::unordered_map<std::string, TelemetryVariant> telemetries_;  // declared BEFORE group_
    TelemetryUpdateGroup group_;  // destroyed first (reverse decl order)
    bool dirty_ = true;
    static inline std::atomic<uint16_t> next_group_id_{0};

public:
    explicit TelemetryGroupHelper(const TelemetryDefinitions& defs)
        : definitions_(defs), group_(next_group_id_++) {}

    void setitem(const std::string& name, nb::object value) {
        const TelemetryDefinition* def = definitions_.find(name);
        if (!def) {
            throw nb::key_error(name.c_str());
        }

        auto it = telemetries_.find(name);
        if (it != telemetries_.end()) {
            // Update existing telemetry value
            std::visit([&value](auto& ptr) {
                using T = std::decay_t<decltype(ptr->getValue())>;
                ptr->setValue(nb::cast<T>(value));
            }, it->second);
            return;
        }

        // Create new Telemetry<T> based on definition type
        switch (def->type.getBaseType()) {
            case Type::boolean:
                telemetries_.emplace(name, std::make_unique<Telemetry<bool>>(name, nb::cast<bool>(value)));
                break;
            case Type::i8:
                telemetries_.emplace(name, std::make_unique<Telemetry<int8_t>>(name, nb::cast<int8_t>(value)));
                break;
            case Type::u8:
                telemetries_.emplace(name, std::make_unique<Telemetry<uint8_t>>(name, nb::cast<uint8_t>(value)));
                break;
            case Type::i16:
                telemetries_.emplace(name, std::make_unique<Telemetry<int16_t>>(name, nb::cast<int16_t>(value)));
                break;
            case Type::u16:
                telemetries_.emplace(name, std::make_unique<Telemetry<uint16_t>>(name, nb::cast<uint16_t>(value)));
                break;
            case Type::i32:
                telemetries_.emplace(name, std::make_unique<Telemetry<int32_t>>(name, nb::cast<int32_t>(value)));
                break;
            case Type::u32:
                telemetries_.emplace(name, std::make_unique<Telemetry<uint32_t>>(name, nb::cast<uint32_t>(value)));
                break;
            case Type::i64:
                telemetries_.emplace(name, std::make_unique<Telemetry<int64_t>>(name, nb::cast<int64_t>(value)));
                break;
            case Type::f32:
                telemetries_.emplace(name, std::make_unique<Telemetry<float>>(name, nb::cast<float>(value)));
                break;
            case Type::f64:
                telemetries_.emplace(name, std::make_unique<Telemetry<double>>(name, nb::cast<double>(value)));
                break;
            default:
                throw nb::type_error("Unsupported telemetry type");
        }
        dirty_ = true;
    }

    void delitem(const std::string& name) {
        auto it = telemetries_.find(name);
        if (it == telemetries_.end()) {
            throw nb::key_error(name.c_str());
        }
        telemetries_.erase(it);
        dirty_ = true;
    }

    void send() {
        if (dirty_) {
            std::vector<TelemetryBase*> ptrs;
            ptrs.reserve(telemetries_.size());
            for (auto& [key, var] : telemetries_) {
                std::visit([&ptrs](auto& ptr) {
                    ptrs.push_back(ptr.get());
                }, var);
            }
            group_.set(std::move(ptrs));
            group_.configure(definitions_);
            dirty_ = false;
        }
        ActionResult result = group_.send();
        if (result == ActionResult::failed) {
            throw_internal_error("TelemetryUpdateGroup::send() failed");
        }
    }

    nb::object getitem(const std::string& name) const {
        auto it = telemetries_.find(name);
        if (it == telemetries_.end()) {
            throw nb::key_error(name.c_str());
        }
        return std::visit(
            [](const auto& ptr) -> nb::object { return nb::cast(ptr->getValue()); },
            it->second);
    }

    bool contains(const std::string& name) const {
        return telemetries_.count(name) > 0;
    }

    size_t len() const { return telemetries_.size(); }

    std::vector<std::string> available_names() const {
        std::vector<std::string> names;
        names.reserve(definitions_.size());
        for (const auto& def : definitions_) {
            names.emplace_back(def.name);
        }
        return names;
    }

    uint16_t group_id() const { return group_.getId(); }
};

void bind_telemetry(nb::module_& m) {
    // --- TelemetryDefinition ---

    nb::class_<TelemetryDefinition>(m, "TelemetryDefinition")
        .def_prop_ro("name",
                      [](const TelemetryDefinition& self) { return self.name; })
        .def_prop_ro("type", [](const TelemetryDefinition& self) { return self.type; })
        .def_prop_ro("id", [](const TelemetryDefinition& self) { return self.id; })
        .def_prop_ro("flags", [](const TelemetryDefinition& self) { return self.flags; })
        .def_prop_ro("variable_idx",
                      [](const TelemetryDefinition& self) { return self.variable_idx; })
        .def("__repr__", [](const TelemetryDefinition& self) {
            return "<TelemetryDefinition name='" + self.name +
                   "' type=" + self.type.toString() + ">";
        });

    // --- TelemetryDefinitions ---

    nb::class_<TelemetryDefinitions>(m, "TelemetryDefinitions")
        .def("__len__", &TelemetryDefinitions::size)
        .def(
            "__contains__",
            [](const TelemetryDefinitions& self, const std::string& name) {
                return self.find(name) != nullptr;
            },
            nb::arg("name"))
        .def(
            "__iter__",
            [](const TelemetryDefinitions& self) {
                return nb::make_iterator(nb::type<TelemetryDefinitions>(),
                                         "TelemetryDefinitionsIterator",
                                         self.begin(), self.end());
            },
            nb::keep_alive<0, 1>())
        .def(
            "__getitem__",
            [](const TelemetryDefinitions& self, int index) -> const TelemetryDefinition& {
                int size = static_cast<int>(self.size());
                if (index < 0) index += size;
                if (index < 0 || index >= size) {
                    throw nb::index_error("TelemetryDefinitions index out of range");
                }
                return *(self.begin() + index);
            },
            nb::arg("index"), nb::rv_policy::reference_internal)
        .def(
            "find",
            [](const TelemetryDefinitions& self, const std::string& name,
               std::optional<Type> type) -> nb::object {
                const TelemetryDefinition* def =
                    type ? self.find(name, *type) : self.find(name);
                if (!def) return nb::none();
                return nb::cast(def, nb::rv_policy::reference_internal);
            },
            nb::arg("name"), nb::arg("type") = nb::none(),
            nb::keep_alive<0, 1>())
        .def_prop_ro("names",
                      [](const TelemetryDefinitions& self) {
                          std::vector<std::string> names;
                          names.reserve(self.size());
                          for (auto it = self.begin(); it != self.end(); ++it) {
                              names.emplace_back(it->name);
                          }
                          return names;
                      })
        .def_prop_ro("session",
                      [](const TelemetryDefinitions& self) -> nb::object {
                          auto s = self.getSession();
                          if (!s) return nb::none();
                          return nb::cast(s);
                      })
        .def("__repr__", [](const TelemetryDefinitions& self) {
            return "<TelemetryDefinitions size=" + std::to_string(self.size()) + ">";
        });

    // --- TelemetryUpdateGroup (via TelemetryGroupHelper) ---

    nb::class_<TelemetryGroupHelper>(m, "TelemetryUpdateGroup")
        .def(nb::init<TelemetryDefinitions>(), nb::arg("telemetry_definitions"))
        .def("__setitem__", &TelemetryGroupHelper::setitem)
        .def("__getitem__", &TelemetryGroupHelper::getitem)
        .def("__delitem__", &TelemetryGroupHelper::delitem)
        .def("send", &TelemetryGroupHelper::send)
        .def("__contains__", &TelemetryGroupHelper::contains)
        .def("__len__", &TelemetryGroupHelper::len)
        .def_prop_ro("available_names", &TelemetryGroupHelper::available_names)
        .def("__repr__", [](const TelemetryGroupHelper& self) {
            return "<TelemetryUpdateGroup size=" + std::to_string(self.len()) +
                   " group_id=" + std::to_string(self.group_id()) + ">";
        });
}
