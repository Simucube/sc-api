#include "sc-api/core/action.h"

#include "api_internal.h"
#include "sc-api/core/session.h"

namespace sc_api::core {

ActionBuilder::ActionBuilder(ActionBuilder&& builder) noexcept
    : session_(std::move(builder.session_)),
      buffer_(std::move(builder.buffer_)),
      cur_start_idx_(builder.cur_start_idx_) {}

ActionBuilder::ActionBuilder(std::shared_ptr<Session> session) { init(std::move(session)); }

ActionBuilder& ActionBuilder::operator=(ActionBuilder&& builder) noexcept {
    if (this == &builder) return *this;

    session_               = std::move(builder.session_);
    buffer_                = std::move(builder.buffer_);
    cur_start_idx_         = builder.cur_start_idx_;
    builder.cur_start_idx_ = 0;
    return *this;
}

void ActionBuilder::init(std::shared_ptr<Session> session) {
    session_ = std::move(session);
    reset();
}

void ActionBuilder::reset() {
    buffer_.resize(0);
    cur_start_idx_ = 0;
}

bool ActionBuilder::build(SC_API_PROTOCOL_Action_t action_id, const uint8_t* payload, size_t payload_size,
                          uint16_t flags) {
    if (!session_ || session_->getControllerId() == 0u) return false;

    cur_start_idx_ = (uint32_t)buffer_.size();
    buffer_.resize(buffer_.size() + sizeof(SC_API_PROTOCOL_ActionHeader_t) + payload_size);
    SC_API_PROTOCOL_ActionHeader_t* hdr = new (&buffer_[cur_start_idx_]) SC_API_PROTOCOL_ActionHeader_t();
    hdr->action_id                      = action_id;
    hdr->controller_id                  = session_->getControllerId();
    hdr->flags                          = flags;
    hdr->size                           = (uint16_t)(sizeof(SC_API_PROTOCOL_ActionHeader_t) + payload_size);
    std::memcpy(&buffer_[cur_start_idx_ + sizeof(SC_API_PROTOCOL_ActionHeader_t)], payload, payload_size);
    return true;
}

uint8_t* ActionBuilder::startBuilding(SC_API_PROTOCOL_Action_t action_id, std::size_t initial_payload_size,
                                      uint16_t flags) {
    if (!session_ || session_->getControllerId() == 0u) return nullptr;

    cur_start_idx_ = (uint32_t)buffer_.size();
    buffer_.resize(cur_start_idx_ + sizeof(SC_API_PROTOCOL_ActionHeader_t) + initial_payload_size);
    SC_API_PROTOCOL_ActionHeader_t* hdr = new (buffer_.data()) SC_API_PROTOCOL_ActionHeader_t();
    hdr->action_id                      = action_id;
    hdr->controller_id                  = session_->getControllerId();
    hdr->flags                          = flags;
    hdr->size                           = (uint16_t)(sizeof(SC_API_PROTOCOL_ActionHeader_t) + initial_payload_size);

    return buffer_.data() + cur_start_idx_ + sizeof(SC_API_PROTOCOL_ActionHeader_t);
}

uint8_t* ActionBuilder::resizePayload(size_t s) {
    buffer_.resize(cur_start_idx_ + sizeof(SC_API_PROTOCOL_ActionHeader_t) + s);
    return buffer_.data() + cur_start_idx_ + sizeof(SC_API_PROTOCOL_ActionHeader_t);
}

void ActionBuilder::asyncSend(std::atomic<ActionResult>& result_status) {
    if (!session_ || buffer_.empty()) {
        reset();
        result_status.store(ActionResult::failed, std::memory_order_release);
        return;
    }

    finalize();

    auto& h = session_->getInternal();

    result_status.store(ActionResult::inprogress, std::memory_order_release);
    {
        cur_start_idx_      = 0;
        auto            buf = asio::const_buffer(buffer_.data(), buffer_.size());
        std::lock_guard lock(h.high_prio_mutex);
        h.high_priority_socket.async_send_to(
            buf, h.high_priority_socket_target,
            [&result_status, buf_life_time = std::move(buffer_)](asio::error_code ec, std::size_t sent_bytes) {
                if (!ec) {
                    result_status.store(ActionResult::complete, std::memory_order_release);
                } else {
                    result_status.store(ActionResult::failed, std::memory_order_release);
                }
            });
    }
}

ActionResult ActionBuilder::sendNonBlocking() {
    if (!session_ || buffer_.empty()) {
        reset();
        return ActionResult::failed;
    }

    finalize();
    auto& h = session_->getInternal();

    asio::error_code ec;
    {
        std::lock_guard lock(h.high_prio_mutex);
        h.high_priority_socket.send_to(asio::const_buffer(buffer_.data(), buffer_.size()),
                                       h.high_priority_socket_target, 0, ec);
    }
    if (!ec) {
        reset();
        return ActionResult::complete;
    }

    if (ec == asio::error::would_block) {
        return ActionResult::would_block;
    }

    reset();
    return ActionResult::failed;
}

ActionResult ActionBuilder::sendBlocking() {
    if (!session_ || buffer_.empty()) {
        reset();
        return ActionResult::failed;
    }

    finalize();
    auto& h = session_->getInternal();

    asio::error_code ec;
    {
        std::lock_guard lock(h.high_prio_mutex);
        h.high_priority_socket.send_to(asio::const_buffer(buffer_.data(), buffer_.size()),
                                       h.high_priority_socket_target, 0, ec);
    }

    if (!ec) {
        reset();
        return ActionResult::complete;
    }

    if (ec != asio::error::would_block) {
        reset();
        return ActionResult::failed;
    }

    // Synchronize this to non-blocking operation
    ActionResult result = ActionResult::inprogress;
    {
        std::unique_lock lock(h.high_prio_mutex);
        h.high_priority_socket.async_send_to(asio::const_buffer(buffer_.data(), buffer_.size()),
                                             h.high_priority_socket_target,
                                             [&](asio::error_code ec, std::size_t sent_bytes) {
                                                 {
                                                     std::lock_guard lock(h.high_prio_mutex);
                                                     if (!ec) {
                                                         result = ActionResult::complete;
                                                     } else {
                                                         result = ActionResult::failed;
                                                     }
                                                 }
                                                 h.high_prio_sync_cv.notify_all();
                                             });

        h.high_prio_sync_cv.wait(lock, [&]() { return result != ActionResult::inprogress; });
    }

    reset();
    return result;
}

void ActionBuilder::finalize() {
    SC_API_PROTOCOL_ActionHeader_t* hdr =
        reinterpret_cast<SC_API_PROTOCOL_ActionHeader_t*>(buffer_.data() + cur_start_idx_);
    hdr->size = (uint16_t)(buffer_.size() - cur_start_idx_);
}

}  // namespace sc_api::core
