/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_SIM_DATA_H_
#define SC_API_CORE_SIM_DATA_H_
#include <cassert>
#include <memory>
#include <optional>

#include "sc-api/core/property_reference.h"
#include "util/bson_reader.h"

namespace sc_api::core {
namespace sim_data {

class SimData;

class SimDataSubSection {
public:
    const uint8_t* getRawBsonPointer() const { return bson_ptr_; }

protected:
    SimDataSubSection(const uint8_t* raw_bson) : bson_ptr_(raw_bson) {}

    template <typename T>
    auto getProperty(const TypedPropertyRef<T>& ref) const -> std::optional<T>;

    template <typename T>
    auto getPropertyOrDefault(const TypedPropertyRef<T>& ref, T def) const -> T;

    template <typename T>
    bool tryGetProperty(const TypedPropertyRef<T>& ref, T& val) const;

    const uint8_t* bson_ptr_;
};

// Provides public access methods that only accept refs of correct type
template <typename PropertyClassTag>
class SimDataSubSectionGetters : public SimDataSubSection {
public:
    template <typename T>
    auto get(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>& ref) const -> std::optional<T> {
        return SimDataSubSection::getProperty(ref);
    }

    template <typename T>
    auto getOrDefault(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>&         ref,
                      typename TypedAndClassifiedPropertyRef<T, PropertyClassTag>::type def) const -> T {
        return SimDataSubSection::getPropertyOrDefault(ref, def);
    }

    template <typename T>
    bool tryGet(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>&          ref,
                typename TypedAndClassifiedPropertyRef<T, PropertyClassTag>::type& value) {
        return SimDataSubSection::tryGetProperty(ref, value);
    }

protected:
    using SimDataSubSection::SimDataSubSection;
};

class Vehicle : public SimDataSubSectionGetters<VehiclePropertyClass> {
public:
    Vehicle(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }
    std::string_view getName() const;

private:
    std::string_view id_;
};

class Session : public SimDataSubSectionGetters<SessionPropertyClass> {
public:
    Session(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }

private:
    std::string_view id_;
};

class Tire : public SimDataSubSectionGetters<TirePropertyClass> {
public:
    Tire(int id, const uint8_t* raw_bson);

    int getId() const { return id_; }

private:
    int id_;
};

class Participant : public SimDataSubSectionGetters<ParticipantPropertyClass> {
public:
    Participant(int id, const uint8_t* raw_bson);

    int getId() const { return id_; }

private:
    int id_;
};

class Track : public SimDataSubSectionGetters<TrackPropertyClass> {
public:
    Track(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }

    std::string_view getName() const;

private:
    std::string_view id_;
};

class Sim : public SimDataSubSectionGetters<SimPropertyClass> {
public:
    Sim(std::string_view id, const uint8_t* raw_bson);
    std::string_view getId() const { return id_; }

private:
    std::string_view id_;
};

using VehiclePtr = std::shared_ptr<Vehicle>;

class SimData : std::enable_shared_from_this<SimData> {
    friend class SimDataSubSection;

public:
    struct RawData {
        std::shared_ptr<const uint8_t[]> raw_bson;
        uint32_t                         revision           = 0;
        int                              active_session_idx = -1;
        std::optional<Sim>               sim                = std::nullopt;
        std::vector<Vehicle>             vehicles;
        std::vector<Session>             sessions;
        std::vector<Track>               tracks;
        std::vector<Participant>         participants;
        std::vector<Tire>                tires;

        const uint8_t* participant_raw_bson = nullptr;
        const uint8_t* vehicles_raw_bson    = nullptr;
        const uint8_t* tires_raw_bson       = nullptr;
    };

    SimData(RawData raw_data);

    const std::optional<Sim>& getSim() const { return r_.sim; }

    const Vehicle*              getVehicle(std::string_view id) const;
    const std::vector<Vehicle>& getVehicles() const;
    const Vehicle*              getPlayerVehicle() const;
    const uint8_t*              getVehiclesRawBson() const { return r_.vehicles_raw_bson; }

    const Participant*              getParticipant(int id) const;
    const std::vector<Participant>& getParticipants() const;
    const Participant*              getParticipantPlayer() const;
    const uint8_t*                  getParticipantsRawBson() const { return r_.participant_raw_bson; }

    const Track*              getTrack(std::string_view id) const;
    const std::vector<Track>& getTracks() const;
    const Track*              getCurrentTrack() const;

    const Session*              getSession(std::string_view id) const;
    const std::vector<Session>& getSessions() const;
    const Session*              getCurrentSession() const;

    const std::vector<Tire>& getTires() const;
    const Tire*              getTire(int id) const;
    const uint8_t*           getTiresRawBson() const { return r_.tires_raw_bson; }

    uint32_t       getRevision() const;
    const uint8_t* getRawBson() const;

    static std::shared_ptr<SimData> parseFromRaw(const std::shared_ptr<const uint8_t[]>& raw_bson, uint32_t revision);

private:
    RawData r_;
};

template <typename T>
inline auto SimDataSubSection::getProperty(const TypedPropertyRef<T>& ref) const -> std::optional<T> {
    util::BsonReader r(bson_ptr_);

    T v;
    if (r.tryFindAndGet(ref.name, v)) {
        return v;
    }

    return std::nullopt;
}

template <typename T>
inline auto SimDataSubSection::getPropertyOrDefault(const TypedPropertyRef<T>& ref, T def) const -> T {
    util::BsonReader r(bson_ptr_);

    r.tryFindAndGet(ref.name, def);
    return def;
}

template <typename T>
inline bool SimDataSubSection::tryGetProperty(const TypedPropertyRef<T>& ref, T& val) const {
    util::BsonReader r(bson_ptr_);
    r.tryFindAndGet(ref.name, val);
}

}  // namespace sim_data

}  // namespace sc_api::core

#endif  // SC_API_CORE_SIM_DATA_H_
