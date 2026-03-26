#include "bind_sim_data.h"

#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/shared_ptr.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <functional>
#include <string>
#include <vector>

#include <sc-api/core/sim_data.h>
#include <sc-api/core/sim_data_builder.h>
#include <sc-api/core/sim_data/participant.h>
#include <sc-api/core/sim_data/session.h>
#include <sc-api/core/sim_data/sim.h>
#include <sc-api/core/sim_data/tire.h>
#include <sc-api/core/sim_data/track.h>
#include <sc-api/core/sim_data/vehicle.h>

namespace nb = nanobind;

using namespace sc_api::core::sim_data;

// ---------------------------------------------------------------------------
// PySubSection: prevents SimData GC while subsection pointers are alive
// ---------------------------------------------------------------------------

template <typename T>
struct PySubSection {
    T                        inner;
    std::shared_ptr<SimData> owner;
};

// ---------------------------------------------------------------------------
// PropertyEntry: unified getter (read) / setter (builder write) per property
// ---------------------------------------------------------------------------

template <typename SubSection, typename Builder>
struct PropertyEntry {
    const char*                                    name;
    std::function<nb::object(const SubSection&)>   getter;
    std::function<void(Builder&, nb::handle)>      setter;
};

template <typename SubSection, typename Builder, typename T, typename PropClass>
PropertyEntry<SubSection, Builder> make_entry(
    const sc_api::core::sim_data::TypedAndClassifiedPropertyRef<T, PropClass>& ref) {
    return {
        ref.name.data(),  // NOLINT(bugprone-suspicious-stringview-data-usage)
        [&ref](const SubSection& s) -> nb::object {
            auto val = s.get(ref);
            if (!val) return nb::none();
            if constexpr (std::is_same_v<T, std::string_view>)
                return nb::cast(std::string(*val));
            else
                return nb::cast(*val);
        },
        [&ref](Builder& b, nb::handle val) {
            if constexpr (std::is_same_v<T, std::string_view>)
                b.set(ref, nb::cast<std::string>(val));
            else
                b.set(ref, nb::cast<T>(val));
        }};
}

// ---------------------------------------------------------------------------
// Property tables (function-local statics for safe init ordering)
// ---------------------------------------------------------------------------

using VehicleEntry     = PropertyEntry<Vehicle, VehicleBuilder>;
using TrackEntry       = PropertyEntry<Track, TrackBuilder>;
using ParticipantEntry = PropertyEntry<Participant, ParticipantBuilder>;
using SessionEntry     = PropertyEntry<Session, SessionBuilder>;
using TireEntry        = PropertyEntry<Tire, TireBuilder>;
using SimEntry         = PropertyEntry<Sim, SimBuilder>;

#include "sim_data_tables_generated.h"

// ---------------------------------------------------------------------------
// Dict-like interface methods (__getitem__, get, keys, __contains__)
// ---------------------------------------------------------------------------

template <typename SubSection, typename Builder>
void add_dict_methods(nb::class_<PySubSection<SubSection>>& cls,
                      const std::vector<PropertyEntry<SubSection, Builder>>* table) {
    cls.def(
        "__getitem__",
        [table](const PySubSection<SubSection>& self,
                const std::string& key) -> nb::object {
            for (const auto& entry : *table) {
                if (key == entry.name) {
                    nb::object val = entry.getter(self.inner);
                    if (val.is_none()) throw nb::key_error(key.c_str());
                    return val;
                }
            }
            throw nb::key_error(key.c_str());
        },
        nb::arg("key"),
        "Return the value for *key*. Raises KeyError if the key is unknown or the property has no value.");

    cls.def(
        "get",
        [table](const PySubSection<SubSection>& self, const std::string& key,
                nb::object default_val) -> nb::object {
            for (const auto& entry : *table) {
                if (key == entry.name) {
                    nb::object val = entry.getter(self.inner);
                    if (val.is_none()) return default_val;
                    return val;
                }
            }
            return default_val;
        },
        nb::arg("key"), nb::arg("default") = nb::none(),
        "Return the value for *key*, or *default* if the key is unknown or the property has no value.");

    cls.def(
        "keys",
        [table](const PySubSection<SubSection>& self) {
            nb::list result;
            for (const auto& entry : *table) {
                nb::object val = entry.getter(self.inner);
                if (!val.is_none()) {
                    result.append(nb::cast(entry.name));
                }
            }
            return result;
        },
        "Return the list of currently available property names.");

    cls.def(
        "__contains__",
        [table](const PySubSection<SubSection>& self,
                const std::string& key) -> bool {
            for (const auto& entry : *table) {
                if (key == entry.name) {
                    nb::object val = entry.getter(self.inner);
                    return !val.is_none();
                }
            }
            return false;
        },
        nb::arg("key"),
        "True if *key* is a known property with a value currently set.");
}

// ---------------------------------------------------------------------------
// Builder helper: apply dict entries to a TypedBuilder using a property table
// ---------------------------------------------------------------------------

template <typename SubSection, typename Builder>
void apply_dict_to_builder(
    Builder& builder, const nb::dict& props,
    const std::vector<PropertyEntry<SubSection, Builder>>& table,
    const char* skip_key = nullptr) {
    for (auto item : props) {
        std::string k = nb::cast<std::string>(item.first);
        if (skip_key && k == skip_key) continue;
        bool found = false;
        for (const auto& entry : table) {
            if (k == entry.name) {
                entry.setter(builder, item.second);
                found = true;
                break;
            }
        }
        if (!found) {
            std::string valid;
            for (const auto& entry : table) {
                if (!valid.empty()) valid += ", ";
                valid += entry.name;
            }
            std::string msg = "Unknown property '" + k + "'. Valid keys: ";
            msg += valid;
            throw nb::value_error(msg.c_str());
        }
    }
}

// ---------------------------------------------------------------------------
// bind_sim_data — register all Python types
// ---------------------------------------------------------------------------

void bind_sim_data(nb::module_& m) {
    // --- Vehicle ---
    auto vehicle_cls =
        nb::class_<PySubSection<Vehicle>>(m, "Vehicle",
                                          "Vehicle state data from the simulator.")
            .def_prop_ro("id",
                         [](const PySubSection<Vehicle>& self) {
                             return std::string(self.inner.getId());
                         },
                         "Vehicle identifier string.")
            .def_prop_ro("name",
                         [](const PySubSection<Vehicle>& self) {
                             return std::string(self.inner.getName());
                         },
                         "Vehicle name.")
            .def("__repr__", [](const PySubSection<Vehicle>& self) {
                return "<Vehicle id='" +
                       std::string(self.inner.getId()) + "'>";
            });
    add_dict_methods(vehicle_cls, &get_vehicle_table());

    // --- Track ---
    auto track_cls =
        nb::class_<PySubSection<Track>>(m, "Track",
                                        "Track/circuit info from the simulator.")
            .def_prop_ro("id",
                         [](const PySubSection<Track>& self) {
                             return std::string(self.inner.getId());
                         },
                         "Track identifier string.")
            .def_prop_ro("name",
                         [](const PySubSection<Track>& self) {
                             return std::string(self.inner.getName());
                         },
                         "Track name.")
            .def("__repr__", [](const PySubSection<Track>& self) {
                return "<Track id='" +
                       std::string(self.inner.getId()) + "'>";
            });
    add_dict_methods(track_cls, &get_track_table());

    // --- Participant ---
    auto participant_cls =
        nb::class_<PySubSection<Participant>>(m, "Participant",
                                             "A driver/participant in the sim session.")
            .def_prop_ro("id",
                         [](const PySubSection<Participant>& self) {
                             return self.inner.getId();
                         },
                         "Participant identifier integer.")
            .def("__repr__", [](const PySubSection<Participant>& self) {
                return "<Participant id=" +
                       std::to_string(self.inner.getId()) + ">";
            });
    add_dict_methods(participant_cls, &get_participant_table());

    // --- SimSession (sim_data::Session, avoid name clash with core::Session) ---
    auto session_cls =
        nb::class_<PySubSection<Session>>(m, "SimSession",
                                          "Sim session info (practice, qualifying, race, etc.).")
            .def_prop_ro("id",
                         [](const PySubSection<Session>& self) {
                             return std::string(self.inner.getId());
                         },
                         "Session identifier string.")
            .def("__repr__", [](const PySubSection<Session>& self) {
                return "<SimSession id='" +
                       std::string(self.inner.getId()) + "'>";
            });
    add_dict_methods(session_cls, &get_session_table());

    // --- Tire ---
    auto tire_cls =
        nb::class_<PySubSection<Tire>>(m, "Tire",
                                      "Tire state data from the simulator.")
            .def_prop_ro("id",
                         [](const PySubSection<Tire>& self) {
                             return self.inner.getId();
                         },
                         "Tire identifier integer.")
            .def("__repr__", [](const PySubSection<Tire>& self) {
                return "<Tire id=" +
                       std::to_string(self.inner.getId()) + ">";
            });
    add_dict_methods(tire_cls, &get_tire_table());

    // --- Sim ---
    auto sim_cls =
        nb::class_<PySubSection<Sim>>(m, "Sim",
                                     "Simulator identity info.")
            .def_prop_ro("id",
                         [](const PySubSection<Sim>& self) {
                             return std::string(self.inner.getId());
                         },
                         "Simulator identifier string.")
            .def("__repr__", [](const PySubSection<Sim>& self) {
                return "<Sim id='" +
                       std::string(self.inner.getId()) + "'>";
            });
    add_dict_methods(sim_cls, &get_sim_table());

    // --- SimData ---
    nb::class_<SimData>(m, "SimData",
                        "Top-level container for all simulator state data. Read-only.")
        .def_prop_ro("revision", &SimData::getRevision,
                     "Revision counter, incremented on each update.")
        .def_prop_ro(
            "sim",
            [](const std::shared_ptr<SimData>& self) -> nb::object {
                const auto& s = self->getSim();
                if (!s) return nb::none();
                return nb::cast(PySubSection<Sim>{*s, self});
            },
            "Simulator identity info, or None if not available.")
        .def_prop_ro(
            "vehicles",
            [](const std::shared_ptr<SimData>& self) {
                nb::list result;
                for (const auto& v : self->getVehicles()) {
                    result.append(
                        nb::cast(PySubSection<Vehicle>{v, self}));
                }
                return result;
            },
            "List of all vehicles in the session.")
        .def_prop_ro(
            "player_vehicle",
            [](const std::shared_ptr<SimData>& self) -> nb::object {
                const auto* v = self->getPlayerVehicle();
                if (!v) return nb::none();
                return nb::cast(PySubSection<Vehicle>{*v, self});
            },
            "The player's vehicle, or None if not available.")
        .def_prop_ro(
            "tracks",
            [](const std::shared_ptr<SimData>& self) {
                nb::list result;
                for (const auto& t : self->getTracks()) {
                    result.append(
                        nb::cast(PySubSection<Track>{t, self}));
                }
                return result;
            },
            "List of all tracks.")
        .def_prop_ro(
            "current_track",
            [](const std::shared_ptr<SimData>& self) -> nb::object {
                const auto* t = self->getCurrentTrack();
                if (!t) return nb::none();
                return nb::cast(PySubSection<Track>{*t, self});
            },
            "The active track, or None if not available.")
        .def_prop_ro(
            "participants",
            [](const std::shared_ptr<SimData>& self) {
                nb::list result;
                for (const auto& p : self->getParticipants()) {
                    result.append(
                        nb::cast(PySubSection<Participant>{p, self}));
                }
                return result;
            },
            "List of all participants/drivers in the session.")
        .def_prop_ro(
            "sessions",
            [](const std::shared_ptr<SimData>& self) {
                nb::list result;
                for (const auto& s : self->getSessions()) {
                    result.append(
                        nb::cast(PySubSection<Session>{s, self}));
                }
                return result;
            },
            "List of all sim sessions.")
        .def_prop_ro(
            "current_session",
            [](const std::shared_ptr<SimData>& self) -> nb::object {
                const auto* s = self->getCurrentSession();
                if (!s) return nb::none();
                return nb::cast(PySubSection<Session>{*s, self});
            },
            "The active sim session, or None if not available.")
        .def_prop_ro(
            "tires",
            [](const std::shared_ptr<SimData>& self) {
                nb::list result;
                for (const auto& t : self->getTires()) {
                    result.append(
                        nb::cast(PySubSection<Tire>{t, self}));
                }
                return result;
            },
            "List of all tires.")
        .def("__repr__", [](const SimData& self) {
            return "<SimData revision=" +
                   std::to_string(self.getRevision()) + ">";
        });
}

// ---------------------------------------------------------------------------
// populate_sim_data_builder — walk a Python dict and fill a builder
// ---------------------------------------------------------------------------

void populate_sim_data_builder(SimDataUpdateBuilder& builder, const nb::dict& data) {
    for (auto item : data) {
        std::string section = nb::cast<std::string>(item.first);

        if (section == "sim") {
            SimBuilder sb;
            apply_dict_to_builder(sb, nb::cast<nb::dict>(item.second),
                                  get_sim_table(), "id");
            builder.buildAndSet(sb);

        } else if (section == "vehicles") {
            VehiclesBuilder list_builder;
            for (nb::handle elem : nb::cast<nb::list>(item.second)) {
                nb::dict d = nb::cast<nb::dict>(elem);
                std::string id = nb::cast<std::string>(d["id"]);
                VehicleBuilder vb;
                apply_dict_to_builder(vb, d, get_vehicle_table(), "id");
                list_builder.buildAndAdd(id, vb);
            }
            builder.buildAndSet(list_builder);

        } else if (section == "tracks") {
            TracksBuilder list_builder;
            for (nb::handle elem : nb::cast<nb::list>(item.second)) {
                nb::dict d = nb::cast<nb::dict>(elem);
                std::string id = nb::cast<std::string>(d["id"]);
                TrackBuilder tb;
                apply_dict_to_builder(tb, d, get_track_table(), "id");
                list_builder.buildAndAdd(id, tb);
            }
            builder.buildAndSet(list_builder);

        } else if (section == "participants") {
            ParticipantsBuilder list_builder;
            for (nb::handle elem : nb::cast<nb::list>(item.second)) {
                nb::dict d = nb::cast<nb::dict>(elem);
                int id = nb::cast<int>(d["id"]);
                ParticipantBuilder pb;
                apply_dict_to_builder(pb, d, get_participant_table(), "id");
                list_builder.buildAndAdd(id, pb);
            }
            builder.buildAndSet(list_builder);

        } else if (section == "sessions") {
            SessionsBuilder list_builder;
            for (nb::handle elem : nb::cast<nb::list>(item.second)) {
                nb::dict d = nb::cast<nb::dict>(elem);
                std::string id = nb::cast<std::string>(d["id"]);
                SessionBuilder sb;
                apply_dict_to_builder(sb, d, get_session_table(), "id");
                list_builder.buildAndAdd(id, sb);
            }
            builder.buildAndSet(list_builder);

        } else if (section == "tires") {
            TiresBuilder list_builder;
            for (nb::handle elem : nb::cast<nb::list>(item.second)) {
                nb::dict d = nb::cast<nb::dict>(elem);
                int id = nb::cast<int>(d["id"]);
                TireBuilder tb;
                apply_dict_to_builder(tb, d, get_tire_table(), "id");
                list_builder.buildAndAdd(id, tb);
            }
            builder.buildAndSet(list_builder);

        } else if (section == "active_session") {
            builder.setActiveSession(nb::cast<std::string>(item.second));

        } else {
            throw nb::value_error(
                ("Unknown section '" + section +
                 "'. Valid sections: sim, vehicles, tracks, participants, "
                 "sessions, tires, active_session")
                    .c_str());
        }
    }
}
