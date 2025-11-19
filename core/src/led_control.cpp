/**
 * @file
 * @brief LED control API implementation
 *
 */

#include "sc-api/core/led_control.h"

#include <algorithm>

#include "sc-api/core/command.h"
#include "sc-api/core/session.h"

namespace sc_api::core {

LedControl::LedControl(const std::shared_ptr<Session>& session, DeviceSessionId device)
    : session_(session), device_(device) {}

LedControl::~LedControl() { releaseControl(); }

bool LedControl::setControlledLeds(const uint32_t* indices, unsigned count) {
    if (!session_ || !indices || count == 0) {
        return false;
    }

    // Store indices for later remapping in setColors()
    controlled_indices_.assign(indices, indices + count);
    indices_ascending_ = std::is_sorted(indices, indices + count);

    CommandRequest req("led", "set_control_mask");

    // Add device session ID
    req.docAddElement("device_session_id", static_cast<int32_t>(device_.id));

    // Begin LED indices array
    req.docBeginSubArray("indices");

    // Add each LED index
    for (unsigned i = 0; i < count; ++i) {
        req.arrayAddElement(static_cast<int32_t>(indices[i]));
    }

    req.endArray();

    return session_->asyncCommand(std::move(req), [](const AsyncCommandResult& result) {
    });
}

bool LedControl::setColors(const RgbColor* colors, unsigned count) {
    if (!session_ || !colors || count == 0) {
        return false;
    }

    // Validate that count is not more than number of leds we control
    if (count > controlled_indices_.size()) {
        return false;
    }

    if (indices_ascending_) {
    }

    CommandRequest req("led", "update_colors");
    req.docAddElement("device_session_id", static_cast<int32_t>(device_.id));

    // Begin colors array - flat array of R,G,B values
    req.docBeginSubArray("colors");
    if (indices_ascending_) {
        // Fast path. Indices are already in correct order so no need to sort
        for (unsigned i = 0; i < count; ++i) {
            req.arrayAddElement(static_cast<int32_t>(colors[i].r));
            req.arrayAddElement(static_cast<int32_t>(colors[i].g));
            req.arrayAddElement(static_cast<int32_t>(colors[i].b));
        }
    } else {
        // Backend expects colors in ascending index order, but user provides them
        // in the order matching controlled_indices_. We need to remap.
        // Create pairs of (index, color) and sort by index
        std::vector<std::pair<uint32_t, RgbColor>> index_color_pairs;
        index_color_pairs.reserve(count);
        for (unsigned i = 0; i < count; ++i) {
            index_color_pairs.emplace_back(controlled_indices_[i], colors[i]);
        }

        // Sort by LED index (ascending order)
        std::sort(index_color_pairs.begin(), index_color_pairs.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });

        // Add RGB values in ascending index order: [r0, g0, b0, r1, g1, b1, ...]
        for (const auto& [index, color] : index_color_pairs) {
            req.arrayAddElement(static_cast<int32_t>(color.r));
            req.arrayAddElement(static_cast<int32_t>(color.g));
            req.arrayAddElement(static_cast<int32_t>(color.b));
        }
    }

    req.endArray();

    // Send command asynchronously
    return session_->asyncCommand(std::move(req), [](const AsyncCommandResult& result) {
    });
}

bool LedControl::setControlledLedsAndColors(const uint32_t* indices, const RgbColor* colors, unsigned count) {
    if (!session_ || !indices || !colors || count == 0) {
        return false;
    }

    // Store indices for later remapping in setColors()
    controlled_indices_.assign(indices, indices + count);
    indices_ascending_ = std::is_sorted(indices, indices + count);

    CommandRequest req("led", "set_colors");
    req.docAddElement("device_session_id", static_cast<int32_t>(device_.id));

    // Add indices as flat array
    req.docBeginSubArray("indices");
    for (unsigned i = 0; i < count; ++i) {
        req.arrayAddElement(static_cast<int32_t>(indices[i]));
    }
    req.endArray();

    // Add colors as flat array: [r0, g0, b0, r1, g1, b1, ...]
    req.docBeginSubArray("colors");
    for (unsigned i = 0; i < count; ++i) {
        req.arrayAddElement(static_cast<int32_t>(colors[i].r));
        req.arrayAddElement(static_cast<int32_t>(colors[i].g));
        req.arrayAddElement(static_cast<int32_t>(colors[i].b));
    }
    req.endArray();

    return session_->asyncCommand(std::move(req), [](const AsyncCommandResult& result) {
    });
}

bool LedControl::clearLeds() {
    if (!session_) {
        return false;
    }

    CommandRequest req("led", "clear_controlled");
    req.docAddElement("device_session_id", static_cast<int32_t>(device_.id));

    return session_->asyncCommand(std::move(req), [](const AsyncCommandResult& result) {
    });
}

bool LedControl::releaseControl() {
    if (!session_) {
        return false;
    }

    if (controlled_indices_.empty()) {
        // We are not controlling anything anyway
        return true;
    }

    // Clear stored indices
    controlled_indices_.clear();
    indices_ascending_ = false;

    CommandRequest req("led", "set_control_mask");
    req.docAddElement("device_session_id", static_cast<int32_t>(device_.id));

    // Send empty indices array to release all control
    req.docBeginSubArray("indices");
    req.endArray();

    return session_->asyncCommand(std::move(req), [](const AsyncCommandResult& result) {
    });
}

}  // namespace sc_api::core
