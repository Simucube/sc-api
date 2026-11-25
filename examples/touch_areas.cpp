/**
 * Touch area example
 *
 * Streams a page with one button to the first wheel that supports mouse suppressed areas. A tap on the button
 * opens a pop-up, and a tap on the close button of the pop-up closes it. Each frame carries its visible buttons as
 * mouse suppressed areas, so a touch on a button does not move the PC mouse pointer.
 */

#include <sc-api/api.h>
#include <sc-api/dash_stream.h>
#include <sc-api/device_info.h>
#include <sc-api/input_events.h>
#include <sc-api/touch_areas.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

using namespace sc_api;

namespace {

constexpr int      k_submit_fps = 30;
constexpr uint32_t k_batch_size = 64;

std::atomic<bool> g_running{true};

void onSignal(int) { g_running = false; }

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

struct ScreenDevice {
    uint16_t device_session_id = 0;
    uint16_t width             = 0;
    uint16_t height            = 0;
};

/** Finds the first device with a streamable screen that takes mouse suppressed areas. */
std::optional<ScreenDevice> findScreenDevice(const std::shared_ptr<Session>& session) {
    auto dev_info = session->getDeviceInfo();
    if (!dev_info) {
        return std::nullopt;
    }
    for (const device_info::DeviceInfo& device : *dev_info) {
        for (const device_info::ScreenFeedback& screen : device.getScreens()) {
            if (screen.max_mouse_suppressed_areas >= 1) {
                return ScreenDevice{device.getSessionId().id, screen.width, screen.height};
            }
        }
    }
    return std::nullopt;
}

void fillRect(std::vector<uint16_t>& frame, const ScreenDevice& screen, const TouchArea& area, uint16_t color) {
    for (int y = std::max<int>(area.y, 0); y < std::min<int>(area.y + area.h, screen.height); ++y) {
        for (int x = std::max<int>(area.x, 0); x < std::min<int>(area.x + area.w, screen.width); ++x) {
            frame[static_cast<size_t>(y) * screen.width + static_cast<size_t>(x)] = color;
        }
    }
}

TouchArea areaFromScreenFractions(const ScreenDevice& screen, double x, double y, double w, double h) {
    return TouchArea{static_cast<int16_t>(screen.width * x), static_cast<int16_t>(screen.height * y),
                     static_cast<uint16_t>(screen.width * w), static_cast<uint16_t>(screen.height * h)};
}

/** A main page with an open button, and a pop-up with a close button on top of it. */
class Page {
public:
    explicit Page(const ScreenDevice& screen)
        : screen_(screen),
          frame_(static_cast<size_t>(screen.width) * screen.height),
          open_button_{areaFromScreenFractions(screen, 0.1, 0.6, 0.25, 0.25)},
          popup_{areaFromScreenFractions(screen, 0.2, 0.2, 0.6, 0.6)},
          close_button_{areaFromScreenFractions(screen, 0.4, 0.55, 0.2, 0.15)} {}

    std::vector<TouchArea> visibleButtons() const { return {popup_open_ ? close_button_ : open_button_}; }

    void togglePopup() { popup_open_ = !popup_open_; }

    const std::vector<uint16_t>& render() {
        std::fill(frame_.begin(), frame_.end(), rgb565(0, 0, 32));
        if (popup_open_) {
            fillRect(frame_, screen_, popup_, rgb565(80, 80, 80));
            fillRect(frame_, screen_, close_button_, rgb565(200, 0, 0));
        } else {
            fillRect(frame_, screen_, open_button_, rgb565(0, 160, 0));
        }
        return frame_;
    }

private:
    ScreenDevice          screen_;
    std::vector<uint16_t> frame_;
    TouchArea             open_button_;
    TouchArea             popup_;
    TouchArea             close_button_;
    bool                  popup_open_ = false;
};

bool isTapOnVisibleButton(const InputEvent& event, const Page& page) {
    const std::optional<TouchPosition> tap     = getSingleFingerTap(event);
    const std::vector<TouchArea>       buttons = page.visibleButtons();
    return tap && std::any_of(buttons.begin(), buttons.end(),
                              [&](const TouchArea& button) { return button.contains(tap->x, tap->y); });
}

}  // namespace

int main() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    Api api;

    ApiUserInformation user_info;
    user_info.display_name   = "Touch area example";
    user_info.type           = "dash_controller";
    user_info.author         = "Simucube";
    user_info.version_string = "1.0";
    NoAuthControlEnabler control_enabler(&api, Session::control_display, "touch_areas", user_info);

    std::cout << "Waiting for Tuner and a wheel that supports mouse suppressed areas..." << std::endl;
    std::shared_ptr<Session>    session;
    std::optional<ScreenDevice> screen;
    while (g_running && !screen) {
        session = api.getSession();
        if (session && session->getState() == SessionState::connected_control) {
            screen = findScreenDevice(session);
        }
        if (!screen) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
    if (!screen) {
        return 0;
    }

    DashStreamer     streamer(session, screen->device_session_id);
    InputEventReader reader(session);
    if (!streamer.open() || !reader.open()) {
        std::cout << "Cannot open the stream or the input events" << std::endl;
        return 1;
    }

    Page       page(*screen);
    const auto frame_interval = std::chrono::microseconds(1000000 / k_submit_fps);
    InputEvent events[k_batch_size];

    std::cout << "Streaming. Tap the green button on the wheel." << std::endl;
    auto next_frame = std::chrono::steady_clock::now();

    while (g_running && reader.isValid()) {
        const InputEventReader::ReadResult result = reader.read(events, k_batch_size);
        // Taps count only while this streamer owns the device; else another streamer's frame is on the screen
        if (streamer.isOwner()) {
            for (uint32_t i = 0; i < result.count; ++i) {
                if (events[i].device_session_id == screen->device_session_id && isTapOnVisibleButton(events[i], page)) {
                    page.togglePopup();
                    break;  // the rest of the batch was on the old page
                }
            }
        }

        streamer.streamFrame(screen->width, screen->height, page.render().data(), page.visibleButtons());

        next_frame += frame_interval;
        std::this_thread::sleep_until(next_frame);
    }

    streamer.stop();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // lets the Api thread send stop before exit
    std::cout << "Stopped." << std::endl;
    return 0;
}
