/**
 * @file
 * @brief LED control API for controlling RGB lights on connected devices
 *
 */

#ifndef SC_API_LED_CONTROL_H_
#define SC_API_LED_CONTROL_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "device.h"
#include "session_fwd.h"

namespace sc_api {

/**
 * @brief RGB color representation for LEDs
 *
 * Color values range from 0-255 for each component
 */
struct RgbColor {
    uint8_t r            = 0;  ///< Red component (0-255)
    uint8_t g            = 0;  ///< Green component (0-255)
    uint8_t b            = 0;  ///< Blue component (0-255)

    constexpr RgbColor() = default;
    constexpr RgbColor(uint8_t red, uint8_t green, uint8_t blue) : r(red), g(green), b(blue) {}

    constexpr bool operator==(const RgbColor& other) const { return r == other.r && g == other.g && b == other.b; }
    constexpr bool operator!=(const RgbColor& other) const { return !(*this == other); }
};

/**
 * @brief Handle for controlling LEDs on a specific device
 *
 * This class provides an interface for controlling RGB LEDs on SC-link connected devices.
 * LEDs are identified by their index (0-based), which can be obtained from the device's
 * FeedbackType::rgb_light entries in DeviceInfo.
 *
 * Only one LedControl can be created for each device within Session.
 *
 * Example usage:
 * @code
 *     // Step 1: Declare which LEDs you want to control
 *     uint32_t indices[] = {0, 1};
 *     led_control.setControlledLeds(indices, 2);
 *
 *     // Step 2: Set colors for the controlled LEDs
 *     RgbColor colors[] = {{255, 0, 0}, {0, 0, 255}};
 *     led_control.setColors(colors, 2);
 * @endcode
 */
class LedControl {
public:
    /**
     * @brief Construct LED control handle for a device
     *
     * @param session API session
     * @param device Device session specific ID which LEDs should be controlled
     */
    explicit LedControl(const std::shared_ptr<Session>& session, DeviceSessionId device);

    LedControl(const LedControl&) = delete;

    /** Will release control of leds */
    ~LedControl();

    LedControl& operator=(const LedControl&)     = delete;
    LedControl& operator=(LedControl&&) noexcept = delete;

    /**
     * @brief Declare which LEDs this controller will control
     *
     * Sets the control mask for this LED control instance. Once set, this controller
     * claims control of the specified LEDs and will prevent lower-priority controllers
     * from affecting them. Other controllers with higher priority can still override.
     *
     * This should be called before setColors() to establish which LEDs you want to control.
     *
     * LED indices must be valid for the device (as reported in DeviceInfo feedback entries).
     * Invalid indices can cause mismatch when trying to use setColors as it only considers valid indices.
     *
     * @param indices Pointer to array of LED indices to control
     * @param count Number of LEDs (size of array)
     * @return true if command was sent successfully, false if session is invalid or count is 0
     */
    bool setControlledLeds(const uint32_t* indices, unsigned count);

    /**
     * @brief Update colors for the controlled LEDs
     *
     * Sets colors for the LEDs that were declared with setControlledLeds(). The colors
     * array must match the order of indices from setControlledLeds().
     *
     * Example:
     * @code
     * uint32_t indices[] = {5, 10, 15};
     * RgbColor colors[] = {{255, 0, 0}, {0, 255, 0}, {0, 0, 255}};
     * led_control.setControlledLeds(indices, 3);  // Declare control of LEDs 5, 10, 15
     * led_control.setColors(colors, 3);            // Set colors: 5=red, 10=green, 15=blue
     * @endcode
     *
     * @param colors Pointer to array of RGB colors (must match controlled LEDs count and order)
     * @param count Number of colors in the array
     * @return true if command was sent successfully, false if session is invalid or count is 0
     */
    bool setColors(const RgbColor* colors, unsigned count);

    /**
     * @brief Convenience method to set controlled LEDs and their colors in one call
     *
     * Atomically sets which LEDs to control and their colors. This is equivalent to calling
     * setControlledLeds() followed by setColors(), but more efficient.
     *
     * @param indices Pointer to array of LED indices to control
     * @param colors Pointer to array of RGB colors (must be same size as indices)
     * @param count Number of LEDs to set (size of both arrays)
     * @return true if command was sent successfully, false if session is invalid or count is 0
     */
    bool setControlledLedsAndColors(const uint32_t* indices, const RgbColor* colors, unsigned count);

    /**
     * @brief Clear (turn off) all controlled LEDs
     *
     * Sets all LEDs that are controlled by this instance to black (R=0, G=0, B=0),
     * effectively turning them off. The control mask remains unchanged - this controller
     * still owns these LEDs.
     *
     * @return true if command was sent successfully, false if session is invalid
     */
    bool clearLeds();

    /**
     * @brief Release control of all LEDs
     *
     * Clears the control mask, releasing control of all LEDs. This allows lower-priority
     * controllers to take over control of these LEDs. After calling this, you must call
     * setControlledLeds() or setControlledLedsAndColors() again to control any LEDs.
     *
     * This is different from clearLeds() which turns LEDs off but maintains control.
     *
     * @return true if command was sent successfully, false if session is invalid
     */
    bool releaseControl();

    /**
     * @brief Get the device session ID this control is assigned to
     *
     * @return DeviceSessionId of the device
     */
    DeviceSessionId getDevice() const { return device_; }

    /**
     * @brief Get the session this control uses
     *
     * @return Shared pointer to the Session
     */
    std::shared_ptr<Session> getSession() const { return session_; }

private:
    std::shared_ptr<Session> session_;
    DeviceSessionId          device_ = k_invalid_device_session_id;
    std::vector<uint32_t>    controlled_indices_;  ///< Currently controlled LED indices (in user's order)

    /** If indices are provided in ascending order then we never need to remap */
    bool indices_ascending_ = false;
};

}  // namespace sc_api

#endif  // SC_API_LED_CONTROL_H_
