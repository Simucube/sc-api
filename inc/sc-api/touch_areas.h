/**
 * @file
 * @brief Screen areas where a touch does not move the PC mouse pointer.
 * @see DashStreamer::streamFrame
 */

#ifndef SC_API_TOUCH_AREAS_H_
#define SC_API_TOUCH_AREAS_H_

#include <cstddef>
#include <cstdint>

namespace sc_api {

/** @brief Largest number of mouse suppressed areas that one streamed frame takes. */
inline constexpr std::size_t k_max_mouse_suppressed_areas = 4;

/**
 * @brief A rectangle of the screen in screen pixels, the same as the frame and the touch events.
 *
 * The origin is the top left corner of the device screen, x to the right and y down, as in
 * @ref TouchEventPayload. The area covers `x` to `x + w - 1` and `y` to `y + h - 1`. An area with
 * `w` or `h` 0 is empty. An area can overlap other areas and the screen edges.
 * `{0, 0, width, height}` with the device_info::ScreenFeedback size covers the full screen.
 */
struct TouchArea {
    std::int16_t  x = 0;
    std::int16_t  y = 0;
    std::uint16_t w = 0;
    std::uint16_t h = 0;

    /** @brief Whether the pixel (px, py) is inside the area, with the same edge rule as the device. */
    constexpr bool contains(int px, int py) const { return px >= x && px < x + w && py >= y && py < y + h; }

    constexpr bool operator==(const TouchArea& b) const { return x == b.x && y == b.y && w == b.w && h == b.h; }
    constexpr bool operator!=(const TouchArea& b) const { return !(*this == b); }
};

}  // namespace sc_api

#endif  // SC_API_TOUCH_AREAS_H_
