#ifndef SC_API_INTERNAL_BACKEND_VERSION_H_
#define SC_API_INTERNAL_BACKEND_VERSION_H_
#include <cstdint>
#include <optional>

#include "sc-api/protocol/core.h"
#include "sc-api/version.h"

namespace sc_api::detail {

/** Read the backend version through the core block named `core_shm_name`. Empty unless the core block stays active
 *  and names the same session before and after this function reads the session block. */
std::optional<BackendVersion> readBackendVersion(const char* core_shm_name);

/** Backend version of a mapped session block. Empty unless the block has the known major version, is active, has
 *  the id and version that the core block gives, and declares a data size from its header size to `mapped_size`. */
inline std::optional<BackendVersion> sessionBackendVersion(const SC_API_PROTOCOL_Session_t* session,
                                                           uint32_t mapped_size, uint32_t expected_id,
                                                           uint32_t expected_version) {
    constexpr uint32_t k_header_size = sizeof(SC_API_PROTOCOL_Session_t);
    if (mapped_size < k_header_size) return std::nullopt;

    const bool is_named_active_session =
        SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(SC_API_PROTOCOL_SESSION_SHM_VERSION, session->session_version) &&
        session->session_id == expected_id && session->session_version == expected_version &&
        session->state == SC_API_PROTOCOL_SESSION_ACTIVE;
    const uint32_t data_size         = session->session_data_size;
    const bool     data_fits_mapping = data_size >= k_header_size && data_size <= mapped_size;
    if (!is_named_active_session || !data_fits_mapping) return std::nullopt;

    return BackendVersion{session->backend_version_major, session->backend_version_minor,
                          session->backend_version_patch};
}

}  // namespace sc_api::detail

#endif  // SC_API_INTERNAL_BACKEND_VERSION_H_
