/**
 * @file
 * @brief Version of the API implementation and the oldest Tuner version it supports.
 *
 * The SC_API_CORE_VERSION macros give the version of the API implementation itself. The API sends this version to
 * Simucube Tuner when it registers a session.
 *
 * This version is not the version of the sc-api package. The package version comes from
 * the CMake project and Doxygen shows it as the project number.
 *
 * sc_api::k_min_backend_version is the oldest Tuner version that this API version supports. Compare it with
 * sc_api::ApiCore::getBackendVersion or sc_api::Session::getBackendVersion.
 */

#ifndef SC_API_VERSION_H_
#define SC_API_VERSION_H_
#include <cstdint>

#define SC_API_CORE_VERSION_MAJOR 0
#define SC_API_CORE_VERSION_MINOR 8
#define SC_API_CORE_VERSION_PATCH 0

namespace sc_api {

/** Version of Simucube Tuner, in the form major.minor.patch */
struct BackendVersion {
    uint16_t major = 0;
    uint16_t minor = 0;
    uint16_t patch = 0;

    constexpr bool operator==(const BackendVersion& b) const {
        return major == b.major && minor == b.minor && patch == b.patch;
    }
    constexpr bool operator!=(const BackendVersion& b) const { return !(*this == b); }
    constexpr bool operator<(const BackendVersion& b) const {
        if (major != b.major) return major < b.major;
        if (minor != b.minor) return minor < b.minor;
        return patch < b.patch;
    }
    constexpr bool operator>(const BackendVersion& b) const { return b < *this; }
    constexpr bool operator<=(const BackendVersion& b) const { return !(b < *this); }
    constexpr bool operator>=(const BackendVersion& b) const { return !(*this < b); }
};

/** Oldest Tuner version that this API version supports */
inline constexpr BackendVersion k_min_backend_version{3, 2, 0};

}  // namespace sc_api

#endif  // SC_API_VERSION_H_
