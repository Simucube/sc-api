/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_SHM_BSON_DATA_PROVIDER_H_
#define SC_API_SHM_BSON_DATA_PROVIDER_H_
#include <shared_mutex>

#include "sc-api/core/device_info_fwd.h"

namespace sc_api::core::internal {

class BsonShmDataProvider {
public:
    enum UpdateResult {
        new_data,
        no_new_data,

        failed
    };

    BsonShmDataProvider();
    virtual ~BsonShmDataProvider();

    /** Sets the pointer to shared memory buffer
     */
    void setShmBuffer(const uint8_t* buffer, std::size_t size);

    /** Updates bson information from shared memory
     *
     * @return new_data if there was new data available and information has changed
     *         no_new_data, if the data hasn't changed since the last call
     *         failed, reading data failed
     */
    UpdateResult update();

    /** Get the full data in raw BSON format
     *
     *
     * @param[out] revision_out Revision number of the data returned
     *                          If revision number stays same between calls, the returned
     *                          shared_ptr will point to the same data and data isn't modified
     * @return Pointer to raw BSON data representing the full device information or nullptr
     *         if there isn't data available
     */
    BsonBuffer getRawBson(uint32_t& revision_out) const;

protected:
    /** This is called during update() if new valid BSON data is received.
     *
     * Mutex is exclusively locked during this so validation must be quick.
     *
     * @return true, if data is valid and is accepted
     *         false, if data is invalid. Active buffer won't be updated and update() returns UpdateResult::failed
     *
     * Default implementation does nothing and returns true.
     */
    virtual bool parseAndValidateNewData(const uint8_t* new_data, std::size_t size);

    mutable std::shared_mutex mutex_;

    // mutex_ must be locked for these
    const std::shared_ptr<uint8_t[]>& getActiveBuffer() const { return active_buffer_; }
    uint32_t                          getActiveBufferSize() const { return active_buffer_size_; }
    uint32_t                          getActiveBufferRevision() const { return active_buffer_revision_; }

private:
    const void* shm_buffer_      = nullptr;
    std::size_t shm_buffer_size_ = 0;

    std::shared_ptr<uint8_t[]> active_buffer_;
    uint32_t                   active_buffer_size_     = 0;
    uint32_t                   active_buffer_revision_ = 0;
    bool                       buffer_changed_         = false;
};

}  // namespace sc_api::core::internal

#endif  // SC_API_SHM_BSON_DATA_PROVIDER_H_
