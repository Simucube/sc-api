#include "sc-api/events.h"

namespace sc_api::event {
const SessionStateChanged* getIfSessionStateChanged(const Event* e) { return std::get_if<SessionStateChanged>(e); }

const VariableDefinitionsChanged* getIfVariableDefinitionsChanged(const Event* e) {
    return std::get_if<VariableDefinitionsChanged>(e);
}

const TelemetryDefinitionsChanged* getIfTelemetryDefinitionsChanged(const Event* e) {
    return std::get_if<TelemetryDefinitionsChanged>(e);
}

const DeviceInfoChanged* getIfDeviceInfoChanged(const Event* e) { return std::get_if<DeviceInfoChanged>(e); }

const SimDataChanged* getIfSimDataChanged(const Event* e) { return std::get_if<SimDataChanged>(e); }

bool hasSessionStateChanged(const Event& e) { return std::holds_alternative<SessionStateChanged>(e); }

bool hasTelemetryDefinitionsChanged(const Event& e) { return std::holds_alternative<TelemetryDefinitionsChanged>(e); }

bool hasVariableDefinitionsChanged(const Event& e) { return std::holds_alternative<VariableDefinitionsChanged>(e); }

bool hasDeviceInfoChanged(const Event& e) { return std::holds_alternative<DeviceInfoChanged>(e); }

bool hasSimDataChanged(const Event& e) { return std::holds_alternative<SimDataChanged>(e); }

}  // namespace sc_api::event
