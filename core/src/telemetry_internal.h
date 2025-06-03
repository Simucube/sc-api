#ifndef SC_API_TELEMETRY_INTERNAL_H_
#define SC_API_TELEMETRY_INTERNAL_H_
#include <mutex>

#include "sc-api/core/telemetry.h"

namespace sc_api::core::internal {

class TelemetrySystem {
    friend class TelemetryDefinitionIterator;

public:
    TelemetrySystem();
    ~TelemetrySystem();

    void initialize(const void* shm_buffer, std::size_t shm_buffer_size);

    bool updateDefinitions();

    TelemetryDefinitions getDefinitions(const std::shared_ptr<Session>& session) const;

private:
    using DefStorage = TelemetryDefinitions::DefStorage;

    uint16_t registerUpdateGroup(TelemetryUpdateGroup* g);
    void     unregisterUpdateGroup(TelemetryUpdateGroup* g);

    uint32_t getMaxTelemetryPayload() const;

    mutable std::mutex m_;

    const void*    defs_header_ = nullptr;
    const uint8_t* defs_start_  = nullptr;
    uint32_t       defs_size_   = 0;
    uint32_t       max_defs_    = 0;
    uint16_t       max_telemetry_set_payload_size_;
    uint16_t       group_id_counter_ = 0;

    bool telemetry_control_active_   = false;

    std::shared_ptr<TelemetryDefinitions::DefStorage> cur_defs_;
};

}  // namespace sc_api::core::internal

#endif  // SC_API_TELEMETRY_INTERNAL_H_
