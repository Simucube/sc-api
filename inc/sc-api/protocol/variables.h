#ifndef SC_API_PROTOCOL_VARIABLES_H_
#define SC_API_PROTOCOL_VARIABLES_H_

#include <stdint.h>

#include "core.h"
#include "variable_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SC_API_PROTOCOL_VARIABLE_HEADER_SHM_ID      0x85532367u
#define SC_API_PROTOCOL_VARIABLE_HEADER_SHM_VERSION 0x00000001u
typedef struct SC_API_PROTOCOL_VariableHeaderShm_s {
    SC_API_PROTOCOL_ShmBlockHeader_t header;

    /** Offset from the start of this shared memory block to the variable definitions */
    uint32_t var_definition_offset;
    /** Size of a single variable definition */
    uint32_t var_definition_data_size;

    /** Number of variable definitions
     *
     * This can increase for example when new device is connected, but will never decrease
     * and variable offsets are never changed.
     */
    uint32_t var_definition_count;

    /** Reserved flags */
    uint32_t flags;

    /** Reserved just to align this nicely to 32 bytes */
    uint32_t reserved;
} SC_API_PROTOCOL_VariableDefinitionsShm_t;

#define SC_API_PROTOCOL_VARIABLE_DATA_SHM_ID      0x85782367u
#define SC_API_PROTOCOL_VARIABLE_DATA_SHM_VERSION 0x00000001u
typedef struct SC_API_PROTOCOL_VariableDataShm_s {
    SC_API_PROTOCOL_ShmBlockHeader_t header;

    /** Offset to the start of the variable data
     *
     * Offsets to the individual values are in the status header data.
     */
    uint32_t var_data_offset;

    /** Size of the block containing all values of variables */
    uint32_t var_data_size;

    /** Reserved space for flag bits to align timestamp_us to 64bits */
    uint32_t flags;
} SC_API_PROTOCOL_VariableDataShm_t;

typedef struct SC_API_PROTOCOL_VariableDefinition_s {
    /** Combination of SC_API_VariableFlag_t
     *
     *  Only field that can changed after variable definition first appears. (Just validity bit)
     */
    uint32_t flags;

    /** Type of the value. SC_API_PROTOCOL_Type_t */
    SC_API_PROTOCOL_Type_t type;

    /** Type variant specific data eg array size or bit index */
    SC_API_PROTOCOL_TypeVariantData_t type_variant_data;

    /** Offset from the start of the shared memory buffer to the value
     *
     * The value_offset isn't necessarily unique. For backwards compatibility reasons there can be multiple variable
     * definitions pointing to the same data or one variable may point to a single element in the array that is exposed
     * by another variable definition.
     *
     * Offsets are guaranteed to be aligned so that the value is suitably aligned for atomic access of a single element
     */
    uint32_t value_offset;

    /** Id of the logical device that this variable belongs to
     *
     * 0, if it isn't related to any logical device. For example telemetry status data that is used by multiple devices
     * belongs to this category
     *
     * Variables that are related to some logical devices only have SC_API_VAR_FLAG_VALID flag
     * when that specific device is connected.
     */
    uint16_t device_session_id;

    /** Id name of the variable
     *
     * Name is only unique in context of a logical_device. So different devices can and will have variables with the
     * same name. Guaranteed to be null-terminated to simplify usage.
     */
    char name[50];
} SC_API_PROTOCOL_VariableDefinition_t;

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_VARIABLES_H_
