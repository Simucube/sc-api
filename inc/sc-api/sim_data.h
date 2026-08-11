/**
 * @file
 * @brief Read access to simulator state data.
 *
 * Sim data describes the running game and the current play session: the simulator, the
 * vehicles, the track, the session, the participants and the tires. Tuner uses it to
 * recognize the game and the vehicle.
 *
 * The data is one BSON document in shared memory. Session::getSimData returns the parsed
 * form. A SimDataChanged event signals that a newer revision is available.
 *
 * Every getter returns std::nullopt or nullptr when the simulator does not supply that
 * property. Sim data is optional, so no property is guaranteed to be present.
 *
 * @see sc-api/sim_data_builder.h to write sim data.
 */

#ifndef SC_API_SIM_DATA_H_
#define SC_API_SIM_DATA_H_
#include <cassert>
#include <memory>
#include <optional>

#include "sc-api/property_reference.h"
#include "sc-api/sim_data/participant.h"
#include "sc-api/sim_data/session.h"
#include "sc-api/sim_data/sim.h"
#include "sc-api/sim_data/tire.h"
#include "sc-api/sim_data/track.h"
#include "sc-api/sim_data/vehicle.h"
#include "sc-api/sim_data_builder.h"
#include "util/bson_reader.h"

namespace sc_api {
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

/** Read access to the properties of one sim data section
 *
 * The section tag makes the getters accept only the property references of this section.
 * Therefore a track property cannot be read from a vehicle.
 *
 * Each getter reads the value from the BSON document on every call.
 */
template <typename PropertyClassTag>
class SimDataSubSectionGetters : public SimDataSubSection {
public:
    /** Get a property value
     *
     * @param ref Reference to the property. @see sc-api/sim_data/ generated headers
     * @return The value, or std::nullopt if the simulator did not supply this property
     */
    template <typename T>
    auto get(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>& ref) const -> std::optional<T> {
        return SimDataSubSection::getProperty(ref);
    }

    /** Get a property value, or def if the simulator did not supply this property */
    template <typename T>
    auto getOrDefault(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>&         ref,
                      typename TypedAndClassifiedPropertyRef<T, PropertyClassTag>::type def) const -> T {
        return SimDataSubSection::getPropertyOrDefault(ref, def);
    }

    /** Get a property value into value
     *
     * @return true, if the property is present. Then value holds it.
     *         false, if the property is absent. Then value is unchanged.
     */
    template <typename T>
    bool tryGet(const TypedAndClassifiedPropertyRef<T, PropertyClassTag>&          ref,
                typename TypedAndClassifiedPropertyRef<T, PropertyClassTag>::type& value) {
        return SimDataSubSection::tryGetProperty(ref, value);
    }

protected:
    using SimDataSubSection::SimDataSubSection;
};

/** One vehicle in the sim data. @see sc-api/sim_data/vehicle.h for the properties */
class Vehicle : public SimDataSubSectionGetters<VehiclePropertyClass> {
public:
    Vehicle(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }

    /** Display name of the vehicle. Empty if the simulator gave none */
    std::string_view getName() const;

private:
    std::string_view id_;
};

/** One play session in the game, such as practice, qualifying or a race
 *
 * This is not sc_api::Session, which is the connection to Tuner.
 *
 * @see sc-api/sim_data/session.h for the properties
 */
class Session : public SimDataSubSectionGetters<SessionPropertyClass> {
public:
    Session(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }

private:
    std::string_view id_;
};

/** One tire of the player vehicle. @see sc-api/sim_data/tire.h for the properties */
class Tire : public SimDataSubSectionGetters<TirePropertyClass> {
public:
    Tire(int id, const uint8_t* raw_bson);

    int getId() const { return id_; }

private:
    int id_;
};

/** One driver in the play session. @see sc-api/sim_data/participant.h for the properties */
class Participant : public SimDataSubSectionGetters<ParticipantPropertyClass> {
public:
    Participant(int id, const uint8_t* raw_bson);

    int getId() const { return id_; }

private:
    int id_;
};

/** One track in the sim data. @see sc-api/sim_data/track.h for the properties */
class Track : public SimDataSubSectionGetters<TrackPropertyClass> {
public:
    Track(std::string_view id, const uint8_t* raw_bson);

    std::string_view getId() const { return id_; }

    /** Display name of the track. Empty if the simulator gave none */
    std::string_view getName() const;

private:
    std::string_view id_;
};

/** The simulator that produces this sim data. @see sc-api/sim_data/sim.h for the properties */
class Sim : public SimDataSubSectionGetters<SimPropertyClass> {
public:
    Sim(std::string_view id, const uint8_t* raw_bson);
    std::string_view getId() const { return id_; }

private:
    std::string_view id_;
};

using VehiclePtr = std::shared_ptr<Vehicle>;

/** Parsed simulator state data for one revision
 *
 * Get an instance from Session::getSimData. The instance is a snapshot and it never changes.
 * After a SimDataChanged event, call Session::getSimData again to get the newer revision.
 *
 * All returned pointers and references point into this instance. They stay valid only as long
 * as the shared pointer to this SimData exists.
 */
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

    /** Data about the simulator itself. std::nullopt if the simulator gave none */
    const std::optional<Sim>& getSim() const { return r_.sim; }

    /** Find a vehicle by its string id. nullptr if no vehicle has this id */
    const Vehicle*              getVehicle(std::string_view id) const;
    const std::vector<Vehicle>& getVehicles() const;

    /** Vehicle that the player drives
     *
     * Resolved through the current session. Returns nullptr if there is no current session,
     * if the session does not name a player vehicle, or if that vehicle is not in the data.
     */
    const Vehicle* getPlayerVehicle() const;
    const uint8_t* getVehiclesRawBson() const { return r_.vehicles_raw_bson; }

    /** Find a participant by its numeric id. nullptr if no participant has this id */
    const Participant*              getParticipant(int id) const;
    const std::vector<Participant>& getParticipants() const;

    /** Participant that represents the player
     *
     * Resolved through the current session, in the same way as getPlayerVehicle.
     */
    const Participant* getParticipantPlayer() const;
    const uint8_t*     getParticipantsRawBson() const { return r_.participant_raw_bson; }

    /** Find a track by its string id. nullptr if no track has this id */
    const Track*              getTrack(std::string_view id) const;
    const std::vector<Track>& getTracks() const;

    /** Track of the current session
     *
     * Resolved through the current session, in the same way as getPlayerVehicle.
     */
    const Track* getCurrentTrack() const;

    /** Find a session by its string id. nullptr if no session has this id */
    const Session*              getSession(std::string_view id) const;
    const std::vector<Session>& getSessions() const;

    /** Session that is active now. nullptr if the simulator marks no session as active
     *
     * @note This is a sim_data::Session, which is a play session in the game. It is not an
     *       sc_api::Session, which is the connection to Tuner.
     */
    const Session* getCurrentSession() const;

    const std::vector<Tire>& getTires() const;

    /** Find a tire by its numeric id. nullptr if no tire has this id */
    const Tire*    getTire(int id) const;
    const uint8_t* getTiresRawBson() const { return r_.tires_raw_bson; }

    /** Revision counter of this data. It increases every time Tuner replaces the sim data */
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
    return r.tryFindAndGet(ref.name, val);
}

}  // namespace sim_data

}  // namespace sc_api

#endif  // SC_API_SIM_DATA_H_
