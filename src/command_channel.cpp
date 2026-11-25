#include "command_channel.h"

#include <algorithm>
#include <cstring>
#include <string>

#include "sc-api/util/bson_reader.h"

namespace sc_api::detail {

ResultCode PendingCommands::add(int32_t cmd_id, CommandCallback cb) {
    std::lock_guard lock(m_);
    if (closed_) return ResultCode::error_invalid_session_state;
    if (callbacks_.size() >= max_pending_) return ResultCode::error_busy;
    return callbacks_.emplace(cmd_id, std::move(cb)).second ? ResultCode::ok : ResultCode::error_busy;
}

CommandCallback PendingCommands::take(int32_t cmd_id) {
    std::lock_guard lock(m_);
    auto            it = callbacks_.find(cmd_id);
    if (it == callbacks_.end()) return {};

    CommandCallback cb = std::move(it->second);
    callbacks_.erase(it);
    return cb;
}

void PendingCommands::remove(int32_t cmd_id) {
    std::lock_guard lock(m_);
    callbacks_.erase(cmd_id);
}

void PendingCommands::close(ResultCode code, std::string_view message) {
    std::unordered_map<int32_t, CommandCallback> settled;
    {
        std::lock_guard lock(m_);
        closed_ = true;
        settled.swap(callbacks_);
    }

    // AsyncCommandResult reads the message as a null-terminated string
    const std::string message_str(message);
    for (auto& [cmd_id, cb] : settled) {
        cb(AsyncCommandResult::createFailure(code, message_str));
    }
}

std::size_t PendingCommands::size() const {
    std::lock_guard lock(m_);
    return callbacks_.size();
}

FramingResult consumeDocuments(std::vector<uint8_t>& buffer, uint32_t& used, uint32_t max_document_size,
                               const std::function<void(const uint8_t* data, int32_t size)>& on_document) {
    constexpr int32_t k_min_valid_bson_size = 5;

    uint32_t offset                         = 0;
    int32_t  partial_doc_size               = 0;
    while (used - offset >= sizeof(int32_t)) {
        const int32_t doc_size = util::BsonReader::getTotalDocumentSize(&buffer[offset]);
        if (doc_size < k_min_valid_bson_size || static_cast<uint32_t>(doc_size) > max_document_size) {
            return FramingResult::invalid;
        }

        if (static_cast<uint32_t>(doc_size) > used - offset) {
            partial_doc_size = doc_size;
            break;
        }

        on_document(&buffer[offset], doc_size);
        offset += static_cast<uint32_t>(doc_size);
    }

    if (offset > 0) {
        std::memmove(buffer.data(), buffer.data() + offset, used - offset);
        used -= offset;
    }

    const std::size_t needed = std::max<std::size_t>(used + 1, static_cast<std::size_t>(partial_doc_size));
    if (buffer.size() < needed) {
        buffer.resize(needed);
    }
    return FramingResult::ok;
}

}  // namespace sc_api::detail
