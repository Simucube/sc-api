
#include "sc-api/core/sim_data.h"

#include <cassert>

#include "sc-api/core/session.h"
#include "sc-api/core/sim_data/session.h"
#include "sc-api/core/sim_data/track.h"
#include "sc-api/core/sim_data/vehicle.h"
#include "sim_data_internal.h"

namespace sc_api::core {

/* Participant ids are 4 character hex string that we just convert to number */
static int convertStringNumericKey(std::string_view s) {
    if (s.size() != 4) return -1;

    int num = 0;
    for (char c : s) {
        if (c >= '0' && c <= '9') {
            num = (num << 4) + (int)(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            num = (num << 4) + (int)(c - 'A') + 10;
        } else if (c >= 'a' && c <= 'f') {
            num = (num << 4) + (int)(c - 'a') + 10;
        } else {
            return -1;
        }
    }

    return num;
}

std::shared_ptr<sim_data::SimData> internal::SimDataProvider::parseSimData() {
    std::shared_ptr<uint8_t[]> raw_bson;
    uint32_t                   raw_bson_revision = 0;
    {
        std::shared_lock lock(mutex_);
        raw_bson = getActiveBuffer();
        raw_bson_revision = getActiveBufferRevision();
        if (parsed_data_ && raw_bson_revision == parsed_data_->getRevision() &&
            raw_bson.get() == parsed_data_->getRawBson()) {
            // No new data available. We can use previously parsed version
            return parsed_data_;
        }
    }

    std::shared_ptr<sim_data::SimData> parsed = sim_data::SimData::parseFromRaw(raw_bson, raw_bson_revision);
    std::unique_lock                   lock(mutex_);
    parsed_data_ = parsed;
    return parsed;
}

namespace sim_data {

Vehicle::Vehicle(std::string_view id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

std::string_view Vehicle::getName() const { return getOrDefault(sc_api::core::sim_data::vehicle::name, ""); }

Track::Track(std::string_view id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

std::string_view Track::getName() const { return getOrDefault(sc_api::core::sim_data::track::name, ""); }

Tire::Tire(int id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

Participant::Participant(int id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

Session::Session(std::string_view id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

SimData::SimData(RawData raw_data) : r_(std::move(raw_data)) {}

const Vehicle* SimData::getVehicle(std::string_view id) const {
    for (auto& v : r_.vehicles) {
        if (v.getId() == id) return &v;
    }

    return nullptr;
}

const std::vector<Vehicle>& SimData::getVehicles() const { return r_.vehicles; }

const Vehicle* SimData::getPlayerVehicle() const {
    const Session* current_session = getCurrentSession();
    if (!current_session) return nullptr;

    if (auto opt_player_vehicle_id = current_session->get(session::player_vehicle_id)) {
        return getVehicle(*opt_player_vehicle_id);
    }
    return nullptr;
}

const Participant* SimData::getParticipant(int id) const {
    for (auto& s : r_.participants) {
        if (s.getId() == id) return &s;
    }

    return nullptr;
}

const std::vector<Participant>& SimData::getParticipants() const { return r_.participants; }

const Participant* SimData::getParticipantPlayer() const {
    const Session* current_session = getCurrentSession();
    if (!current_session) return nullptr;

    if (auto opt_player_id = current_session->get(session::player_participant_id)) {
        return getParticipant(*opt_player_id);
    }
    return nullptr;
}

const Track* SimData::getTrack(std::string_view id) const {
    for (auto& t : r_.tracks) {
        if (t.getId() == id) return &t;
    }

    return nullptr;
}

const std::vector<Track>& SimData::getTracks() const { return r_.tracks; }

const Track* SimData::getCurrentTrack() const {
    const Session* current_session = getCurrentSession();
    if (!current_session) return nullptr;

    if (auto opt_track_id = current_session->get(session::track_id)) {
        return getTrack(*opt_track_id);
    }
    return nullptr;
}

const Session* SimData::getSession(std::string_view id) const {
    for (auto& s : r_.sessions) {
        if (s.getId() == id) return &s;
    }

    return nullptr;
}

const std::vector<Session>& SimData::getSessions() const { return r_.sessions; }

const Session* SimData::getCurrentSession() const {
    if (r_.active_session_idx >= 0 && r_.active_session_idx < (int)r_.sessions.size()) {
        return &r_.sessions[r_.active_session_idx];
    }
    return nullptr;
}

const Tire* SimData::getTire(int id) const {
    for (auto& t : r_.tires) {
        if (t.getId() == id) return &t;
    }

    return nullptr;
}

uint32_t SimData::getRevision() const { return r_.revision; }

const uint8_t* SimData::getRawBson() const { return r_.raw_bson.get(); }

std::shared_ptr<SimData> SimData::parseFromRaw(const std::shared_ptr<const uint8_t[]>& raw_bson, uint32_t revision) {
    if (!raw_bson) {
        return nullptr;
    }

    util::BsonReader r(raw_bson.get());

    RawData raw;
    raw.raw_bson                  = raw_bson;

    const uint8_t*   sim_data_ptr = nullptr;
    std::string_view active_session;
    std::string_view active_sim;

    using E = util::BsonReader::ElementType;
    while (true) {
        E t = r.next();
        if (t == E::ELEMENT_END) break;

        if (t == E::ELEMENT_DOC) {
            if (r.key() == "vehicles") {
                raw.vehicles_raw_bson = r.subdocument().first;
                r.beginSub();

                while (!r.atEnd()) {
                    t = r.next();

                    if (t == E::ELEMENT_DOC) {
                        raw.vehicles.emplace_back(r.key(), raw_bson.get() + r.subdocumentOffset());
                    }
                }

                r.endSub();
            } else if (r.key() == "participants") {
                raw.participant_raw_bson = r.subdocument().first;
                r.beginSub();

                while (!r.atEnd()) {
                    t      = r.next();

                    int id = convertStringNumericKey(r.key());

                    // Ignore invalid keys
                    if (id == -1) continue;

                    if (t == E::ELEMENT_DOC) {
                        raw.participants.emplace_back(id, raw_bson.get() + r.subdocumentOffset());
                    }
                }

                r.endSub();
            } else if (r.key() == "sessions") {
                r.beginSub();

                while (!r.atEnd()) {
                    t = r.next();

                    if (t == E::ELEMENT_DOC) {
                        raw.sessions.emplace_back(r.key(), raw_bson.get() + r.subdocumentOffset());
                    }
                }

                r.endSub();
            } else if (r.key() == "tracks") {
                r.beginSub();

                while (!r.atEnd()) {
                    t = r.next();

                    if (t == E::ELEMENT_DOC) {
                        raw.tracks.emplace_back(r.key(), raw_bson.get() + r.subdocumentOffset());
                    }
                }

                r.endSub();
            } else if (r.key() == "tires") {
                raw.tires_raw_bson = r.subdocument().first;
                r.beginSub();

                while (!r.atEnd()) {
                    t      = r.next();

                    int id = convertStringNumericKey(r.key());
                    // Ignore invalid keys
                    if (id == -1) continue;

                    if (t == E::ELEMENT_DOC) {
                        raw.tires.emplace_back(id, raw_bson.get() + r.subdocumentOffset());
                    }
                }

                r.endSub();
            } else if (r.key() == "sim") {
                sim_data_ptr = r.subdocument().first;
            }
        } else if (t == E::ELEMENT_STR) {
            if (r.key() == "active_session") {
                active_session = r.stringValue();
            } else if (r.key() == "active_sim") {
                active_sim = r.stringValue();
            }
        }
    }

    for (int idx = 0; idx < (int)raw.sessions.size(); ++idx) {
        if (raw.sessions[idx].getId() == active_session) {
            raw.active_session_idx = idx;
            break;
        }
    }

    if (sim_data_ptr && !active_sim.empty()) {
        raw.sim = Sim(active_sim, sim_data_ptr);
    }

    return std::make_shared<sim_data::SimData>(std::move(raw));
}

Sim::Sim(std::string_view id, const uint8_t* raw_bson) : SimDataSubSectionGetters(raw_bson), id_(id) {}

}  // namespace sim_data

}  // namespace sc_api::core
