/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_PROTOCOL_COMMANDS_H_
#define SC_API_PROTOCOL_COMMANDS_H_

#ifdef __cplusplus
extern "C" {
#endif

/** Response codes returned by commands */
typedef enum SC_API_PROTOCOL_ResponseCodes_e {
    SC_API_PROTOCOL_OK                        = 0,

    /** Command argument data is invalid */
    SC_API_PROTOCOL_ERROR_INVALID_ARGUMENT    = 1,

    /** Command request format is invalid */
    SC_API_PROTOCOL_ERROR_INVALID_FORMAT      = 2,

    /** Command isn't supported */
    SC_API_PROTOCOL_ERROR_NOT_SUPPORTED       = 3,

    /** Command failed because some resource is unavailable or limit is reached
     */
    SC_API_PROTOCOL_ERROR_NO_RESOURCE         = 4,

    /** First command must always be "core:register" */
    SC_API_PROTOCOL_ERROR_NOT_REGISTERED      = 5,

    /** Command requires control flag that wasn't requested or approved */
    SC_API_PROTOCOL_ERROR_NO_CONTROL          = 6,

    /** Internal communication occured within SC-API implementation
     *
     * Most likely device that should have received the command, disconnected before command was executed.
     */
    SC_API_PROTOCOL_ERROR_INTERNAL_COMM_ERROR = 7,

    /** API backend is not compatible with this version of the API */
    SC_API_PROTOCOL_ERROR_INCOMPATIBLE        = 8,

    /** Some unknown error occured within the SC-API implementation.
     *
     * This shouldn't occur and is a bug in the implementation.
     */
    SC_API_PROTOCOL_ERROR_INTERNAL            = 0xfff0,

    /** Last possible error code */
    SC_API_PROTOCOL_ERROR_MASK                = 0xffff,
} SC_API_PROTOCOL_ResponseCodes_t;

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_COMMANDS_H_
