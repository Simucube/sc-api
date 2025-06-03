/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_TELEMETRY_H_
#define SC_API_CORE_TELEMETRY_H_
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "action.h"
#include "events.h"
#include "type.h"

namespace sc_api::core {

class Session;

struct TelemetryReferenceBase {
    std::string_view name;
};

template <typename T>
struct TelemetryReference : TelemetryReferenceBase {
    using type = T;
};

/** Definition of a single available telemetry
 *
 * Telemetries are uniquely identified by combination of name and type.
 * There can be multiple telemetries with same name for backwards compatibility purposes.
 *
 */
struct TelemetryDefinition {
    /** Name of the telemetry */
    std::string name;

    /** Type of the telemetry data. Currently only BaseTypes are used */
    Type        type;

    /** Numeric session specific id of the telemetry. Used in commands to refer to particular telemetry
     *
     * This id may change when Tuner is updated so it cannot be relied to stay the same. Use name and type to refer to
     * particular variable.
     */
    uint16_t    id          = 0;

    /** Reserved flags */
    uint16_t    flags       = 0;

    /** Index of the variable data that refers to this telemetry data
     *
     * Variable always represents the currently active state and may not necessarily update instantly when
     * TelemetryUpdateGroup is sent.
     */
    uint32_t    variable_idx = 0;
};

namespace internal {
class TelemetrySystem;
}

/** Base class for handles to telemetries */
class TelemetryBase {
    friend class TelemetryUpdateGroup;

public:
    TelemetryBase(std::string&& name, Type type);

    const std::string& getName() const { return name_; }
    Type               getType() const { return type_; }

    virtual const uint8_t* getSerializedValueBuf() const  = 0;
    virtual std::size_t    getSerializedValueSize() const = 0;

protected:
    /** Name is used to find correct telemetry data */
    std::string name_;

    Type        type_;

    /** This is filled and used by TelemetryUpdateGroup when it finds matching telemetry */
    struct RefState {
        uint16_t id;
        uint16_t flags;
    } ref_state_;
};

/** Reference to a telemetry data and the intended value for the telemetry
 *
 * Telemetry handle will correctly connect and update telemetry only if the name and type match exactly.
 */
template <typename T>
class Telemetry : public TelemetryBase {
public:
    explicit Telemetry(const TelemetryReference<T>& ref, T initial_value = {})
        : Telemetry(std::string(ref.name), initial_value) {}
    explicit Telemetry(std::string name, T initial_value = {})
        : TelemetryBase(std::move(name), get_base_type<T>::value) {
        setValue(initial_value);
    }
    Telemetry(const Telemetry&) = delete;
    Telemetry(Telemetry&&)      = delete;

    void setValue(T v) { value_serialized_ = v; }
    T    getValue() const { return value_serialized_; }

    const uint8_t* getSerializedValueBuf() const override {
        return reinterpret_cast<const uint8_t*>(&value_serialized_);
    }
    std::size_t    getSerializedValueSize() const override { return sizeof(T); }

private:
    T value_serialized_;
};

template <typename T>
Telemetry(const TelemetryReference<T>& ref, T initial_value = {}) -> Telemetry<T>;

class TelemetryDefinitions;

/** List of telemetry data that is sent as one complete set
 *
 * Telemetry data should be grouped by their update frequency so that
 * rarely changing data isn't unnecessarily sent during every update cycle.
 * Eg. "transmission_gear" changes much more rarely than the velocity.
 *
 * Single telemetry handle can belong to multiple update groups. Most recently received value for the telemetry is
 * always used.
 *
 * Currently if multiple controllers connect to the API, the older connections have priority
 * and more recently connected cannot override telemetry values provided by older connections, but they can still
 * provide new data.
 */
class TelemetryUpdateGroup {
public:
    /** Initialize update group and define its identifying number
     *
     * @param group_id Id number for this group within the session
     *                 Number must be unique so generally it is best to define for example enum that lists all used
     *                 update groups within the software and use that to generate group ids. Ids don't have to be
     *                 consecutive.
     */
    explicit TelemetryUpdateGroup(uint16_t group_id);

    /** Destructs the update group
     *
     *  If the group has been configured, then it is removed and telemetry values are set back to their default values.
     */
    ~TelemetryUpdateGroup();

    /** Set the list of telemetries that belong to this group. Configure must be called before value update can be sent
     *
     * @param telemetries List of the telemetries
     */
    void set(std::vector<TelemetryBase*> telemetries);

    /** Add given telemetry to the list of telemetries. Configure must be called before value update can be sent
     *
     * @param telemetry Telemetry that is appended to the telemetry list.
     */
    void add(TelemetryBase* telemetry);

    /** Add given list of telemetries to this group. Configure must be called before value update can be sent
     *
     * @param telemetries Telemetries that are appended to the telemetry list.
     */
    void add(const std::initializer_list<TelemetryBase*>& telemetries);

    /** Configure this group to contain previously given telemetries that are resolved using provided definitions
     *
     *  @param group_id Id of this update group. Used to identify this particular group during the session
     *  @param definitions Telemetry definitions that are used to resolve telemetry handles
     */
    bool configure(const TelemetryDefinitions& definitions);

    /** Configure this group to contain the given list of telemetries that are resolved using provided definitions
     *
     *  Same as calling TelemetryUpdateGroup::set followed by TelemetryUpdateGroup::configure
     *
     *  @param telemetries Telemetries that are included to this
     *  @param definitions Telemetry definitions that are used to resolve telemetry handles
     */
    bool configure(std::vector<TelemetryBase*> telemetries, const TelemetryDefinitions& definitions);

    /** Send all currently configured telemetries to the API backend
     *
     * configure must have completed successfully before this can succeed
     */
    ActionResult send();

    /** Get list of telemetries that have been added to this group
     *
     * Order of telemetries is undefined and can change when new telemetries are added or configure is called.
     */
    const std::vector<TelemetryBase*>& getTelemetries() const { return telemetries_; }

    uint16_t getId() const { return group_id_; }

    /** Disable this telemetry update group
     *
     * These telemetry values wont affect until configure() is called again.
     */
    ActionResult disable();

private:
    std::vector<TelemetryBase*> telemetries_;
    std::vector<const uint8_t*> base_value_bufs_;
    ActionBuilder               action_builder_;
    uint16_t                    base_value_entries_by_size_[5];
    uint16_t                    set_payload_size_ = 0;
    uint16_t                    group_id_         = 0;
    bool                        prepared_         = false;
    bool                        enabled_          = false;
};

/** List of all available telemetries
 *
 *
 */
class TelemetryDefinitions {
    friend class internal::TelemetrySystem;

public:
    using iterator               = std::vector<TelemetryDefinition>::iterator;
    using const_iterator         = std::vector<TelemetryDefinition>::const_iterator;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
    using value_type             = std::vector<TelemetryDefinition>::value_type;
    using pointer                = TelemetryDefinition*;
    using const_pointer          = const TelemetryDefinition*;
    using reference              = TelemetryDefinition&;
    using const_reference        = const TelemetryDefinition&;
    using size_type              = std::vector<TelemetryDefinition>::size_type;
    using difference_type        = std::vector<TelemetryDefinition>::difference_type;

    /** Creates empty telemetry definition list */
    TelemetryDefinitions();
    TelemetryDefinitions(const TelemetryDefinitions& defs)            = default;
    TelemetryDefinitions& operator=(const TelemetryDefinitions& defs) = default;

    const_iterator         begin() const { return s_->defs.begin(); }
    const_iterator         end() const { return s_->defs.begin(); }
    const_reverse_iterator rbegin() const { return s_->defs.rbegin(); }
    const_reverse_iterator rend() const { return s_->defs.rend(); }
    size_type              size() const { return s_->defs.size(); }

    const TelemetryDefinition* find(std::string_view name) const;
    const TelemetryDefinition* find(std::string_view name, Type type) const;
    const TelemetryDefinition* find(uint16_t id) const;

    std::shared_ptr<Session> getSession() const { return session_; }

private:
    struct DefStorage {
        std::vector<TelemetryDefinition> defs;
    };

    explicit TelemetryDefinitions(const std::shared_ptr<DefStorage>& defs, const std::shared_ptr<Session>& session);

    std::shared_ptr<DefStorage> s_;
    std::shared_ptr<Session>    session_;

    /** Empty storage for default constructed instances to avoid nullptr checks in every function */
    static std::shared_ptr<TelemetryDefinitions::DefStorage> s_empty_storage;
};

}  // namespace sc_api::core

#endif  // SC_API_TELEMETRY_H_
