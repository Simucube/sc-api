/**
 * file
 * brief
 *
 */

#ifndef SC_API_PROTOCOL_ACTIONS_H_
#define SC_API_PROTOCOL_ACTIONS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code
#define SC_API_PROTOCOL_UDP_PROTOCOL_VERSION_MAJOR 0

typedef enum SC_API_PROTOCOL_Action_e {
    SC_API_PROTOCOL_ACTION_FB_EFFECT                = 0x1,
    SC_API_PROTOCOL_ACTION_FB_EFFECT_CLEAR          = 0x2,
    SC_API_PROTOCOL_ACTION_REGISTER_TELEMETRY_GROUP = 0x1000,
    SC_API_PROTOCOL_ACTION_SET_TELEMETRY_GROUP      = 0x1001,

    /** Temporary effects for AP API testing */
    SC_API_PROTOCOL_ACTION_TEMP_AP_EFFECTS          = 10,

    SC_API_PROTOCOL_ACTION_TEMP_TELEMETRY_DATA      = 29,
} SC_API_PROTOCOL_Action_t;

typedef enum SC_API_PROTOCOL_ActionFlag_e {
    SC_API_PROTOCOL_ACTION_FLAG_NONE      = 0,

    /** Packet is AES-128-GCM encrypted by secret key generated during registering and exchanging public keys
     *
     * Command header is in this case followed by 12 bytes of random IV and 12 byte authentication tag.
     * These have to be considered when calculating maximum possible packet size.
     */
    SC_API_PROTOCOL_ACTION_FLAG_ENCRYPTED = 1 << 0,
} SC_API_PROTOCOL_ActionFlag_t;

typedef struct SC_API_PROTOCOL_EncryptedActionHeader_s {
    uint8_t iv[12];
} SC_API_PROTOCOL_EncryptedActionHeader_t;

typedef struct SC_API_PROTOCOL_EncryptedActionFooter_s {
    uint8_t tag[12];
} SC_API_PROTOCOL_EncryptedActionFooter_t;

typedef struct SC_API_PROTOCOL_ActionHeader_s {
    /** Controller id that is used in SC_API_PROTOCOL_RegisteredController_t
     *
     *  Must be set to valid id listed in SC_API_PROTOCOL_RegisteredController_t for all
     *  commands except REGISTER.
     */
    uint16_t controller_id;

    /** Combination of SC_API_PROTOCOL_ActionFlag_t */
    uint16_t flags;

    /** SC_API_PROTOCOL_Action_t action id that defines the content of the data */
    uint16_t action_id;

    /** Size of the action packet. Includes this header and any extra headers related to encrypted packets */
    uint16_t size;
} SC_API_PROTOCOL_ActionHeader_t;

typedef enum SC_API_PROTOCOL_FbEffectFormat_e {
    /** Offset to the ActivePedal force output in 32-bit float values and in newtons
     *
     *  Positive values push pedal toward the driver and negative pull it back.
     *
     *  Requires input type: "active_pedal"
     */
    SC_API_PROTOCOL_FB_EFFECT_PEDAL_FORCE_OFFSET_F32_N       = 0,

    /** Force offset relative to the current pedal force measurement
     *
     * 0.05 ==  Push towards user with 5% more force than user currently uses
     * -0.05 == reduce force by 5%.
     *
     * Usually this is only used for force reducion
     *
     *
     * Requires input type: "active_pedal"
     */
    SC_API_PROTOCOL_FB_EFFECT_PEDAL_RELATIVE_OFFSET_F32      = 1,

    /** Add offset to the force curve position. Limited by physical movement range of the pedal
     *
     * Positive offset moves pedal towards the driver.
     *
     *
     * Requires input type: "active_pedal"
     */
    SC_API_PROTOCOL_FB_EFFECT_PEDAL_POSITION_OFFSET_F32_MM   = 2,

    /** Add torque offset for a wheelbase
     *
     * Positive values turn wheel clockwise and negative counter-clockwise.
     *
     * Requires input type: "wheelbase"
     */
    SC_API_PROTOCOL_FB_EFFECT_WHEELBASE_TORQUE_OFFSET_F32_NM = 3,

    /** Add rumble vibrations
     *
     * Requires input type: "rumble"
     */
    // SC_API_PROTOCOL_EFFECT_RUMBLE_FORCE_AND_FREQ_HZ = 4,
} SC_API_PROTOCOL_FbEffectFormat_t;

typedef enum SC_API_PROTOCOL_FbSampleFormat_e {
    SC_API_PROTOCOL_FB_SAMPLE_FORMAT_F32 = 0,

    /** Effect pipeline considers i16 values as -1.0 - 1.0 values so using gain parameter when configuring the pipeline
       is important */
    SC_API_PROTOCOL_FB_SAMPLE_FORMAT_I16 = 1,

    /** Effect pipeline considers u16 values as 0 - 1.0 values so using gain parameter when configuring the pipeline
       is important */
    SC_API_PROTOCOL_FB_SAMPLE_FORMAT_U16 = 2,
} SC_API_PROTOCOL_FbSampleFormat_t;

#define SC_API_PROTOCOL_COMMAND_EFFECT_MAX_SAMPLE_COUNT 256

/** Part of the action that is never encrypted, but is part of tag calculation and authenticated */
typedef struct SC_API_PROTOCOL_ActionFbEffect_AAD_s {
    /** Index of the feedback pipeline that is this effect is put
     *
     * Each pipeline can have only one effect active so next action with the same pipeline index overrides the
     * previous. Effects from multiple pipelines are combined.
     */
    uint8_t fb_pipeline_idx;

    uint8_t flags;

    uint16_t reserved_0 = 0;
    uint32_t reserved_1 = 0;
    uint32_t reserved_2 = 0;
    uint32_t reserved_3 = 0;

} SC_API_PROTOCOL_ActionFbEffect_AAD_t;

/** Part of the action that is encrypted */
typedef struct SC_API_PROTOCOL_ActionFbEffect_Enc_s {
    uint8_t sample_format;

    /** High bits of sample duration if sample duration is more than 0xffffffffu ns*/
    uint8_t sample_duration_high = 0;

    /** Number of samples in this action
     *
     * Total duration of the effect is sample_duration * (sample_count_minus_1 + 1) and effect will stop if new ffb
     * effect isn't given before that
     */
    uint16_t sample_count_minus_1;

    /** Ticks between samples
     *
     * First sample is used immediately when the device receives this ffb action and next after sample_duration ticks.
     * If SC_API_PROTOCOL_FFB_EFFECT_FLAG_LIN_INTRP flag is set, then interpolates the offsets linearly between these
     * samples
     */
    uint32_t sample_duration;

    /** Start timestamp when the effect should activate
     */
    uint32_t start_time_low;
    uint32_t start_time_high;
    // This is followed by sample_count of values in the format defined by SC_API_PROTOCOL_EffectFormat_t
} SC_API_PROTOCOL_ActionFbEffect_Enc_t;

typedef struct SC_API_PROTOCOL_ActionFbClear_Enc_s {
    uint8_t cleared_pipeline_count;

    /** Index of the feedback pipelines that will be cleared */
    uint8_t fb_pipelines[31];
} SC_API_PROTOCOL_ActionFbClear_Enc_t;

// NOLINTEND(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_ACTIONS_H_
