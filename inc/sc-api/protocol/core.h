#ifndef SC_API_PROTOCOL_CORE_H_
#define SC_API_PROTOCOL_CORE_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// NOLINTBEGIN(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using) C compatible code

#define SC_API_PROTOCOL_CORE_SHM_FILENAME "$sc-api-core$"

typedef struct SC_API_PROTOCOL_ShmBlockHeader_s {
    /** Core version number for this basic information */
    uint32_t version;

    /** Incremented every time following data is started to be modified and second time when modification ends.
     *
     * Copied data should stay valid data_revision_counter is even and stays the same during copying.
     * Barriers must be used to guarantee correct ordering.
     */
    volatile uint32_t data_revision_counter;

    /** Size of this shared memory block */
    uint32_t shm_size;
} SC_API_PROTOCOL_ShmBlockHeader_t;

typedef struct SC_API_PROTOCOL_ShmBlockReference_s {
    /** Id number that states the content of this shared memory block */
    uint32_t id;

    /** Version of the shared memory block contents
     *
     * There can be multiple shared memory blocks with the same id but different major version number
     * for backwards compatiblity reasons.
     *
     */
    uint32_t version;

    /** Size of the shared memory block */
    uint32_t size;

    /** Path to shared memory file */
    char shm_path[64];
} SC_API_PROTOCOL_ShmBlockReference_t;

typedef enum SC_API_PROTOCOL_CoreState_e {
    SC_API_PROTOCOL_CORE_OFFLINE      = 0,
    SC_API_PROTOCOL_CORE_INITIALIZING = 1,
    SC_API_PROTOCOL_CORE_ACTIVE       = 2,
    SC_API_PROTOCOL_CORE_SHUTDOWN     = 3
} SC_API_PROTOCOL_CoreState_t;

typedef enum SC_API_PROTOCOL_SessionState_e {
    /** This shouldn't really be visible for users of API as the session reference is only added to core after session
     * shm is initialized
     */
    SC_API_PROTOCOL_SESSION_INITIALIZING = 0,

    /** Normal state */
    SC_API_PROTOCOL_SESSION_ACTIVE       = 1,

    /** Session has been cleanly shutdown*/
    SC_API_PROTOCOL_SESSION_SHUTDOWN     = 2,
} SC_API_PROTOCOL_SessionState_t;

typedef struct SC_API_PROTOCOL_PublicKeyHeader_s {
    /** Security method */
    uint16_t security_method;

    /** Size of the key in bytes */
    uint16_t key_size;

    /** Offset from the start of this struct to the key data */
    uint16_t key_offset;

    /** If key isn't signed then 0 */
    uint16_t signature_size;

    /** Offset from start of this struct to the signature data */
    uint16_t signature_offset;
} SC_API_PROTOCOL_PublicKeyHeader_t;
#define SC_API_PROTOCOL_MAX_PUBLIC_KEYS 8

/** Session specific configuration
 *
 * This data is fully filled and won't change (except keep_alive_counter) when the session shm path and id are added to
 * the core shm block.
 */
typedef struct SC_API_PROTOCOL_Session_s {
    /** This struct version. 16 MSB are major version number. If that isn't known value, session must be considered
     * incompatible with this API version */
    uint32_t session_version;

    /** Session ID number that will stay constant during session
     */
    uint32_t session_id;

    /** SC_API_PROTOCOL_SessionState_t  */
    volatile uint32_t state;

    /** Increased around every 100ms. This can be used to detect situations where the backend has died without properly
     * cleaning up session */
    volatile uint32_t keep_alive_counter;

    // ---------- CONSTANT DATA --------------
    // Following fields won't be updated during active session
    // so they can freely be copied to non-shared memory when session is started to avoid safety checks against
    // malicious modification of the shared memory

    /** Size of this shared memory data
     *
     *  Includes also data that isn't directly in this struct but is defined as offsets from
     */
    uint32_t session_data_size;

    /** Process ID that fills the data for this API */
    uint64_t manager_process_pid;

    uint32_t tcp_core_protocol_version;
    uint32_t tcp_core_feature_flags;
    uint8_t  tcp_core_address[4];
    uint16_t tcp_core_port;
    uint16_t tcp_core_reserved_padding_;
    uint32_t tcp_core_max_packet_size;

    uint32_t tcp_core_reserved_[4];

    /** Version number of the protocol
     *
     * 16 MSB are the major version number and API implementation only works with certain major API versions
     */
    uint32_t udp_control_protocol_version;

    uint32_t udp_control_feature_flags_available[4];

    /** 127.0.0.1 usually but could be some other local address as well. */
    uint8_t udp_control_address[4];

    /** Control protocol UDP port. Can change if the default port is reserved by some other program. */
    uint16_t udp_control_port;

    /** Maximum packet size for a command packet
     *
     * If encryption is used then udp_control_max_encrypted_packet_size limits the size.
     */
    uint16_t udp_control_max_plaintext_packet_size;

    /** Max total size of a encrypted packet
     *
     * Encrypted packets are decrypted on firmware and there are limitations to the total size packet that
     * firmware can handle. This includes encryption related header and footer data.
     *
     * Always less or equal to the udp_control_max_plaintext_packet_size.
     */
    uint16_t udp_control_max_encrypted_packet_size;

    // Just for alignment purposes now. Maybe used later for something
    uint16_t reserved_padding_;

    uint32_t udp_control_reserved_[4];

    /** Number of SC_API_PROTOCOL_ShmBlockReference_t that are somewhere with in this core shared memory block */
    uint16_t shm_reference_count;

    /** Size of SC_API_PROTOCOL_ShmBlockReference_t
     *
     *  Address of a SC_API_PROTOCOL_ShmBlockReference_t by index is:
     *  this_block_address + shm_reference_offset + (shm_reference_size * idx)
     */
    uint16_t shm_reference_size;

    /** Offset from the start of this shared memory block to the start of the first shm reference */
    int32_t shm_reference_offset;

    /** Offset from the start of this shared memory block to the SC_API_PROTOCOL_PublicKeyHeader_t structs
     *
     *  Unused entries are 0
     */
    uint16_t public_key_offsets[SC_API_PROTOCOL_MAX_PUBLIC_KEYS];
} SC_API_PROTOCOL_Session_t;
#define SC_API_PROTOCOL_SESSION_SHM_VERSION 0x00000001u

/** Minimum valid value for SC_API_PROTOCOL_Session_t::udp_control_max_plaintext_packet_size
 *
 * Actual value is normally more than that
 */
#define SC_API_PROTOCOL_UDP_CONTROL_MIN_PLAINTEXT_PACKET_SIZE_LIMIT 4096

/** Minimum valid value for SC_API_PROTOCOL_Session_t::udp_control_max_encrypted_packet_size
 *
 * Actual value is normally more than that
 */
#define SC_API_PROTOCOL_UDP_CONTROL_MIN_ENCRYPTED_PACKET_SIZE_LIMIT 1400

#define SC_API_PROTOCOL_CORE_SHM_SIZE 4096

typedef struct SC_API_PROTOCOL_Core_t {
    /** Core version number for this basic information */
    volatile uint32_t version;

    /** Increased by 1 when this shared memory block is started to be modified and again by 1 when modification is done
     */
    volatile uint32_t revision_counter;

    /** Id of the currently active session which data is in separate shared memory block
     *
     *
     * If this changes, that means the backend was restarted and all the shared blocks have new paths and data.
     */
    volatile uint32_t session_id;

    /** Version number of the session information*/
    volatile uint32_t session_version;

    volatile uint32_t session_shm_size;

    /** SC_API_PROTOCOL_CoreState_t that defines current state of the backend
     *
     * Generally users of the API should just wait until state is SC_API_PROTOCOL_CORE_ACTIVE. This does not
     * strictly guarantee that the backend is working correctly as if the backend is killed or crashes it may
     * not set the state back to OFFLINE so keep_alive_counter must be monitored to detect these situations.
     */
    volatile uint32_t state;

    /** Path to shared memory buffer that contains actual session data */
    volatile char session_shm_path[64];
} SC_API_PROTOCOL_Core_t;

#define SC_API_PROTOCOL_CORE_SHM_VERSION 0x00000001u

#define SC_API_PROTOCOL_IS_SHM_VERSION_COMPATIBLE(known_version, shm_version) \
    ((known_version & 0xffff0000u) == (shm_version & 0xffff0000))

#define SC_API_PROTOCOL_TCP_CORE_VERSION 0x00010000u

// NOLINTEND(hicpp-deprecated-headers,modernize-deprecated-headers,modernize-use-using)

#ifdef __cplusplus
}
#endif

#endif  // SC_API_PROTOCOL_CORE_H_
