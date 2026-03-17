#include <nanobind/nanobind.h>
#include <nanobind/make_iterator.h>
#include <nanobind/ndarray.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <cstring>
#include <string>
#include <vector>

#include <sc-api/core/session.h>
#include <sc-api/core/type.h>
#include <sc-api/core/variables.h>

namespace nb = nanobind;

using sc_api::core::DeviceSessionId;
using sc_api::core::Session;
using sc_api::core::Type;
using sc_api::core::VariableDefinition;
using sc_api::core::VariableDefinitions;
using sc_api::core::k_invalid_device_session_id;

// --- read_value: read a scalar from shared memory ---

static nb::object read_bit(const VariableDefinition& var) {
    if (!var.value_ptr || !var.type.isBit()) {
        return nb::none();
    }

    unsigned bit_idx = var.type.getBitIndex();
    switch (var.type.getBaseType()) {
        case Type::u8: {
            auto val = *reinterpret_cast<const uint8_t*>(var.value_ptr);
            return nb::cast(static_cast<bool>((val >> bit_idx) & 1));
        }
        case Type::u16: {
            auto val = *reinterpret_cast<const uint16_t*>(var.value_ptr);
            return nb::cast(static_cast<bool>((val >> bit_idx) & 1));
        }
        case Type::u32: {
            auto val = *reinterpret_cast<const uint32_t*>(var.value_ptr);
            return nb::cast(static_cast<bool>((val >> bit_idx) & 1));
        }
        case Type::i32: {
            auto val = *reinterpret_cast<const int32_t*>(var.value_ptr);
            return nb::cast(static_cast<bool>((val >> bit_idx) & 1));
        }
        case Type::i64: {
            auto val = *reinterpret_cast<const int64_t*>(var.value_ptr);
            return nb::cast(static_cast<bool>((val >> bit_idx) & 1));
        }
        default:
            return nb::none();
    }
}

static nb::object read_value(const VariableDefinition& var) {
    if (!var.value_ptr || var.type.isArray()) {
        return nb::none();
    }

    if (var.type.isBit()) {
        return read_bit(var);
    }

    switch (var.type.getBaseType()) {
        case Type::boolean: {
            auto val = *reinterpret_cast<const bool*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::i8: {
            auto val = *reinterpret_cast<const int8_t*>(var.value_ptr);
            return nb::cast(static_cast<int>(val));
        }
        case Type::u8: {
            auto val = *reinterpret_cast<const uint8_t*>(var.value_ptr);
            return nb::cast(static_cast<int>(val));
        }
        case Type::i16: {
            auto val = *reinterpret_cast<const int16_t*>(var.value_ptr);
            return nb::cast(static_cast<int>(val));
        }
        case Type::u16: {
            auto val = *reinterpret_cast<const uint16_t*>(var.value_ptr);
            return nb::cast(static_cast<int>(val));
        }
        case Type::i32: {
            auto val = *reinterpret_cast<const int32_t*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::u32: {
            auto val = *reinterpret_cast<const uint32_t*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::i64: {
            auto val = *reinterpret_cast<const int64_t*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::f32: {
            auto val = *reinterpret_cast<const float*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::f64: {
            auto val = *reinterpret_cast<const double*>(var.value_ptr);
            return nb::cast(val);
        }
        case Type::cstring: {
            auto ptr = reinterpret_cast<const char*>(var.value_ptr);
            return nb::cast(std::string(ptr));
        }
        default:
            return nb::none();
    }
}

// --- read_array: atomically copy array variable to numpy ndarray ---

template <typename T>
static nb::object read_array_typed(const VariableDefinition& var) {
    sc_api::core::RevisionCountedArrayRef<T> ref(var.type.getArraySize(), var.value_ptr);
    std::vector<T> data = ref.atomicCopy();
    if (data.empty() && var.type.getArraySize() > 0) {
        return nb::none();  // atomic copy failed
    }

    size_t count = data.size();
    T* buf = new T[count];
    std::memcpy(buf, data.data(), count * sizeof(T));

    nb::capsule owner(buf, [](void* p) noexcept { delete[] static_cast<T*>(p); });

    size_t shape[] = {count};
    return nb::cast(nb::ndarray<nb::numpy, T>(buf, 1, shape, std::move(owner)));
}

static nb::object read_array(const VariableDefinition& var) {
    if (!var.value_ptr || !var.type.isArray() || var.type.getBaseType() == Type::cstring) {
        return nb::none();
    }

    switch (var.type.getBaseType()) {
        case Type::i8:
            return read_array_typed<int8_t>(var);
        case Type::u8:
            return read_array_typed<uint8_t>(var);
        case Type::i16:
            return read_array_typed<int16_t>(var);
        case Type::u16:
            return read_array_typed<uint16_t>(var);
        case Type::i32:
            return read_array_typed<int32_t>(var);
        case Type::u32:
            return read_array_typed<uint32_t>(var);
        case Type::i64:
            return read_array_typed<int64_t>(var);
        case Type::f32:
            return read_array_typed<float>(var);
        case Type::f64:
            return read_array_typed<double>(var);
        case Type::boolean: {
            // std::vector<bool> is special — use uint8_t and convert
            sc_api::core::RevisionCountedArrayRef<uint8_t> ref(
                var.type.getArraySize(), var.value_ptr);
            std::vector<uint8_t> data = ref.atomicCopy();
            if (data.empty() && var.type.getArraySize() > 0) {
                return nb::none();
            }
            size_t count = data.size();
            auto* buf = new bool[count];
            for (size_t i = 0; i < count; ++i) buf[i] = data[i] != 0;
            nb::capsule owner(buf, [](void* p) noexcept { delete[] static_cast<bool*>(p); });
            size_t shape[] = {count};
            return nb::cast(nb::ndarray<nb::numpy, bool>(buf, 1, shape, std::move(owner)));
        }
        default:
            return nb::none();
    }
}

void bind_variables(nb::module_& m) {
    // --- Type ---

    nb::class_<Type>(m, "Type")
        .def_prop_ro("base_type", &Type::getBaseType)
        .def_prop_ro("is_array", &Type::isArray)
        .def_prop_ro("array_size", &Type::getArraySize)
        .def_prop_ro("is_bit", &Type::isBit)
        .def_prop_ro("bit_index", &Type::getBitIndex)
        .def_prop_ro("is_invalid", &Type::isInvalid)
        .def_prop_ro("is_base_type", &Type::isBaseType)
        .def("__eq__",
             [](const Type& a, const Type& b) { return a == b; })
        .def("__repr__", [](const Type& self) {
            return "<Type " + self.toString() + ">";
        });

    // --- VariableDefinition ---

    nb::class_<VariableDefinition>(m, "VariableDefinition")
        .def_prop_ro("name",
                      [](const VariableDefinition& self) { return std::string(self.name); })
        .def_prop_ro("type", [](const VariableDefinition& self) { return self.type; })
        .def_prop_ro("device_session_id",
                      [](const VariableDefinition& self) { return self.device_session_id; })
        .def_prop_ro("flags", [](const VariableDefinition& self) { return self.flags; })
        .def("__bool__",
             [](const VariableDefinition& self) { return static_cast<bool>(self); })
        .def("__repr__", [](const VariableDefinition& self) {
            return "<VariableDefinition name='" + std::string(self.name) + "' type=" +
                   self.type.toString() +
                   " device=" + std::to_string(self.device_session_id.id) + ">";
        });

    // --- VariableDefinitions ---

    nb::class_<VariableDefinitions>(m, "VariableDefinitions")
        .def("__len__", &VariableDefinitions::size)
        .def(
            "__contains__",
            [](const VariableDefinitions& self, const std::string& name) {
                auto def = self.find(name);
                return static_cast<bool>(def);
            },
            nb::arg("name"))
        .def(
            "__iter__",
            [](const VariableDefinitions& self) {
                return nb::make_iterator(nb::type<VariableDefinitions>(),
                                         "VariableDefinitionsIterator",
                                         self.begin(), self.end());
            },
            nb::keep_alive<0, 1>())
        .def(
            "__getitem__",
            [](const VariableDefinitions& self, int index) {
                int size = static_cast<int>(self.size());
                if (index < 0) index += size;
                if (index < 0 || index >= size) {
                    throw nb::index_error("VariableDefinitions index out of range");
                }
                return self[static_cast<uint32_t>(index)];
            },
            nb::arg("index"))
        .def(
            "find",
            [](const VariableDefinitions& self, const std::string& name,
               std::optional<DeviceSessionId> device) -> nb::object {
                auto def = device ? self.find(name, *device)
                                  : self.find(name);
                if (!def) return nb::none();
                return nb::cast(def);
            },
            nb::arg("name"), nb::arg("device") = nb::none())
        .def_prop_ro("names",
                      [](const VariableDefinitions& self) {
                          std::vector<std::string> names;
                          names.reserve(self.size());
                          for (uint32_t i = 0; i < self.size(); ++i) {
                              names.emplace_back(self[i].name);
                          }
                          return names;
                      })
        .def_prop_ro("session",
                      [](const VariableDefinitions& self) -> nb::object {
                          auto s = self.getSession();
                          if (!s) return nb::none();
                          return nb::cast(s);
                      })
        .def(
            "read_value",
            [](const VariableDefinitions&, const VariableDefinition& var) {
                return read_value(var);
            },
            nb::arg("var"))
        .def(
            "read_array",
            [](const VariableDefinitions&, const VariableDefinition& var) {
                return read_array(var);
            },
            nb::arg("var"))
        .def("__repr__", [](const VariableDefinitions& self) {
            return "<VariableDefinitions size=" + std::to_string(self.size()) + ">";
        });
}
