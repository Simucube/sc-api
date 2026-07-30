#include "sc-api/dash_stream.h"

#include <atomic>
#include <chrono>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "compatibility.h"
#include "sc-api/command.h"
#include "sc-api/protocol/dash_stream.h"
#include "sc-api/util/bson_reader.h"

namespace sc_api {

namespace {
namespace dsp = sc_api::dash_stream_protocol;
}

struct DashStreamer::Impl {
    std::shared_ptr<sc_api::Session> session           = nullptr;
    uint16_t                         device_session_id = 0;
    sc_api::detail::SharedMemory     shm;
    std::size_t                      buffer_size = 0;

    // Backoff for the lazy open() path: open() issues a blocking command (up to a ~1 s timeout on an
    // unresponsive backend), so without a cooldown a per-frame render loop would re-attempt — and
    // potentially block — on every frame while disconnected. steady_clock for monotonic local timing
    // (Clock is QPC/cross-process and stubbed off-Windows, wrong tool for a local cooldown).
    std::optional<std::chrono::steady_clock::time_point> last_failed_open;
    static constexpr auto k_open_retry_interval = std::chrono::milliseconds{500};

    Impl(std::shared_ptr<sc_api::Session> sess, uint16_t dev_id)
        : session(std::move(sess)), device_session_id(dev_id) {}

    ~Impl() {
        if (!session || !shm.isOpen()) {
            return;
        }
        // Fire-and-forget release. Backend also cleans up on controller disconnect,
        // so a failed async send here is harmless. Placed here (not in ~DashStreamer)
        // so move-assignment also triggers cleanup of the replaced Impl.
        sc_api::CommandRequest req{dsp::k_service_name, dsp::k_cmd_release_buffer};
        req.docAddElement(dsp::k_field_device_session_id, device_session_id);
        session->asyncCommand(std::move(req), [](const sc_api::AsyncCommandResult&) {});
    }

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
    Impl(Impl&&)                 = delete;
    Impl& operator=(Impl&&)      = delete;
};

DashStreamer::DashStreamer(std::shared_ptr<sc_api::Session> session, uint16_t device_session_id)
    : impl_(std::make_unique<Impl>(std::move(session), device_session_id)) {
    if (!impl_->session) {
        return;
    }
}

// Release logic lives in Impl::~Impl() so that move-assignment of DashStreamer
// also triggers cleanup of the replaced buffer (unique_ptr destroys old Impl).
DashStreamer::~DashStreamer()                                        = default;

DashStreamer::DashStreamer(DashStreamer&& other) noexcept            = default;
DashStreamer& DashStreamer::operator=(DashStreamer&& other) noexcept = default;

bool DashStreamer::isValid() const { return impl_ && impl_->shm.isOpen(); }

bool DashStreamer::open() {
    if (isValid()) return true;
    if (!impl_ || !impl_->session) return false;

    sc_api::CommandRequest req{dsp::k_service_name, dsp::k_cmd_request_buffer};
    req.docAddElement(dsp::k_field_device_session_id, impl_->device_session_id);
    req.docAddElement(dsp::k_field_version, k_dash_frame_shm_version);
    req.docAddElement(dsp::k_field_format, dsp::k_format_rgb565);

    auto response = impl_->session->blockingCommand(std::move(req));
    if (response.isSuccess()) {
        sc_api::util::BsonReader reader(response.getPayload().data(), response.getPayload().size());
        std::string_view         shm_path;
        int32_t                  buffer_size = 0;
        if (reader.tryFindAndGet(dsp::k_field_shm_path, shm_path) &&
            reader.tryFindAndGet(dsp::k_field_buffer_size, buffer_size) && buffer_size > 0) {
            if (impl_->shm.openForReadWrite(std::string(shm_path).c_str(), static_cast<std::size_t>(buffer_size))) {
                impl_->buffer_size      = static_cast<std::size_t>(buffer_size);

                auto* shm_data          = static_cast<DashFrameShm*>(impl_->shm.getBuffer());
                shm_data->version       = k_dash_frame_shm_version;
                shm_data->feature_flags = 0;
                std::memset(shm_data->reserved_, 0, sizeof(shm_data->reserved_));
                impl_->last_failed_open.reset();
                return true;
            }
        }
    }

    impl_->last_failed_open = std::chrono::steady_clock::now();
    return false;
}

FrameResult DashStreamer::streamFrame(uint16_t width, uint16_t height, const uint16_t* rgb565_data) {
    if (!rgb565_data) {
        return FrameResult::failed;
    }
    if (!tryOpenIfNecessary()) return FrameResult::failed;

    // Check whether the requested frame fits the allocated shared-memory buffer.
    // Reject oversize frames instead of corrupting memory past the mapping.
    const std::size_t frame_size    = static_cast<std::size_t>(width) * height * 2;
    const std::size_t required_size = sizeof(DashFrameShm) + frame_size;
    if (required_size > impl_->buffer_size) {
        return FrameResult::failed;
    }

    auto* shm_data                  = static_cast<DashFrameShm*>(impl_->shm.getBuffer());

    // Read current SHM revision as the single source of truth
    const uint32_t current_revision = shm_data->revision.load(std::memory_order_acquire);
    if ((current_revision % 2) == 1) {
        // Odd = backend has not yet consumed the previous frame. Drop this one.
        return FrameResult::dropped;
    }

    shm_data->offset_x = 0;
    shm_data->offset_y = 0;
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
    sc_api::CommandRequest req{dsp::k_service_name, dsp::k_cmd_stop_stream};
    req.docAddElement(dsp::k_field_device_session_id, impl_->device_session_id);
    impl_->session->asyncCommand(std::move(req), [](const sc_api::AsyncCommandResult&) {});
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
    fb.last_ack_time = Clock::time_point{Clock::duration{shm_data->last_ack_time.load(std::memory_order_acquire)}};
    return fb;
}

std::optional<uint16_t> DashStreamer::getDeviceSessionId() const {
    if (!impl_) {
        return std::nullopt;
    }
    return impl_->device_session_id;
}

bool DashStreamer::tryOpenIfNecessary() {
    if (isValid()) return true;
    if (!impl_) return false;
    // Rate-limit reconnect attempts so a disconnected stream doesn't block the caller's render loop
    // on a blocking open() every frame. Explicit open() is not throttled — only this lazy path.
    if (impl_->last_failed_open &&
        (std::chrono::steady_clock::now() - *impl_->last_failed_open) < Impl::k_open_retry_interval) {
        return false;
    }
    return open();
}

}  // namespace sc_api
