/**
 * Dash streaming example
 *
 * Streams a generated animation to the first connected device that has a streamable screen.
 * Shows the full lifecycle: find the screen device, register as a controller, open the
 * stream, pace frames, poll feedback and ownership, recover from session loss, and stop.
 */

#include <sc-api/api.h>
#include <sc-api/dash_stream.h>
#include <sc-api/device_info.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

using namespace sc_api;

namespace {

// The submit rate caps the device frame rate, so submit fast: a newer frame replaces one still waiting to be sent.
constexpr int k_submit_fps      = 60;

constexpr uint16_t k_target_fps = 0;  // 0 = no target; higher trades quality for frame rate

constexpr int k_ball_count      = 4;

std::atomic<bool> g_running{true};

void onSignal(int) { g_running = false; }

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

// One flat RGB565 color per ball, tail first.
constexpr uint16_t k_palette[k_ball_count] = {
    rgb565(128, 0, 255),
    rgb565(0, 128, 255),
    rgb565(0, 255, 128),
    rgb565(255, 64, 0),
};

constexpr int k_ball_radius[k_ball_count]      = {12, 24, 36, 48};

// Time offset behind the head for each ball. The gap between two balls scales with their
// sizes, so the small tail balls travel close together.
constexpr double k_ball_offset_s[k_ball_count] = {0.29, 0.23, 0.13, 0.0};

/** Filled circle in one flat color, clipped to the frame. */
void fillCircle(std::vector<uint16_t>& buf, int w, int h, int cx, int cy, int radius, uint16_t color) {
    for (int dy = -radius; dy <= radius; ++dy) {
        int y = cy + dy;
        if (y < 0 || y >= h) continue;
        int half = static_cast<int>(std::sqrt(static_cast<float>(radius * radius - dy * dy)));
        int x0   = std::max(cx - half, 0);
        int x1   = std::min(cx + half, w - 1);
        for (int x = x0; x <= x1; ++x) {
            buf[static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)] = color;
        }
    }
}

/** A chain of balls that traces a Lissajous curve. Each ball has its own color, and the
 * balls get smaller toward the tail.
 *
 * The function clears and redraws the full frame on each tick. This is almost free on the
 * wire: the backend compresses each frame against the previous one, so pixels that do not
 * change cost nothing. The payload stays at the balls themselves, a small share of the
 * pixels.
 *
 * Each ball has one flat color on purpose. Runs of one color compress into a few bytes.
 * Anti-aliased or gradient edges make the payload roughly ten times larger.
 */
void renderFrame(std::vector<uint16_t>& buf, int w, int h, double t) {
    std::fill(buf.begin(), buf.end(), rgb565(0, 0, 0));

    // Draw the tail first, so that the head stays on top where the curve crosses itself.
    for (int i = 0; i < k_ball_count; ++i) {
        double ts = t - k_ball_offset_s[i];
        int    cx = static_cast<int>(w * (0.5 + 0.37 * std::sin(ts * 1.9)));
        int    cy = static_cast<int>(h * (0.5 + 0.37 * std::sin(ts * 2.5)));
        fillCircle(buf, w, h, cx, cy, k_ball_radius[i], k_palette[i]);
    }
}

/** Device frame counter at one feedback poll. */
struct DeviceSample {
    uint32_t                              frames = 0;
    std::chrono::steady_clock::time_point time;
};

/** Device frame rate between two polls, or nullopt without an earlier poll or when the counter
 * dropped back (see StreamFeedback::device_frame_counter). */
std::optional<int> deviceFps(const std::optional<DeviceSample>& earlier, const DeviceSample& later) {
    if (!earlier || later.frames < earlier->frames) return std::nullopt;
    const double elapsed_s = std::chrono::duration<double>(later.time - earlier->time).count();
    return static_cast<int>((later.frames - earlier->frames) / elapsed_s);
}

struct ScreenDevice {
    uint16_t session_id = 0;
    uint16_t width      = 0;
    uint16_t height     = 0;
};

/** Finds the first device that advertises a streamable screen, and reads its screen geometry. */
std::optional<ScreenDevice> findScreenDevice(const std::shared_ptr<Session>& session) {
    auto dev_info = session->getDeviceInfo();
    if (!dev_info) {
        return std::nullopt;
    }
    auto device = dev_info->findFirstByFilter(
        [](const device_info::DeviceInfo& dev) { return dev.hasFeedbackType(device_info::FeedbackType::screen); });
    if (!device) {
        return std::nullopt;
    }

    auto screen = device->getTypedFeedback<device_info::ScreenFeedback>();
    // Sanity bound before the geometry sizes the frame buffer: a broken device descriptor must
    // not cause a huge allocation. 2048 per axis mirrors the backend's own frame limit. The
    // backend also validates every frame it pulls, so oversize geometry cannot reach the device.
    if (!screen || screen.width > 2048 || screen.height > 2048) {
        return std::nullopt;
    }
    ScreenDevice result;
    result.session_id = device->getSessionId().id;
    result.width      = screen.width;
    result.height     = screen.height;
    return result;
}

}  // namespace

int main() {
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    Api api;

    ApiUserInformation user_info;
    user_info.display_name   = "Dash stream example";
    user_info.type           = "dash_controller";
    user_info.path           = "";
    user_info.author         = "Simucube";
    user_info.version_string = "1.0";

    // Streaming requires a registered controller. Registration runs in the background on the
    // Api thread. When it starts here, it is usually complete before the first open() call.
    NoAuthControlEnabler control_enabler(&api, Session::control_display, "dash_stream", user_info);

    std::unique_ptr<DashStreamer> streamer;
    std::shared_ptr<Session>      streamer_session;  // session the streamer was built on
    ScreenDevice                  screen;
    std::vector<uint16_t>         frame;

    uint32_t                    delivered = 0;  // frames that the backend accepted (not proof of display)
    uint32_t                    dropped   = 0;  // the backend had not consumed the previous frame yet: normal
    uint32_t                    failed    = 0;  // transport or state errors
    std::optional<DeviceSample> last_device_sample;
    std::string                 last_status;

    // Frame pacing uses steady_clock. sc_api::Clock is only for backend timestamps such as
    // StreamFeedback::last_ack_time. It is not a usable local clock on all platforms.
    const auto frame_interval = std::chrono::microseconds(1000000 / k_submit_fps);
    const auto start_time     = std::chrono::steady_clock::now();
    auto       next_frame     = start_time;
    auto       next_poll      = start_time;

    while (g_running) {
        auto now = std::chrono::steady_clock::now();

        // The 1 Hz path finds the session and the device, retries open(), and prints status.
        // open() is a blocking command that can take 1 s, so it must stay off the frame path.
        if (now >= next_poll) {
            next_poll += std::chrono::seconds(1);
            if (next_poll < now) {
                next_poll = now + std::chrono::seconds(1);  // no retry burst after a stall
            }

            auto printStatus = [&last_status](const std::string& status) {
                if (status != last_status) {
                    std::cout << status << std::endl;
                    last_status = status;
                }
            };

            auto session = api.getSession();
            if (!session || session->getState() == SessionState::session_lost) {
                // The streamer holds the lost session. A new streamer is built on the next session.
                streamer.reset();
                streamer_session.reset();
                printStatus("Waiting for an API session (start Tuner, it hosts the backend)...");
            } else if (session->getState() != SessionState::connected_control) {
                printStatus("Waiting for control registration...");
            } else {
                auto target = findScreenDevice(session);
                if (!target) {
                    if (streamer) {
                        // The device is gone. Stop and release the stream, so the frame path
                        // stops pushing frames to a device that cannot show them.
                        streamer->stop();
                        streamer.reset();
                        streamer_session.reset();
                    }
                    printStatus("Waiting for a device with a streamable screen...");
                } else {
                    if (!streamer || streamer_session != session ||
                        streamer->getDeviceSessionId() != target->session_id) {
                        if (streamer) {
                            streamer->stop();  // tell the old device to exit streaming now
                        }
                        screen   = *target;
                        streamer = std::make_unique<DashStreamer>(session, screen.session_id);
                        streamer->setTargetFps(k_target_fps);
                        streamer_session = session;
                        last_device_sample.reset();
                        frame.assign(static_cast<size_t>(screen.width) * screen.height, 0);
                    }
                    if (!streamer->isValid()) {
                        streamer->open();
                    }
                    if (!streamer->isValid()) {
                        printStatus("Stream buffer not open yet, retrying...");
                    } else {
                        printStatus("Streaming to device " + std::to_string(screen.session_id) + " (" +
                                    std::to_string(screen.width) + "x" + std::to_string(screen.height) + ")");
                        StreamFeedback feedback = streamer->getStreamFeedback();
                        // device_frame_counter and dropped_count are global to the device. A
                        // non-owner sees the owner's progress, so the device frame rate has
                        // meaning only for the owner.
                        std::cout << "owner=" << (feedback.is_owner ? "yes" : "no") << " delivered=" << delivered
                                  << " dropped=" << dropped << " failed=" << failed;
                        if (feedback.is_owner) {
                            const DeviceSample sample{feedback.device_frame_counter, now};
                            if (const std::optional<int> fps = deviceFps(last_device_sample, sample)) {
                                std::cout << " target_fps=" << k_target_fps << " device_fps=" << *fps
                                          << " device_drops=" << feedback.dropped_count;
                            }
                            // The counter reads 0 until the device reports its first count.
                            if (sample.frames != 0) {
                                last_device_sample = sample;
                            } else {
                                last_device_sample.reset();
                            }
                        } else {
                            last_device_sample.reset();
                        }
                        std::cout << std::endl;
                    }
                }
            }
        }

        // The frame path requires isValid(), not only a streamer object. Without this check,
        // streamFrame retries the blocking open() internally and stalls the loop while
        // disconnected.
        if (streamer && streamer->isValid()) {
            double t = std::chrono::duration<double>(now - start_time).count();
            renderFrame(frame, screen.width, screen.height, t);
            switch (streamer->streamFrame(screen.width, screen.height, frame.data())) {
                case FrameResult::delivered:
                    ++delivered;
                    break;
                case FrameResult::dropped:
                    ++dropped;
                    break;
                case FrameResult::failed:
                    ++failed;
                    break;
            }
        }

        next_frame += frame_interval;
        if (next_frame < now) {
            next_frame = now + frame_interval;  // resync after a stall such as a blocking open()
        }
        std::this_thread::sleep_until(next_frame);
    }

    if (streamer) {
        // stop() falls back to another live source if one exists; otherwise it tells the device
        // to exit streaming now, without its ~3 s frame timeout.
        streamer->stop();
        // Destruction releases the buffer, and with it this streamer's claim on the device.
        streamer.reset();
        // The Api thread sends both commands in the background. Give them time to reach the
        // wire before Api is destroyed.
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "Stopped." << std::endl;
    return 0;
}
