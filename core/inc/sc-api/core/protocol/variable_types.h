#ifndef SC_API_PROTOCOL_VARIABLE_TYPES_H
#define SC_API_PROTOCOL_VARIABLE_TYPES_H

#include "sc-api/core/protocol/types.h"

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code

typedef enum SC_API_VariableFlag_e {
    SC_API_VAR_FLAG_NONE             = 0,

    /** This variable is guaranteed to stay supported and available for future releases */
    SC_API_VAR_FLAG_STABLE           = 1 << 0,

    /** Variable is actually constant related to the device that won't change when device is connected.
     *
     * These can still change if device firmware is updated or device is otherwise rebooted.
     */
    SC_API_VAR_FLAG_DEVICE_CONSTANT  = 1 << 2,

    /** Constant that won't change unless SC-API backend is restarted */
    SC_API_VAR_FLAG_SESSION_CONSTANT = 1 << 3,
} SC_API_VariableFlag_t;

// NOLINTEND(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using)

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_VARIABLE_TYPES_H
