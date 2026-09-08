#include "sc-api/input_events.h"

#include <utility>

#include "api_internal.h"
#include "compatibility.h"

namespace sc_api {

struct InputEventReader::Impl {
    std::shared_ptr<sc_api::Session> session;
    InputEventRingReadView           ring;
    std::uint64_t                    cursor = 0;

    explicit Impl(std::shared_ptr<sc_api::Session> sess) : session(std::move(sess)) {}

    Impl(const Impl&)            = delete;
    Impl& operator=(const Impl&) = delete;
};

namespace {

/** The block stays mapped after the session is lost, and the backend can still write it, so the
 *  state is what stops the reader. */
bool isSessionAlive(const std::shared_ptr<sc_api::Session>& session) {
    if (!session) {
        return false;
    }
    const SessionState state = session->getState();
    return state == SessionState::connected_monitor || state == SessionState::connected_control;
}

}  // namespace

InputEventReader::InputEventReader(std::shared_ptr<sc_api::Session> session)
    : impl_(std::make_unique<Impl>(std::move(session))) {}

InputEventReader::~InputEventReader()                                            = default;

InputEventReader::InputEventReader(InputEventReader&& other) noexcept            = default;
InputEventReader& InputEventReader::operator=(InputEventReader&& other) noexcept = default;

bool InputEventReader::isValid() const { return impl_ && impl_->ring.isValid() && isSessionAlive(impl_->session); }

bool InputEventReader::open() {
    if (isValid()) {
        return true;
    }
    if (!impl_ || !isSessionAlive(impl_->session)) {
        return false;
    }

    const detail::ShmBlock& block = impl_->session->getInternal().input_events;

    impl_->ring                   = inputEventRingOpen(block.getBuffer(), block.getSize());
    if (!impl_->ring.isValid()) {
        return false;
    }

    impl_->cursor = inputEventRingLiveEdge(impl_->ring);
    return true;
}

InputEventReader::ReadResult InputEventReader::read(InputEvent* out, uint32_t max_events) {
    ReadResult result;
    if (!isValid() || out == nullptr || max_events == 0) {
        return result;
    }

    std::uint32_t scanned = 0;
    while (true) {
        // A loss is reachable only while no event of this call has been delivered yet, because the
        // caller must be able to read a baseline that covers every returned event. The codec
        // delivers no event in the call that reports a loss, so the loop below ends there.
        const InputEventReadResult batch = inputEventRingRead(impl_->ring, impl_->cursor, out, max_events);
        result.lost                      = batch.lost;

        for (std::uint32_t i = 0; i < batch.count; ++i) {
            if (isKnownInputEventType(out[i].type)) {
                if (i != result.count) {
                    out[result.count] = out[i];
                }
                ++result.count;
            }
        }

        scanned += batch.count;
        // The capacity bound caps the work of one call when the ring holds only unknown types. The
        // next pass refills `out` from index 0, so the loop must end once a known event is in it.
        if (result.count > 0 || batch.count == 0 || scanned >= impl_->ring.capacity) {
            break;
        }
    }

    return result;
}

}  // namespace sc_api
