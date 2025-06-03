/**
 * file
 * brief
 *
 */

#ifndef SC_API_DEVICE_INFO_INTERNAL_H_
#define SC_API_DEVICE_INFO_INTERNAL_H_
#include "sc-api/core/device.h"
#include "sc-api/core/device_info.h"
#include "shm_bson_data_provider.h"

namespace sc_api::core {

class Session;

namespace internal {

class DeviceInfoProvider : public BsonShmDataProvider {
    friend class sc_api::core::Session;

public:
    DeviceInfoProvider();
    ~DeviceInfoProvider();

    void initialize(const void* shm_buffer, size_t shm_buffer_size);

    BsonBuffer getBsonDeviceInfo(uint32_t& revision_out) const { return getRawBson(revision_out); }

    /** Parse full device info into format that is easily readable
     *
     * FullInfo represents to current state at the time of calling this function and
     * won't be updated automatically when the underlying BSON data changes. Instead
     * this function should be called to get updated device information when necessary,
     * usually in response to the device_info_changed event.
     *
     * Parsed device info is cached and if this is called multiple times without device
     * info actually changing, the same pointer is returned.
     *
     * @note thread-safe
     *
     * @returns Parsed device information, or nullptr if device info cannot be fetched (session is disconnected)
     */
    std::shared_ptr<device_info::FullInfo> parseDeviceInfo();

private:
    bool parseAndValidateNewData(const uint8_t* new_data, std::size_t size) override;

    std::shared_ptr<device_info::FullInfo> cached_device_info_ = nullptr;
};

}  // namespace internal
}  // namespace sc_api::core

#endif  // SC_API_DEVICE_INFO_INTERNAL_H_
