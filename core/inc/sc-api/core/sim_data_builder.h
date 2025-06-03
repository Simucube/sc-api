#ifndef SC_API_CORE_SIM_DATA_BUILDER_H_
#define SC_API_CORE_SIM_DATA_BUILDER_H_
#include <cassert>
#include <string>

#include "sc-api/core/property_reference.h"
#include "util/bson_builder.h"

namespace sc_api::core::sim_data_builder {

class BuilderBase {
public:
    std::pair<const uint8_t*, int32_t> finish();

    void initialize();

protected:
    BuilderBase();

    void docAddNullHex16Key(int id);

    void docAddSubDocHex16Key(int id, const uint8_t* sub_doc);

    std::vector<uint8_t> buffer_;
    util::BsonBuilder    b_;
};
class TypedBuilderBase : public BuilderBase {
protected:
    void setImpl(std::string_view key, int32_t val);
    void setImpl(std::string_view key, int64_t val);
    void setImpl(std::string_view key, double val);
    void setImpl(std::string_view key, std::string_view val);
    void setImpl(std::string_view key, bool val);
};

template <typename PropertyClass>
class TypedBuilder : public TypedBuilderBase {
public:
    TypedBuilder() {}

    template <typename T>
    TypedBuilder& set(const TypedAndClassifiedPropertyRef<T, PropertyClass>&         ref,
                      typename TypedAndClassifiedPropertyRef<T, PropertyClass>::type value) {
        setImpl(ref.name, value);
        return *this;
    }
};

template <typename PropertyClass>
class TypedListBuilder : public BuilderBase {
public:
    TypedListBuilder() {}
    ~TypedListBuilder() {}

    /** Build an item with string id and it to the list
     *
     * @param id String that identifies the item. Should only use characters a-z and _.
     * @param builder Builder that contains data of the item
     *                Builder will be reinitialized to empty after data has been copied to this builder
     * @returns *this to enable chaining operations
     */
    TypedListBuilder& buildAndAdd(std::string_view id, TypedBuilder<PropertyClass>& builder) {
        assert(!id.empty());
        b_.docAddSubDoc(id, builder.finish().first);
        builder.initialize();
        return *this;
    }

    /** Encode removal of item with the given numberic id
     *
     * @param id String that identifies the item. Should only use characters a-z and _.
     * @returns *this to enable chaining operations
     */
    TypedListBuilder& addRemoval(std::string_view id) {
        assert(!id.empty());
        b_.docAddElement(id, nullptr);
        return *this;
    }
};

template <typename PropertyClass>
class TypedNumIdListBuilder : public BuilderBase {
public:
    /** Build an item with numeric id and it to the list
     *
     * @param id Number that identifies the item
     *           Must be between 1 and 0xfffe (inclusive). 0 and 0xffff are reserved.
     * @param builder Builder that contains data of the item
     *                Builder will be reinitialized to empty after data has been copied to this builder
     * @returns *this to enable chaining operations
     */
    TypedNumIdListBuilder& buildAndAdd(int id, TypedBuilder<PropertyClass>& builder) {
        assert(id > 0 && id < 0xffff);
        docAddSubDocHex16Key(id, builder.finish().first);
        builder.initialize();
        return *this;
    }

    /** Encode removal of item with the given numberic id
     *
     * @param id Number that identifies the item
     *           Must be between 1 and 0xfffe (inclusive). 0 and 0xffff are reserved.
     * @returns *this to enable chaining operations
     */
    TypedNumIdListBuilder& addRemoval(int id) {
        assert(id > 0 && id < 0xffff);
        docAddNullHex16Key(id);
        return *this;
    }
};

using VehiclesBuilder     = TypedListBuilder<VehiclePropertyClass>;
using VehicleBuilder      = TypedBuilder<VehiclePropertyClass>;
using ParticipantsBuilder = TypedNumIdListBuilder<ParticipantPropertyClass>;
using ParticipantBuilder  = TypedBuilder<ParticipantPropertyClass>;
using TracksBuilder       = TypedListBuilder<TrackPropertyClass>;
using TrackBuilder        = TypedBuilder<TrackPropertyClass>;
using TiresBuilder        = TypedNumIdListBuilder<TirePropertyClass>;
using TireBuilder         = TypedBuilder<TirePropertyClass>;
using SimBuilder          = TypedBuilder<SimPropertyClass>;
using SessionsBuilder     = TypedListBuilder<SessionPropertyClass>;
using SessionBuilder      = TypedBuilder<SessionPropertyClass>;

class SimDataUpdateBuilder : private BuilderBase {
public:
    /** Construct builder for constructing and serializing information about the sim and game session
     *
     *  @param sim_id Id of the simulator that this constructed data will refer
     *         This is used to avoid mismatched data when the simulator is changed.
     *  @param activate_sim If true, the backend is requested to activate this sim_id as the currently active sim
     *         Only one sim can be active at time, so this does not necessarily change the active sim if there are
     *         multiple different sources for this information.
     */
    SimDataUpdateBuilder(std::string_view sim_id, bool activate_sim);
    std::string_view getSimId() const { return sim_id_; }

    SimDataUpdateBuilder& buildAndSet(VehiclesBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(std::string_view vehicle_id, VehicleBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(ParticipantsBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(int participant_id, ParticipantBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(TracksBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(std::string_view track_id, TrackBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(TiresBuilder& builder);
    // tire_id (index) starts from one
    SimDataUpdateBuilder& buildAndSet(int tire_id, TireBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(SessionsBuilder& builder);
    SimDataUpdateBuilder& buildAndSet(std::string_view session_id, SessionBuilder& builder);

    SimDataUpdateBuilder& setActiveSession(std::string_view session_id);

    SimDataUpdateBuilder& buildAndSet(SimBuilder& builder);

    void                               initialize();
    std::pair<const uint8_t*, int32_t> finish();

private:
    std::string sim_id_;
    bool        activate_sim_;
};

}  // namespace sc_api::core::sim_data_builder

#endif  // SC_API_CORE_SIM_DATA_BUILDER_H_
