/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_PROPERTY_DEFINITION_H_
#define SC_API_CORE_PROPERTY_DEFINITION_H_
#include <string_view>

namespace sc_api {

struct PropertyRef {
    std::string_view name;
};

/** Strongly typed reference to a known property to help providing information in correct format */
template <typename Type>
struct TypedPropertyRef : public PropertyRef {
    using type = Type;
};

/** Wrapper that also includes classification for the purpose of the property so that sim properties cannot be provided
 * in vehicle info
 */
template <typename Type, typename Classification>
struct TypedAndClassifiedPropertyRef : TypedPropertyRef<Type> {};

class SimPropertyClass;
class VehiclePropertyClass;
class ParticipantPropertyClass;
class TrackPropertyClass;
class TirePropertyClass;
class SessionPropertyClass;

template <typename Type>
using SimPropertyRef = TypedAndClassifiedPropertyRef<Type, SimPropertyClass>;

template <typename Type>
using VehiclePropertyRef = TypedAndClassifiedPropertyRef<Type, VehiclePropertyClass>;

template <typename Type>
using ParticipantPropertyRef = TypedAndClassifiedPropertyRef<Type, ParticipantPropertyClass>;

template <typename Type>
using TrackPropertyRef = TypedAndClassifiedPropertyRef<Type, TrackPropertyClass>;

template <typename Type>
using TirePropertyRef = TypedAndClassifiedPropertyRef<Type, TirePropertyClass>;

template <typename Type>
using SessionPropertyRef = TypedAndClassifiedPropertyRef<Type, SessionPropertyClass>;

}  // namespace sc_api

#endif  // SC_API_PROPERTY_DEFINITION_H_
