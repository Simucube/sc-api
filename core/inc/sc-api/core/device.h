/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_DEVICE_H_
#define SC_API_CORE_DEVICE_H_
#include <sc-api/core/device_info_definitions.h>

#include <cstdint>

namespace sc_api::core {

/** Session specific identifier for a device
 *
 * Numeric id of the device will stay the same during the session even if the device connected and reconnects.
 *
 */
struct DeviceSessionId {
    uint16_t id = 0;

    constexpr explicit operator bool() const { return id != 0; }

    constexpr bool operator==(DeviceSessionId b) const { return b.id == id; }
    constexpr bool operator!=(DeviceSessionId b) const { return b.id != id; }

    // Not really useful except for std::map and similar that require some type of sorting
    constexpr bool operator<(DeviceSessionId b) const { return id < b.id; }
    constexpr bool operator>(DeviceSessionId b) const { return id > b.id; }
    constexpr bool operator<=(DeviceSessionId b) const { return id <= b.id; }
    constexpr bool operator>=(DeviceSessionId b) const { return id >= b.id; }
};

inline constexpr DeviceSessionId k_invalid_device_session_id = DeviceSessionId{0};

}  // namespace sc_api::core

namespace std {
template <>
struct hash<sc_api::core::DeviceSessionId> {
    std::size_t operator()(const sc_api::core::DeviceSessionId& s) const noexcept { return std::hash<uint16_t>()(s.id); }
};
}  // namespace std

#endif  // SC_API_CORE_DEVICE_H_
