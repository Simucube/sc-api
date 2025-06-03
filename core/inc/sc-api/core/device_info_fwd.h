#ifndef SC_API_CORE_DEVICE_INFO_FWD_H
#define SC_API_CORE_DEVICE_INFO_FWD_H

#include <memory>

namespace sc_api::core {

/** Wrapper class for a pointer to a valid bson document
 */
struct BsonBuffer {
    explicit operator bool() const { return bson != nullptr; }

    int32_t size() const {
        if (bson) {
            return *(const int32_t*)bson.get();
        }
        return 0;
    }
    std::shared_ptr<const uint8_t[]> bson;
};

struct BsonDeviceInfo {
    /** Device info data in BSON format */
    BsonBuffer data;

    /** This is copy of the revision number */
    uint32_t main_revision_number;
};

namespace device_info {

class DeviceInfo;
class FullInfo;

/** Shared pointer to DeviceInfo that shares the control block with the FullInfo
 *
 * As long as any of these pointers is alive, the whole ParsedInfo will stay valid.
 */
using DeviceInfoPtr = std::shared_ptr<const DeviceInfo>;

/** Shared pointer to full device information for all devices */
using FullInfoPtr   = std::shared_ptr<FullInfo>;

}  // namespace device_info
}  // namespace sc_api::core

#endif  // SC_API_CORE_DEVICE_INFO_FWD_H
