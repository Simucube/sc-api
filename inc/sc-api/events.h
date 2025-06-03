/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_INTERNAL_EVENTS_H_
#define SC_API_INTERNAL_EVENTS_H_
#include <optional>

#include "sc-api/core/events.h"

namespace sc_api {

using core::Event;

namespace event {

using core::session_event::DeviceInfoChanged;
using core::session_event::SessionStateChanged;
using core::session_event::SimDataChanged;
using core::session_event::TelemetryDefinitionsChanged;
using core::session_event::VariableDefinitionsChanged;

/** Return pointer to SessionStateChanged event if given event contains that */
const event::SessionStateChanged* getIfSessionStateChanged(const Event* e);

/** Return pointer to VariableDefinitionsChanged event if given event contains that */
const event::VariableDefinitionsChanged* getIfVariableDefinitionsChanged(const Event* e);

/** Return pointer to DeviceInfoChanged event if given event contains that */
const event::DeviceInfoChanged* getIfDeviceInfoChanged(const Event* e);

/** Return pointer to TelemetryDefinitionsChanged event if given event contains that */
const event::TelemetryDefinitionsChanged* getIfTelemetryDefinitionsChanged(const Event* e);

/** Return pointer to SimDataChanged event if given event contains that */
const event::SimDataChanged* getIfSimDataChanged(const Event* e);

/** Return true, if given event is SessionStateChanged */
bool hasSessionStateChanged(const Event& e);

/** Return true, if given event is DeviceInfoChanged */
bool hasDeviceInfoChanged(const Event& e);

/** Return true, if given event is TelemetryDefinitionsChanged */
bool hasTelemetryDefinitionsChanged(const Event& e);

/** Return true, if given event is VariableDefinitionsChanged */
bool hasVariableDefinitionsChanged(const Event& e);

/** Return true, if given event is SimDataChanged */
bool hasSimDataChanged(const Event& e);

/** Return pointer to SessionStateChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const event::SessionStateChanged* getIfSessionStateChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfSessionStateChanged(&e->value());
}

/** Return pointer to VariableDefinitionsChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const event::VariableDefinitionsChanged* getIfVariableDefinitionsChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfVariableDefinitionsChanged(&e->value());
}

/** Return pointer to DeviceInfoChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const event::DeviceInfoChanged* getIfDeviceInfoChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfDeviceInfoChanged(&e->value());
}

/** Return pointer to TelemetryDefinitionsChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const event::TelemetryDefinitionsChanged* getIfTelemetryDefinitionsChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfTelemetryDefinitionsChanged(&e->value());
}

/** Return pointer to SimDataChanged event if given optional event contains that, otherwise returns nullptr
 */
inline const event::SimDataChanged* getIfSimDataChanged(const std::optional<Event>* e) {
    if (!e->has_value()) return nullptr;
    return getIfSimDataChanged(&e->value());
}

}  // namespace event

}  // namespace sc_api

#endif  // SC_API_INTERNAL_EVENTS_H_
