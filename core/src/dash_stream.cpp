#include "sc-api/core/dash_stream.h"

#include <atomic>
#include <cstring>
#include <string>
#include <utility>

#include "compatibility.h"
#include "sc-api/core/command.h"
#include "sc-api/core/protocol/dash_stream.h"
#include "sc-api/core/util/bson_reader.h"

namespace sc_api::core {

namespace {
namespace dsp = sc_api::core::dash_stream_protocol;
}

struct DashStreamer::Impl {
    std::shared_ptr<sc_api::core::Session> session           = nullptr;
    uint16_t                               device_session_id = 0;
    sc_api::core::internal::SharedMemory   shm;
    std::size_t                            buffer_size = 0;

    Impl(std::shared_ptr<sc_api::core::Session> sess, uint16_t dev_id)
        : session(std::move(sess)), device_session_id(dev_id) {}

    ~Impl() {
        if (!session || !shm.isOpen()) {
            return;
        }
        // Fire-and-forget release. Backend also cleans up on controller disconnect,
        // so a failed async send here is harmless. Placed here (not in ~DashStreamer)
        // so move-assignment also triggers cleanup of the replaced Impl.
        sc_api::core::CommandRequest req{dsp::k_service_name, dsp::k_cmd_release_buffer};
        req.docAddElement(dsp::k_field_device_session_id, device_session_id);
        session->asyncCommand(std::move(req), [](const sc_api::core::AsyncCommandResult&) {});
    }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&)                 = delete;
    Impl& operator=(Impl&&)      = delete;
};

DashStreamer::DashStreamer(std::shared_ptr<sc_api::core::Session> session, uint16_t device_session_id)
    : impl_(std::make_unique<Impl>(std::move(session), device_session_id)) {
    if (!impl_->session) {
        return;
    }

    sc_api::core::CommandRequest req{dsp::k_service_name, dsp::k_cmd_request_buffer};
    req.docAddElement(dsp::k_field_device_session_id, device_session_id);

    // WARNING: blockingCommand has no timeout — hangs if backend is unresponsive (GOR-790).
    auto response = impl_->session->blockingCommand(std::move(req));
    if (!response.isSuccess()) {
        // Failed to request buffer
        return;
    }

    sc_api::core::util::BsonReader reader(response.getPayload().data(), response.getPayload().size());
    std::string_view               shm_path;
    int32_t                        buffer_size = 0;
    if (reader.tryFindAndGet(dsp::k_field_shm_path, shm_path) &&
        reader.tryFindAndGet(dsp::k_field_buffer_size, buffer_size) && buffer_size > 0) {
        if (impl_->shm.openForReadWrite(std::string(shm_path).c_str(), static_cast<std::size_t>(buffer_size))) {
            impl_->buffer_size = static_cast<std::size_t>(buffer_size);
        }
    }
}

// Release logic lives in Impl::~Impl() so that move-assignment of DashStreamer
// also triggers cleanup of the replaced buffer (unique_ptr destroys old Impl).
DashStreamer::~DashStreamer()                                        = default;

DashStreamer::DashStreamer(DashStreamer&& other) noexcept            = default;
DashStreamer& DashStreamer::operator=(DashStreamer&& other) noexcept = default;

bool DashStreamer::isValid() const { return impl_ && impl_->shm.isOpen(); }

FrameResult DashStreamer::streamFrame(uint16_t width, uint16_t height, const uint16_t* rgb565_data) {
    if (!isValid() || !rgb565_data) {
        return FrameResult::failed;
    }

    // Check whether the requested frame fits the allocated shared-memory buffer.
    // Reject oversize frames instead of corrupting memory past the mapping.
    const std::size_t frame_size    = static_cast<std::size_t>(width) * height * 2;
    const std::size_t required_size = sizeof(DashFrameShm) + frame_size;
    if (required_size > impl_->buffer_size) {
        return FrameResult::failed;
    }

    auto* shm_data                  = static_cast<DashFrameShm*>(impl_->shm.getBuffer());

    // Read current SHM revision as the single source of truth. No local shadow
    // of frame_revision — that previously drifted if the buffer was recreated.
    const uint32_t current_revision = shm_data->revision.load(std::memory_order_acquire);
    if ((current_revision % 2) == 1) {
        // Odd = backend has not yet consumed the previous frame. Drop this one.
        return FrameResult::dropped;
    }

    shm_data->width  = width;
    shm_data->height = height;

    std::memcpy(shm_data + 1, rgb565_data, frame_size);

    // Publish the new frame by flipping revision to odd. The backend's packet timer polls this
    // revision and bumps it back to even on copy-out.
    const uint32_t next_revision = current_revision + 1;
    shm_data->revision.store(next_revision, std::memory_order_release);

    return FrameResult::delivered;
}

void DashStreamer::stop() {
    if (!impl_ || !impl_->session) {
        return;
    }
    // Fire-and-forget controlled-stop hint. Mirrors the release_buffer pattern: a failed/lost send
    // is harmless because the firmware's autonomous fallback timeout still fires.
    sc_api::core::CommandRequest req{dsp::k_service_name, dsp::k_cmd_stop_stream};
    req.docAddElement(dsp::k_field_device_session_id, impl_->device_session_id);
    impl_->session->asyncCommand(std::move(req), [](const sc_api::core::AsyncCommandResult&) {});
}

bool DashStreamer::isOwner() const {
    if (!isValid()) {
        return false;
    }
    auto* shm_data = static_cast<DashFrameShm*>(impl_->shm.getBuffer());
    return shm_data->is_owner.load(std::memory_order_acquire) != 0;
}

StreamFeedback DashStreamer::getStreamFeedback() const {
    StreamFeedback fb;
    if (!isValid()) {
        return fb;
    }
    // Each feedback field is a single-writer (backend) atomic; acquire-load mirrors the backend's
    // release-store discipline. Telemetry-rate snapshot — see StreamFeedback docs.
    auto* shm_data          = static_cast<DashFrameShm*>(impl_->shm.getBuffer());
    fb.is_owner             = shm_data->is_owner.load(std::memory_order_acquire) != 0;
    fb.device_frame_counter = shm_data->device_frame_counter.load(std::memory_order_acquire);
    fb.dropped_count        = shm_data->dropped_count.load(std::memory_order_acquire);
    fb.last_ack_time_us     = shm_data->last_ack_time_us.load(std::memory_order_acquire);
    return fb;
}

std::optional<uint16_t> DashStreamer::getDeviceSessionId() const {
    if (!impl_) {
        return std::nullopt;
    }
    return impl_->device_session_id;
}

}  // namespace sc_api::core
