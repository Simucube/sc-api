#ifndef SC_API_PROTOCOL_BSON_SHM_BLOCKS_H_
#define SC_API_PROTOCOL_BSON_SHM_BLOCKS_H_
#include "core.h"

// Shared memory blocks that contain one large BSON document share similar header

#define SC_API_PROTOCOL_DEVICE_INFO_SHM_ID      0x89765893u
#define SC_API_PROTOCOL_DEVICE_INFO_SHM_VERSION 0x00000001u

#define SC_API_PROTOCOL_SIM_DATA_SHM_ID      0x896f43a2u
#define SC_API_PROTOCOL_SIM_DATA_SHM_VERSION 0x00000001u

typedef struct SC_API_PROTOCOL_BsonDataShm_s {
    SC_API_PROTOCOL_ShmBlockHeader_t header;

    /** Offset to the start of the actual data */
    uint32_t data_offset;

    /** Size of the data */
    uint32_t data_size;

    /** Reserved flags */
    uint32_t flags;
} SC_API_PROTOCOL_BsonDataShm_t;

#endif  // SC_API_PROTOCOL_BSON_SHM_BLOCKS_H_
