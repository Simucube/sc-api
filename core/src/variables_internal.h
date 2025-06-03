/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_VARIABLES_INTERNAL_H_
#define SC_API_VARIABLES_INTERNAL_H_
#include <shared_mutex>

#include "sc-api/core/protocol/variables.h"
#include "sc-api/core/variables.h"

namespace sc_api::core {

struct VariableDefinitions::VariableDefChunk : std::enable_shared_from_this<VariableDefChunk> {
    struct VariableDefCopy {
        const void*     value_ptr;
        Type            type;
        uint32_t        flags;
        DeviceSessionId device_session_id;

        /** Guaranteed to be null-terminated*/
        char name[sizeof(SC_API_PROTOCOL_VariableDefinition_t::name)];

        std::string_view getName() const { return name; }
    };

    struct SearchKey {
        std::string_view name;
        DeviceSessionId  device_session_id;
        Type             type;
    };

    static constexpr uint32_t k_definitions_in_chunk = 1024;
    static constexpr uint32_t k_chunk_count          = 32;

    uint32_t variable_values_max_data_size           = 0;

    // Use separate k_definitions_in_chunk sized blocks to store copies of
    // definitions to always keep pointers to individual VariableDefinitions
    // valid. All definitions here are guaranteed to have null-terminated name and
    // value offset that points. Array is static so that we don't need to reallocate
    // it and accessing definitions can be done without locking
    std::unique_ptr<VariableDefCopy[]> defs[k_chunk_count];

    uint32_t def_count           = 0;
    uint32_t processed_def_count = 0;

    // Compare variable defs by type, device session id and name to allow binary searching
    std::vector<const VariableDefCopy*> search_map;
    static bool                         searchMapSortCmp(const VariableDefCopy* a, const VariableDefCopy* b);
    static bool                         searchMapByKeyCmp(const VariableDefCopy* var, const SearchKey& key);

    const VariableDefCopy& getDefByIdx(int idx) const;
};

namespace internal {

/** VariableProvider allows access to the definitions and values of shared memory variables.
 *
 * Shared memory variablers are the main method for communicating data from SC-API backend to the users of the API.
 *
 * Any direct pointers to the shared memory are valid as long as the session instance is alive.
 * Store std::shared_ptr<Session> with the pointers to avoid session change causing a crash.
 *
 * @note Don't create separate instance, use the instance that is constructed automatically by Api. Api::getVariable
 * @note thread-safe
 */
class VariableProvider {
    friend class VariableHandle;

public:
    using VariableDefChunk = VariableDefinitions::VariableDefChunk;
    using VariableDefCopy  = VariableDefChunk::VariableDefCopy;

    /** Sets session handle and update all VariableHandles to point to new session variables if possible
     *
     * @note thread-safe
     */
    void initialize(Session* session, const void* def_shm_buffer, size_t def_shm_buffer_size,
                    const void* value_shm_buffer, size_t value_shm_buffer_size);

    /** Update known variable definitions and VariableHandles
     *
     * This must be called to get access to any new variables that are made available.
     * Api will call this periodically to keep everything updated.
     *
     * @note thread-safe
     */
    bool updateDefinitions();

    /** Get access to currently known variables
     *
     * @note thread-safe
     */
    VariableDefinitions definitions() const;

    /** Would calling definitions() give different results than the given VariableDefinitions?
     *
     * @note thread-safe
     */
    bool haveDefinitionsChanged(const VariableDefinitions& defs);

private:
    bool refreshDefinitions();

    mutable std::shared_mutex m_;

    std::shared_ptr<VariableDefChunk> def_chunk_;

    Session* session_;
    uint8_t* var_values_                  = nullptr;

    const void*    variable_def_header    = nullptr;
    const uint8_t* variable_defs_start    = nullptr;
    const uint8_t* variable_values_start  = nullptr;
    uint32_t       variable_def_size      = 0;
    uint32_t       max_variable_def_count = 0;
};

}  // namespace internal
}  // namespace sc_api::core

#endif  // SC_API_VARIABLES_INTERNAL_H_
