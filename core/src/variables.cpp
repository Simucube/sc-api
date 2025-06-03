#include "sc-api/core/variables.h"

#include <string.h>

#include <algorithm>
#include <cassert>
#include <cstring>

#include "device_info_internal.h"
#include "sc-api/core/protocol/variables.h"
#include "sc-api/core/session.h"
#include "variables_internal.h"

namespace sc_api::core {

uint32_t volatileRead(const uint32_t& v) { return const_cast<const volatile uint32_t&>(v); }

namespace internal {
void VariableProvider::initialize(Session* session, const void* def_shm_buffer, size_t def_shm_buffer_size,
                                  const void* value_shm_buffer, size_t value_shm_buffer_size) {
    def_chunk_               = std::make_shared<VariableDefChunk>();
    session_                 = session;

    const auto* var_def_shm  = reinterpret_cast<const SC_API_PROTOCOL_VariableDefinitionsShm_t*>(def_shm_buffer);
    const auto* var_data_shm = reinterpret_cast<const SC_API_PROTOCOL_VariableDataShm_t*>(value_shm_buffer);

    variable_def_size        = volatileRead(var_def_shm->var_definition_data_size);
    uint32_t var_def_offset  = volatileRead(var_def_shm->var_definition_offset);
    uint32_t var_data_offset = volatileRead(var_data_shm->var_data_offset);

    if (var_def_offset >= def_shm_buffer_size || var_data_offset >= value_shm_buffer_size) {
        // Sanity check to avoid buffer overflows or exploits
        def_chunk_ = nullptr;
        return;
    }

    variable_def_header                       = var_def_shm;
    variable_defs_start                       = reinterpret_cast<const uint8_t*>(var_def_shm) + var_def_offset;
    variable_values_start                     = reinterpret_cast<const uint8_t*>(var_data_shm) + var_data_offset;

    // Calculate bounds for valid data to make sure we don't access memory outside the shared region
    max_variable_def_count                    = (uint32_t)((def_shm_buffer_size - var_def_offset) / variable_def_size);
    def_chunk_->variable_values_max_data_size = (uint32_t)(def_shm_buffer_size - var_data_offset);

    refreshDefinitions();
}

bool VariableProvider::updateDefinitions() {
    {
        std::unique_lock lock(m_);
        return refreshDefinitions();
    }
}

VariableDefinitions VariableProvider::definitions() const {
    std::shared_lock lock(m_);
    return VariableDefinitions(def_chunk_, session_->shared_from_this());
}

bool VariableProvider::haveDefinitionsChanged(const VariableDefinitions& defs) {
    std::shared_lock lock(m_);
    if (defs.def_chunk_ != def_chunk_) return true;

    uint32_t cur_def_count = 0;
    if (def_chunk_) {
        cur_def_count = def_chunk_->def_count;
    }

    return defs.count_ != cur_def_count;
}

bool VariableProvider::refreshDefinitions() {
    uint32_t    var_def_count = 0;
    const auto* definition_header =
        reinterpret_cast<const SC_API_PROTOCOL_VariableDefinitionsShm_t*>(variable_def_header);
    var_def_count                 = (std::min)(definition_header->var_definition_count, max_variable_def_count);
    ptrdiff_t      def_size       = variable_def_size;
    const uint8_t* var_defs_start = variable_defs_start;
    const uint8_t* values_start   = variable_values_start;

    auto copy_definition          = [&](const SC_API_PROTOCOL_VariableDefinition_t& def) -> VariableDefCopy* {
        unsigned chunk         = def_chunk_->def_count / VariableDefChunk::k_definitions_in_chunk;
        unsigned chunk_var_idx = def_chunk_->def_count % VariableDefChunk::k_definitions_in_chunk;
        if (!def_chunk_->defs[chunk]) {
            def_chunk_->defs[chunk] = std::make_unique<VariableDefCopy[]>(VariableDefChunk::k_definitions_in_chunk);
        }

        VariableDefCopy& copy     = def_chunk_->defs[chunk][chunk_var_idx];
        copy.device_session_id.id = def.device_session_id;
        copy.flags                = def.flags;
        std::memcpy(copy.name, def.name, sizeof(copy.name));
        copy.name[sizeof(copy.name) - 1] = '\0';  // Make sure that name is null-terminated
        copy.type                        = Type(def.type, def.type_variant_data);

        if ((int64_t)def.value_offset + copy.type.getValueByteSize() > def_chunk_->variable_values_max_data_size) {
            // Value out of bounds ignore definition
            return nullptr;
        }

        copy.value_ptr = values_start + def.value_offset;
        ++def_chunk_->def_count;
        return &copy;
    };

    bool new_defs = def_chunk_->processed_def_count < var_def_count;
    for (uint32_t i = def_chunk_->processed_def_count; i < var_def_count; ++i) {
        const auto* def_ptr =
            reinterpret_cast<const SC_API_PROTOCOL_VariableDefinition_t*>(var_defs_start + (def_size * i));
        ++def_chunk_->processed_def_count;
        if (VariableDefCopy* var = copy_definition(*def_ptr)) {
            auto insert_point = std::lower_bound(def_chunk_->search_map.begin(), def_chunk_->search_map.end(), var,
                                                 &VariableDefChunk::searchMapSortCmp);
            def_chunk_->search_map.insert(insert_point, var);
        }
    }

    return new_defs;
}

}  // namespace internal

VariableDefinitions::VariableDefinitions() : count_(0) {}

VariableDefinition VariableDefinitions::operator[](uint32_t idx) const {
    assert(def_chunk_);
    assert(idx < count_);

    const auto& def_copy = def_chunk_->getDefByIdx((int)idx);

    VariableDefinition def;
    def.name              = def_copy.getName();
    def.value_ptr         = def_copy.value_ptr;
    def.type              = def_copy.type;
    def.device_session_id = def_copy.device_session_id;
    def.flags             = def_copy.flags;
    return def;
}

VariableDefinition VariableDefinitions::find(std::string_view name, DeviceSessionId device_session_id) const {
    if (!def_chunk_) {
        return {};
    }

    int def_count = (int)def_chunk_->def_count;
    for (int i = 0; i < def_count; ++i) {
        const auto& def_copy = def_chunk_->getDefByIdx(i);
        if (def_copy.device_session_id == device_session_id && std::string_view(def_copy.name) == name) {
            VariableDefinition def;
            def.name              = def_copy.getName();
            def.value_ptr         = def_copy.value_ptr;
            def.type              = def_copy.type;
            def.device_session_id = def_copy.device_session_id;
            def.flags             = def_copy.flags;
            return def;
        }
    }

    return {};
}

VariableDefinition VariableDefinitions::find(std::string_view name, Type type,
                                             DeviceSessionId device_session_id) const {
    if (!def_chunk_) {
        return {};
    }

    int def_count = (int)def_chunk_->def_count;
    for (int i = 0; i < def_count; ++i) {
        const auto& def_copy = def_chunk_->getDefByIdx(i);
        if (def_copy.device_session_id == device_session_id && def_copy.type == type &&
            std::string_view(def_copy.name) == name) {
            VariableDefinition def;
            def.name              = def_copy.getName();
            def.value_ptr         = def_copy.value_ptr;
            def.type              = def_copy.type;
            def.device_session_id = def_copy.device_session_id;
            def.flags             = def_copy.flags;
            return def;
        }
    }

    return {};
}

const void* VariableDefinitions::findValuePointer(Type type, const std::string_view& name,
                                                  DeviceSessionId device_session_id) const {
    if (!def_chunk_) {
        return nullptr;
    }

    int def_count = (int)def_chunk_->def_count;
    for (int i = 0; i < def_count; ++i) {
        const auto& def_copy = def_chunk_->getDefByIdx(i);
        if (def_copy.device_session_id == device_session_id && std::string_view(def_copy.name) == name &&
            def_copy.type == type) {
            return def_copy.value_ptr;
        }
    }

    return {};
}

VariableDefinitions::VariableDefinitions(std::shared_ptr<VariableDefChunk> chunk, std::shared_ptr<Session> session)
    : def_chunk_(std::move(chunk)), session_(std::move(session)), count_(def_chunk_->def_count) {}

VariableDefinition VariableDefinitions::iterator::operator*() const {
    assert(defs_);
    return (*defs_)[idx_];
}

const VariableDefinitions::VariableDefChunk::VariableDefCopy& VariableDefinitions::VariableDefChunk::getDefByIdx(
    int idx) const {
    assert(defs[idx / VariableDefChunk::k_definitions_in_chunk]);
    return defs[idx / VariableDefChunk::k_definitions_in_chunk][idx % VariableDefChunk::k_definitions_in_chunk];
}

bool VariableDefinitions::VariableDefChunk::searchMapSortCmp(const VariableDefCopy* a, const VariableDefCopy* b) {
    if (a->device_session_id.id < b->device_session_id.id) return true;
    if (a->device_session_id.id > b->device_session_id.id) return false;
    return a->getName() < b->getName();
}

bool VariableDefinitions::VariableDefChunk::searchMapByKeyCmp(const VariableDefCopy* var, const SearchKey& key) {
    if (var->device_session_id.id < key.device_session_id.id) return true;
    if (var->device_session_id.id > key.device_session_id.id) return false;
    return var->getName() < key.name;
}

}  // namespace sc_api::core
