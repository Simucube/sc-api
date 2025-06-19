#include "sc-api/core/sim_data_builder.h"

namespace sc_api::core::sim_data {

static constexpr char k_hex[] = "0123456789abcdef";

SimDataUpdateBuilder::SimDataUpdateBuilder(std::string_view sim_id, bool activate_sim)
    : sim_id_(sim_id), activate_sim_(activate_sim) {
    if (activate_sim) {
        b_.docAddElement("active_sim", sim_id);
    }

    b_.docBeginSubDoc("sim");
    b_.docBeginSubDoc(sim_id);
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(VehiclesBuilder& builder) {
    b_.docAddSubDoc("vehicles", builder.finish().first);
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(std::string_view vehicle_id, VehicleBuilder& builder) {
    b_.docBeginSubDoc("vehicles");
    b_.docAddSubDoc(vehicle_id, builder.finish().first);
    b_.endDocument();
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(ParticipantsBuilder& builder) {
    b_.docAddSubDoc("participants", builder.finish().first);
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(int participant_id, ParticipantBuilder& builder) {
    b_.docBeginSubDoc("participants");
    docAddSubDocHex16Key(participant_id, builder.finish().first);
    b_.endDocument();
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(TracksBuilder& builder) {
    b_.docAddSubDoc("tracks", builder.finish().first);
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(std::string_view track_id, TrackBuilder& builder) {
    b_.docBeginSubDoc("tracks");
    b_.docAddSubDoc(track_id, builder.finish().first);
    b_.endDocument();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(TiresBuilder& builder) {
    b_.docAddSubDoc("tires", builder.finish().first);
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(int tire_id, TireBuilder& builder) {
    b_.docBeginSubDoc("tires");
    docAddSubDocHex16Key(tire_id, builder.finish().first);
    b_.endDocument();
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(SessionsBuilder& builder) {
    b_.docAddSubDoc("sessions", builder.finish().first);
    builder.initialize();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(std::string_view session_id, SessionBuilder& builder) {
    b_.docBeginSubDoc("sessions");
    b_.docAddSubDoc(session_id, builder.finish().first);
    b_.endDocument();
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::setActiveSession(std::string_view session_id) {
    b_.docAddElement("active_session", session_id);
    return *this;
}

SimDataUpdateBuilder& SimDataUpdateBuilder::buildAndSet(SimBuilder& builder) {
    b_.docAddSubDoc("sim", builder.finish().first);
    builder.initialize();
    return *this;
}

void SimDataUpdateBuilder::initialize() {
    b_.initialize(&buffer_);
    if (activate_sim_) {
        b_.docAddElement("active_sim", sim_id_);
    }

    b_.docBeginSubDoc("sim");
    b_.docBeginSubDoc(sim_id_);
}

std::pair<const uint8_t*, int32_t> SimDataUpdateBuilder::finish() {
    b_.endDocument();
    b_.endDocument();
    return BuilderBase::finish();
}

std::pair<const uint8_t*, int32_t> BuilderBase::finish() {
    return b_.finish();
}

void BuilderBase::initialize() { b_.initialize(&buffer_); }

BuilderBase::BuilderBase() : b_(&buffer_) {}

void BuilderBase::docAddNullHex16Key(int id) {
    assert(id > 0 && id <= 0xffff);
    char key[5] = {0};
    key[0]      = k_hex[(id >> 12) & 0xf];
    key[1]      = k_hex[(id >> 8) & 0xf];
    key[2]      = k_hex[(id >> 4) & 0xf];
    key[3]      = k_hex[(id >> 0) & 0xf];
    b_.docAddElement(key, nullptr);
}

void BuilderBase::docAddSubDocHex16Key(int id, const uint8_t* sub_doc) {
    assert(id > 0 && id <= 0xffff);
    char key[5] = {0};
    key[0]      = k_hex[(id >> 12) & 0xf];
    key[1]      = k_hex[(id >> 8) & 0xf];
    key[2]      = k_hex[(id >> 4) & 0xf];
    key[3]      = k_hex[(id >> 0) & 0xf];

    b_.docAddSubDoc(key, sub_doc);
}

void TypedBuilderBase::setImpl(std::string_view key, int32_t val) { b_.docAddElement(key, val); }

void TypedBuilderBase::setImpl(std::string_view key, int64_t val) { b_.docAddElement(key, val); }

void TypedBuilderBase::setImpl(std::string_view key, double val) { b_.docAddElement(key, val); }

void TypedBuilderBase::setImpl(std::string_view key, std::string_view val) { b_.docAddElement(key, val); }

void TypedBuilderBase::setImpl(std::string_view key, bool val) { b_.docAddElement(key, val); }

}  // namespace sc_api::core::sim_data
