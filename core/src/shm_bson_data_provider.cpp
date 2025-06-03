#include "shm_bson_data_provider.h"

#include <cstring>
#include <thread>

#include "compatibility.h"
#include "sc-api/core/protocol/bson_shm_blocks.h"
#include "sc-api/core/util/bson_reader.h"

namespace sc_api::core::internal {

BsonShmDataProvider::BsonShmDataProvider() {}

BsonShmDataProvider::~BsonShmDataProvider() {}

void BsonShmDataProvider::setShmBuffer(const uint8_t* buffer, size_t size) {
    shm_buffer_         = buffer;
    shm_buffer_size_    = size;
    active_buffer_size_ = 0;
    active_buffer_.reset();
    active_buffer_revision_ = 0;
    buffer_changed_         = true;
}

BsonBuffer BsonShmDataProvider::getRawBson(uint32_t& revision_out) const {
    std::shared_lock lock(mutex_);
    if (active_buffer_) {
        revision_out = active_buffer_revision_;
    }

    return BsonBuffer{active_buffer_};
}

bool BsonShmDataProvider::parseAndValidateNewData(const uint8_t* new_data, size_t size) { return true; }

BsonShmDataProvider::UpdateResult BsonShmDataProvider::update() {
    static constexpr int k_max_retries         = 3;

    bool     access_success                    = false;
    uint32_t old_revision                      = 0;
    uint32_t new_revision                      = 0;

    uint32_t                   new_buffer_size = 0;
    std::shared_ptr<uint8_t[]> new_buffer      = nullptr;

    {
        std::shared_lock lock{mutex_};
        if (!shm_buffer_) {
            return UpdateResult::failed;
        }
        old_revision                             = active_buffer_revision_;

        const SC_API_PROTOCOL_BsonDataShm_t* shm = (const SC_API_PROTOCOL_BsonDataShm_t*)shm_buffer_;

        for (int retries = 0; retries < k_max_retries; ++retries) {
            bool valid_data = false;
            access_success =
                internal::shmTryAtomicDataAccess(&shm->header, [&](const void* shm_data, uint32_t hdr_reported_size) {
                    const auto* shm = reinterpret_cast<const SC_API_PROTOCOL_BsonDataShm_t*>(shm_data);
                    if (shm->header.data_revision_counter == old_revision && !buffer_changed_) {
                        return true;
                    }

                    if (hdr_reported_size > shm_buffer_size_) {
                        // Shm header reports buffer size that is more than what we have actually mapped
                        valid_data = false;
                        return false;
                    }

                    new_revision              = shm->header.data_revision_counter;

                    const uint8_t* data_start = reinterpret_cast<const uint8_t*>(shm_data) + shm->data_offset;
                    uint32_t       data_size  = shm->data_size;

                    if ((uint64_t)data_size + shm->data_offset > hdr_reported_size) {
                        valid_data = false;
                        return false;
                    }
                    valid_data = true;

                    if (new_buffer_size != data_size) {
#if __cpp_lib_smart_ptr_for_overwrite >= 202002L
                        new_buffer = std::make_shared_for_overwrite<uint8_t[]>(data_size);
#elif __cpp_lib_shared_ptr_arrays >= 201707L
                        new_buffer = std::make_shared<uint8_t[]>(data_size);
#else
                        new_buffer = std::shared_ptr<uint8_t[]>(new uint8_t[data_size]);
#endif
                        new_buffer_size = data_size;
                    }

                    std::memcpy(new_buffer.get(), data_start, data_size);
                    return true;
                });

            if (access_success) {
                if (new_revision == 0) {
                    // data_revision_counter matched the previous version, so the data hasn't changed
                    return UpdateResult::no_new_data;
                }

                if (valid_data) {
                    break;
                }
            }

            // Give backend side some time to finish updating device data
            std::this_thread::yield();
        }
    }

    if (access_success) {
        if (new_buffer && !util::BsonReader::validate(new_buffer.get(), new_buffer_size)) {
            return UpdateResult::failed;
        }
        std::lock_guard lock(mutex_);
        if (old_revision != active_buffer_revision_) {
            // Some other thread updated active_buffer_ between us unlocking shared mutex and
            // quite unlikely thing but lets assume that they either updated it to newer or
            // the same as we so we just abort this for now. Next update will fix it if we guessed
            // wrong
            return UpdateResult::new_data;
        }

        if (!parseAndValidateNewData(new_buffer.get(), new_buffer_size)) {
            return UpdateResult::failed;
        }

        active_buffer_          = new_buffer;
        active_buffer_size_     = new_buffer_size;
        active_buffer_revision_ = new_revision;
        buffer_changed_         = false;

        return UpdateResult::new_data;
    }

    return UpdateResult::failed;
}

}  // namespace sc_api::core::internal
