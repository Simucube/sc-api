#ifndef SC_API_PROTOCOL_TELEMETRY_H
#define SC_API_PROTOCOL_TELEMETRY_H

#include <stdint.h>

#include "core.h"
#include "sc-api/core/protocol/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code

typedef enum SC_API_PROTOCOL_TelemetryFlag_e {
    /** Data that is potentially used for effects
     *
     * This telemetry data should be given with minimal latency
     */
    SC_API_PROTOCOL_TELEMETRY_USED_FOR_EFFECTS = 1u << 0,

    /** Used for dashes and leds
     *
     * Latency isn't that critical so updating this telemetry can be done at lower rate.
     */
    SC_API_PROTOCOL_TELEMETRY_USED_FOR_DISPLAY = 1u << 1,

    /** Telemetry type that shouldn't anymore used, but is kept for backwards compatibility reasons */
    SC_API_PROTOCOL_TELEMETRY_DEPRECATED       = 1u << 2,
} SC_API_PROTOCOL_TelemetryFlag_t;

typedef struct SC_API_PROTOCOL_TelemetryDef_s {
    /** Id number used to refer this telemetry in commands
     *
     * Unique in the session, but may change. name should be used as the
     */
    uint16_t id;

    /** Combination of SC_API_PROTOCOL_TelemetryFlag_t */
    uint16_t flags;

    /** Type of the value. SC_API_Type_t */
    SC_API_PROTOCOL_Type_t type;
    SC_API_PROTOCOL_TypeVariantData_t type_variant_data;

    /** Index of the variable that represents the current state of this telemetry
     *
     * Updating this telemetry will update this variable, if the telemetry update isn't encrypted.
     *
     * 0xffffffffu, if there isn't variable that would directly represent this telemetry
     */
    uint32_t alias_variable_idx;

    /** Identifying name of the telemetry. Null-terminated
     *
     *  This should be used to find the correct telemetry.
     */
    char name[36];
} SC_API_PROTOCOL_TelemetryDef_t;

#define SC_API_PROTOCOL_TELEMETRY_DEFINITION_SHM_ID 0x78d38efbu

#define SC_API_PROTOCOL_TELEMETRY_DEFINITION_SHM_VERSION 0x00000001u
typedef struct SC_API_PROTOCOL_TelemetryDefinitionShm_s {
    SC_API_PROTOCOL_ShmBlockHeader_t header;

    /** Offset from the start of this shared memory block to the telemetry definitions */
    uint32_t definition_offset;
    /** Size of a single variable definition */
    uint32_t definition_data_size;

    /** Number of variable definitions
     *
     * This can increase during session, but can never decrease and previous definitions
     * are not changed. Change to the definition_count is sequenced so that the data for
     * all definitions up to this value is completely valid when this is set.
     */
    uint32_t definition_count;

    /** Reserved flags */
    uint32_t flags;

    /** Reserved just to align this nicely to 32 bytes */
    uint32_t reserved;
} SC_API_PROTOCOL_TelemetryDefinitionShm_t;

// NOLINTEND(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using)

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_TELEMETRY_H
