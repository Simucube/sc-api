#include "sc-api/core/telemetry.h"

#include <string.h>

#include <algorithm>
#include <cassert>

#include "api_internal.h"
#include "sc-api/core/protocol/telemetry.h"
#include "sc-api/core/session.h"
#include "telemetry_internal.h"

namespace sc_api::core {

static int baseTypeSizeIndex(Type::BaseType b) {
    switch (b) {
        case Type::boolean:
            return 0;
        case Type::i64:
        case Type::f64:
            return 1;
        case Type::i32:
        case Type::u32:
        case Type::f32:
            return 2;
        case Type::i16:
        case Type::u16:
            return 3;
        case Type::i8:
        case Type::u8:
            return 4;
        case Type::cstring:
        default:
            assert(false);
            return -1;
    }
}

std::shared_ptr<TelemetryDefinitions::DefStorage> TelemetryDefinitions::s_empty_storage =
    std::make_shared<TelemetryDefinitions::DefStorage>();

TelemetryUpdateGroup::TelemetryUpdateGroup(uint16_t group_id) : group_id_(group_id) {}

TelemetryUpdateGroup::~TelemetryUpdateGroup() {}

void TelemetryUpdateGroup::set(std::vector<TelemetryBase*> telemetries) {
    prepared_    = false;
    telemetries_ = std::move(telemetries);
}

void TelemetryUpdateGroup::add(TelemetryBase* telemetry) {
    prepared_ = false;
    telemetries_.push_back(telemetry);
}

void TelemetryUpdateGroup::add(const std::initializer_list<TelemetryBase*>& telemetries) {
    prepared_ = false;
    telemetries_.insert(telemetries_.end(), telemetries.begin(), telemetries.end());
}

bool TelemetryUpdateGroup::configure(std::vector<TelemetryBase*> telemetries, const TelemetryDefinitions& definitions) {
    set(std::move(telemetries));
    return configure(definitions);
}

bool TelemetryUpdateGroup::configure(const TelemetryDefinitions& definitions) {
    if (!definitions.getSession() || telemetries_.empty()) return false;

    action_builder_.init(definitions.getSession());
    prepared_ = false;

    base_value_bufs_.clear();
    for (auto& v : base_value_entries_by_size_) v = 0;

    for (TelemetryBase* t : telemetries_) {
        if (const TelemetryDefinition* def = definitions.find(t->getName(), t->getType())) {
            t->ref_state_.id    = def->id;
            t->ref_state_.flags = def->flags;
        } else {
            t->ref_state_.id    = 0;
            t->ref_state_.flags = 0;
        }
    }

    std::sort(telemetries_.begin(), telemetries_.end(), [](TelemetryBase* a, TelemetryBase* b) {
        int a_idx = baseTypeSizeIndex(a->getType().getBaseType());
        int b_idx = baseTypeSizeIndex(b->getType().getBaseType());
        if (a_idx < b_idx) return true;
        if (a_idx > b_idx) return false;

        return a->ref_state_.id < b->ref_state_.id;
    });

    // Remove duplicates
    telemetries_.erase(std::unique(telemetries_.begin(), telemetries_.end(),
                                   [](TelemetryBase* a, TelemetryBase* b) {
                                       if (a->ref_state_.id == 0) return false;
                                       return a->ref_state_.id == b->ref_state_.id;
                                   }),
                       telemetries_.end());

    uint32_t expected_size         = 0;
    uint32_t register_payload_size = 6;
    for (TelemetryBase* t : telemetries_) {
        // Mapping doesn't match
        if (t->ref_state_.id == 0) continue;

        base_value_bufs_.push_back(t->getSerializedValueBuf());
        int base_type_size_idx = baseTypeSizeIndex(t->getType().getBaseType());
        base_value_entries_by_size_[base_type_size_idx]++;

        if (base_type_size_idx != 0) {
            expected_size += 1u << (base_type_size_idx - 1);
        }

        register_payload_size += 2;
    }
    // Bools are stored as 32bit words and total space used by them is aligned to 8 bytes
    expected_size = ((base_value_entries_by_size_[0] + 63u + 32u) / 64) * 8;
    expected_size += base_value_entries_by_size_[1] * 8;
    expected_size += base_value_entries_by_size_[2] * 4;
    expected_size += base_value_entries_by_size_[3] * 2;
    expected_size += base_value_entries_by_size_[4];

    if (expected_size == 0) return false;

    uint8_t* payload =
        action_builder_.startBuilding(SC_API_PROTOCOL_ACTION_REGISTER_TELEMETRY_GROUP, register_payload_size);
    if (!payload) return false;

    uint32_t payload_size = 6;
    for (TelemetryBase* t : telemetries_) {
        // Mapping doesn't match
        if (t->ref_state_.id == 0) continue;

        payload[payload_size]     = t->ref_state_.id & 0xff;
        payload[payload_size + 1] = t->ref_state_.id >> 8;
        payload_size += 2;
    }

    // group id
    payload[0]        = group_id_ & 0xff;
    payload[1]        = group_id_ >> 8;

    // Number of telemetries in this update group
    payload[2]        = (uint8_t)(base_value_bufs_.size() & 0xff);
    payload[3]        = (uint8_t)(base_value_bufs_.size() >> 8);

    // Size of set telemetry group packet
    payload[4]        = (uint8_t)(expected_size & 0xff);
    payload[5]        = (uint8_t)(expected_size >> 8);
    set_payload_size_ = expected_size + 4;

    prepared_         = action_builder_.sendBlocking() == ActionResult::complete;
    return prepared_;
}

ActionResult TelemetryUpdateGroup::send() {
    if (!prepared_) return ActionResult::failed;

    uint8_t* payload = action_builder_.startBuilding(SC_API_PROTOCOL_ACTION_SET_TELEMETRY_GROUP, set_payload_size_);
    if (!payload) return ActionResult::failed;

    unsigned bool_telemetry_count = base_value_entries_by_size_[0];
    auto     t_buf_it             = base_value_bufs_.begin();
    int      payload_idx          = 4;

    payload[0]                    = group_id_ & 0xff;
    payload[1]                    = group_id_ >> 8;

    // placeholder & alignment
    payload[2]                    = 0;
    payload[3]                    = 0;

    // Full words
    uint32_t word                 = 0;
    unsigned bit_idx              = 0;
    for (bit_idx = 0; bit_idx < bool_telemetry_count; ++bit_idx) {
        const uint8_t* bool_buf = *t_buf_it;
        if (*(const bool*)bool_buf) {
            word |= 1u << (bit_idx % 32);
        }
        t_buf_it++;

        if ((bit_idx + 1) % 32 == 0) {
            std::memcpy(&payload[payload_idx], &word, 4);
            payload_idx += 4;
        }
    }
    if (bit_idx % 32 != 0) {
        std::memcpy(&payload[payload_idx], &word, 4);
        payload_idx += 4;
    }

    // Align up to 8 bytes
    payload_idx                    = (payload_idx + 7) & ~7;

    unsigned telemetry_64bit_count = base_value_entries_by_size_[1];
    for (unsigned i = 0; i < telemetry_64bit_count; ++i) {
        std::memcpy(&payload[payload_idx], *t_buf_it, 8);
        payload_idx += 8;
        t_buf_it++;
    }
    unsigned telemetry_32bit_count = base_value_entries_by_size_[2];
    for (unsigned i = 0; i < telemetry_32bit_count; ++i) {
        std::memcpy(&payload[payload_idx], *t_buf_it, 4);
        payload_idx += 4;
        t_buf_it++;
    }
    unsigned telemetry_16bit_count = base_value_entries_by_size_[3];
    for (unsigned i = 0; i < telemetry_16bit_count; ++i) {
        std::memcpy(&payload[payload_idx], *t_buf_it, 2);
        payload_idx += 2;
        t_buf_it++;
    }
    unsigned telemetry_8bit_count = base_value_entries_by_size_[4];
    for (unsigned i = 0; i < telemetry_8bit_count; ++i) {
        payload[payload_idx++] = **t_buf_it;
        t_buf_it++;
    }

    return action_builder_.sendNonBlocking();
}

ActionResult TelemetryUpdateGroup::disable() {
    static constexpr uint32_t k_empty_group_size = 6;
    uint8_t*                  payload =
        action_builder_.startBuilding(SC_API_PROTOCOL_ACTION_REGISTER_TELEMETRY_GROUP, k_empty_group_size);
    if (!payload) return ActionResult::failed;

    payload[0] = group_id_ & 0xff;
    payload[1] = group_id_ >> 8;

    // Number of telemetries in this update group
    payload[2] = 0;
    payload[3] = 0;

    // Size of set telemetry group packet
    payload[4] = 0;
    payload[5] = 0;
    prepared_  = false;

    return action_builder_.sendBlocking();
}

TelemetryBase::TelemetryBase(std::string&& name, Type type) : name_(std::move(name)), type_(type), ref_state_{0, 0} {}

namespace internal {

TelemetrySystem::TelemetrySystem() {}

TelemetrySystem::~TelemetrySystem() {}

void TelemetrySystem::initialize(const void* shm_buffer, size_t shm_buffer_size) {
    cur_defs_                  = TelemetryDefinitions::s_empty_storage;

    const auto* def_shm        = reinterpret_cast<const SC_API_PROTOCOL_TelemetryDefinitionShm_t*>(shm_buffer);

    uint32_t data_size   = def_shm->definition_data_size;
    uint32_t defs_offset = def_shm->definition_offset;

    if (defs_offset > shm_buffer_size || data_size < sizeof(SC_API_PROTOCOL_TelemetryDefinitionShm_t)) {
        // Offset is more than the shared memory buffer or data size is smaller than expected?
        // Should never happen unless some outsider modifies shared memory so we just abort this
        return;
    }

    // Maximum number of definitions that we can access without going outside shared pages
    max_defs_    = (uint32_t)((shm_buffer_size - defs_offset) / data_size);

    defs_header_ = def_shm;
    defs_size_   = data_size;
    defs_start_  = reinterpret_cast<const uint8_t*>(def_shm) + def_shm->definition_offset;

    updateDefinitions();
}

bool TelemetrySystem::updateDefinitions() {
    std::lock_guard lock(m_);

    const auto* var_def_shm = reinterpret_cast<const SC_API_PROTOCOL_TelemetryDefinitionShm_t*>(defs_header_);
    uint32_t    def_count   = var_def_shm->definition_count;
    std::atomic_thread_fence(std::memory_order_acq_rel);

    // Safety check to avoid accessing outside shared memory
    def_count = std::min(max_defs_, def_count);

    // Nothing to update, all definitions are already copied and they are immutable within session
    if (cur_defs_->defs.size() == def_count) return false;

    std::shared_ptr<DefStorage> def_storage = std::make_shared<DefStorage>();
    // Copy previous defs as they cannot change within a session
    def_storage->defs                       = cur_defs_->defs;

    for (std::size_t i = cur_defs_->defs.size(); i < def_count; ++i) {
        const auto* def_ptr =
            reinterpret_cast<const SC_API_PROTOCOL_TelemetryDef_t*>(defs_start_ + (ptrdiff_t)defs_size_ * i);

        TelemetryDefinition def;
        def.id           = def_ptr->id;
#ifdef _MSC_VER
        def.name = std::string_view(def_ptr->name, strnlen_s(def_ptr->name, sizeof(def_ptr->name) - 1));
#else
        def.name = std::string_view(def_ptr->name, strnlen(def_ptr->name, sizeof(def_ptr->name) - 1));
#endif
        def.type         = Type{def_ptr->type, def_ptr->type_variant_data};
        def.flags        = def_ptr->flags;
        def.variable_idx = def_ptr->alias_variable_idx;
        def_storage->defs.push_back(std::move(def));
    }
    std::atomic_thread_fence(std::memory_order_release);
    cur_defs_ = std::move(def_storage);
    return true;
}

TelemetryDefinitions TelemetrySystem::getDefinitions(const std::shared_ptr<Session>& session) const {
    std::lock_guard lock(m_);
    return TelemetryDefinitions(cur_defs_, session);
}

}  // namespace internal

TelemetryDefinitions::TelemetryDefinitions() : s_(s_empty_storage) {}

const TelemetryDefinition* TelemetryDefinitions::find(std::string_view name) const {
    for (const TelemetryDefinition& def : s_->defs) {
        if (def.name == name) return &def;
    }

    return nullptr;
}

const TelemetryDefinition* TelemetryDefinitions::find(std::string_view name, Type type) const {
    for (const TelemetryDefinition& def : s_->defs) {
        if (def.name == name && def.type == type) return &def;
    }

    return nullptr;
}

const TelemetryDefinition* TelemetryDefinitions::find(uint16_t id) const {
    for (const TelemetryDefinition& def : s_->defs) {
        if (def.id == id) return &def;
    }

    return nullptr;
}

TelemetryDefinitions::TelemetryDefinitions(const std::shared_ptr<DefStorage>& defs,
                                           const std::shared_ptr<Session>&    session)
    : s_(defs), session_(session) {}

// namespace internal

}  // namespace sc_api::core
