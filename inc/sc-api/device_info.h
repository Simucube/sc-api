/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_DEVICE_INFO_H_
#define SC_API_DEVICE_INFO_H_

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "device.h"
#include "device_info_fwd.h"
#include "sc-api/device_info_definitions.h"

namespace sc_api {

namespace detail {

class DeviceInfoProvider;

}  // namespace detail

namespace device_info {

struct VariableRef {
    DeviceSessionId  device_session_id;
    std::string_view id;
};

/** Physical controls of the device
 *
 * Physical representation of the device
 */
struct Control {
    /** Identity of this control
     *
     * Feedbacks and input refer to this control by this id
     */
    std::string_view id;

    /** Id of the parent control
     *
     * Most controls don't have parent
     */
    std::string_view parent_id;

    /** Name for this physical control
     *
     * If name isn't available, this is same as id
     */
    std::string_view name;

    /** Type of the control */
    ControlType type = ControlType::unknown;

    explicit operator bool() const { return !id.empty(); }
};

/** Input defines how user can communicate to PC
 */
struct Input {
    std::string_view id;

    /** Physical control that this input relates
     */
    std::string_view control;

    /** What kind of input this is? How the sim should read this data and interpret it? */
    InputType type = InputType::unknown;

    /** What is the intended use for this input?
     *
     * What should be the default input mapping for this? What action should occur when user interacts with this input?
     */
    InputRole role = InputRole::unknown;

    VariableRef variable;

    /** Variable value range, if relevant */
    float range_begin = 0.0f, range_end = 0.0f;

    explicit operator bool() const { return !id.empty(); }
};

/** Reference to input source that is mapped the hid input */
struct InputMapping {
    DeviceSessionId  device_id;
    std::string_view input_id;
};

struct HidAxisInput {
    InputRole role;
    int32_t   range_low = 0, range_high = 0;

    /** Which device inputs affect this hid axis?
     *
     * Usually just one, but it can be multiple in situations like dual clutch paddles
     */
    std::vector<InputMapping> mappings;
};

struct HidButtonInput {
    InputRole role = InputRole::unknown;

    std::vector<InputMapping> mappings;
};

struct UsbDeviceInfo {
    std::string hid_device_path;
    uint16_t    pid;
    uint16_t    vid;
};

/** Represents ways that the API user can use to control the devices and communicate to the person using devices
 *
 * Physical force feedback, controllable lights etc.
 */
struct Feedback {
    std::string_view id;

    /** Physical control that this feedback is connected */
    std::string_view control;
    FeedbackType     type     = FeedbackType::unknown;

    /** BSON data that includes type specific parameters */
    const uint8_t* parameters = nullptr;

    explicit operator bool() const { return !id.empty(); }
};

/** Helper struct for accessing RGB light feedback information
 *
 * Parses the BSON parameters from a Feedback entry to provide easy access to RGB light properties.
 */
struct RgbLightFeedback {
    /** FeedbackType this typed view parses. Lets DeviceInfo::getTypedFeedbacks() select it. */
    static constexpr FeedbackType k_feedback_type = FeedbackType::rgb_light;

    std::string_view id;
    std::string_view control;
    int32_t          index = -1;  ///< LED index, -1 if invalid

    /** Construct from a generic Feedback entry
     *
     * @param feedback Feedback entry to parse (should have type FeedbackType::rgb_light)
     * @return RgbLightFeedback with parsed index, or invalid if parsing fails
     */
    static RgbLightFeedback fromFeedback(const Feedback& feedback);

    /** Check if this represents a valid RGB light */
    explicit operator bool() const { return index >= 0; }
    bool     isValid() const { return index >= 0; }
};

/** Pixel format of a streamable device screen. */
enum class DashPixelFormat : uint8_t {
    rgb565,  ///< 16-bit RGB (5-6-5 layout), 2 bytes per pixel
};

constexpr std::string_view toString(DashPixelFormat f) {
    switch (f) {
        case DashPixelFormat::rgb565:
            return "rgb565";
    }
    return "rgb565";
}

constexpr std::optional<DashPixelFormat> dashPixelFormatFromString(std::string_view s) {
    if (s == "rgb565") return DashPixelFormat::rgb565;
    return std::nullopt;
}

/** Helper struct for accessing streamable screen feedback information
 *
 * Parses the BSON parameters from a FeedbackType::screen Feedback entry into screen geometry.
 */
struct ScreenFeedback {
    /** FeedbackType this typed view parses. Lets DeviceInfo::getTypedFeedbacks() select it. */
    static constexpr FeedbackType k_feedback_type = FeedbackType::screen;

    std::string_view id;
    std::string_view control;
    uint16_t         width        = 0;
    uint16_t         height       = 0;
    DashPixelFormat  pixel_format = DashPixelFormat::rgb565;

    /** Construct from a generic Feedback entry
     *
     * @param fb Feedback entry to parse (should have type FeedbackType::screen)
     * @return ScreenFeedback with parsed geometry, or invalid (width == 0) on type mismatch or
     *         missing/malformed parameters
     */
    static ScreenFeedback fromFeedback(const Feedback& fb);

    bool     isValid() const { return width != 0 && height != 0; }
    explicit operator bool() const { return isValid(); }
};

class DeviceInfo {
    friend class sc_api::detail::DeviceInfoProvider;
    friend class FullInfo;

    struct Data;

public:
    /** DeviceInfoProvider will handle constructing and filling DeviceInfo */
    DeviceInfo(const DeviceInfo&) = delete;
    DeviceInfo(DeviceInfo&&)      = delete;

    const std::vector<Control>& getControls() const { return d_.controls_; }
    const Control&              getControl(std::string_view id) const;

    const std::vector<Input>& getInputs() const { return d_.inputs_; }
    const Input&              getInput(std::string_view id) const;

    const std::vector<Feedback>& getFeedbacks() const { return d_.feedbacks_; }
    const Feedback&              getFeedback(std::string_view id) const;

    /** Returns true, if any of this device's feedbacks has the given FeedbackType */
    bool hasFeedbackType(FeedbackType type) const;

    /** Get all feedbacks of FeedbackT's type, parsed into the typed view FeedbackT.
     *
     * Generic form of getRgbLights(). FeedbackT must provide:
     *   - `static constexpr FeedbackType k_feedback_type` — the FeedbackType it represents,
     *   - `static FeedbackT fromFeedback(const Feedback&)` — parses a Feedback's parameters,
     *   - `bool isValid() const` — whether the parse produced a usable value.
     * Only valid parses are returned.
     */
    template <typename FeedbackT>
    std::vector<FeedbackT> getTypedFeedbacks() const {
        std::vector<FeedbackT> result;
        for (const auto& feedback : d_.feedbacks_) {
            if (feedback.type != FeedbackT::k_feedback_type) continue;
            FeedbackT parsed = FeedbackT::fromFeedback(feedback);
            if (parsed.isValid()) result.push_back(parsed);
        }
        return result;
    }

    /** Get the first feedback of FeedbackT's type, parsed into the typed view FeedbackT.
     *
     * Returns an invalid (default-constructed) FeedbackT if the device has no such feedback or the
     * parse fails. Same FeedbackT requirements as getTypedFeedbacks().
     */
    template <typename FeedbackT>
    FeedbackT getTypedFeedback() const {
        for (const auto& feedback : d_.feedbacks_) {
            if (feedback.type != FeedbackT::k_feedback_type) continue;
            FeedbackT parsed = FeedbackT::fromFeedback(feedback);
            if (parsed.isValid()) return parsed;
        }
        return FeedbackT{};
    }

    /** Get all RGB light feedbacks for this device
     *
     * Convenience wrapper for getTypedFeedbacks<RgbLightFeedback>().
     *
     * @return Vector of RgbLightFeedback with parsed LED indices
     */
    std::vector<RgbLightFeedback> getRgbLights() const;

    /** Get all streamable screen feedbacks for this device
     *
     * Convenience wrapper for getTypedFeedbacks<ScreenFeedback>().
     *
     * @return Vector of ScreenFeedback with parsed screen geometry
     */
    std::vector<ScreenFeedback> getScreens() const;

    const std::vector<HidAxisInput>   getHidAxisInput() const { return d_.hid_axis_; }
    const std::vector<HidButtonInput> getHidButtonInput() const { return d_.hid_buttons_; }

    bool isValid() const { return !d_.uid_.empty(); }

    std::string_view getUid() const { return d_.uid_; }
    DeviceSessionId  getSessionId() const { return d_.session_id_; }
    std::string_view getProductId() const { return d_.product_id_; }
    std::string_view getProductName() const { return d_.product_name_; }
    std::string_view getManufacturerId() const { return d_.manufacturer_id_; }
    std::string_view getManufacturerName() const { return d_.manufacturer_name_; }
    DeviceRole       getRole() const { return d_.role_; }

    DeviceSessionId getParentSessionId() const { return d_.parent_session_id_; }

    std::optional<UsbDeviceInfo> getUsbInfo() const { return d_.usb_info_; }

    BsonBuffer getRawBson() const;

    std::shared_ptr<const DeviceInfo> shared_from_this() const;
    std::shared_ptr<DeviceInfo>       shared_from_this();

private:
    DeviceInfo();
    void construct(Data&& data, FullInfo* info);

    void resolveReferences(FullInfo* info);

    struct Data {
        bool                         parse(const uint8_t* bson);
        std::string_view             uid_;
        DeviceSessionId              session_id_;
        DeviceSessionId              parent_session_id_;
        DeviceRole                   role_;
        std::vector<Control>         controls_;
        std::vector<Input>           inputs_;
        std::vector<Feedback>        feedbacks_;
        std::vector<HidAxisInput>    hid_axis_;
        std::vector<HidButtonInput>  hid_buttons_;
        std::optional<UsbDeviceInfo> usb_info_;

        const uint8_t* bson_     = nullptr;
        int32_t        info_rev_ = -1;

        std::string_view manufacturer_id_;
        std::string_view manufacturer_name_;
        std::string_view product_id_;
        std::string_view product_name_;
    } d_;

    FullInfo* full_info_ = nullptr;
};

/** Full device info data for all devices
 *
 * Immutable and requesting updated data will always return new instance if the data has changed.
 */
class FullInfo : public std::enable_shared_from_this<FullInfo> {
    friend class sc_api::detail::DeviceInfoProvider;

public:
    /** Iterator over const DeviceInfo& inside FullInfo
     *
     * Do note that generally DeviceInfo should be
     */
    class Iterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using difference_type   = std::ptrdiff_t;
        using value_type        = const DeviceInfo;
        using pointer           = const DeviceInfo*;
        using reference         = const DeviceInfo&;

        Iterator()              = default;
        Iterator(std::shared_ptr<const FullInfo> info, std::size_t idx) : info_(info), index_(idx) {}

        reference operator*() const { return info_->refByIndex(index_); }
        pointer   operator->() { return &info_->refByIndex(index_); }
        reference operator[](difference_type index) const { return info_->refByIndex(index_ + index); }

        DeviceInfoPtr asPointer() const { return info_->getByIndex(index_); }

        Iterator& operator++() {
            index_++;
            return *this;
        }
        Iterator& operator--() {
            index_--;
            return *this;
        }
        Iterator operator++(int) {
            Iterator tmp = *this;
            ++(*this);
            return tmp;
        }
        Iterator operator--(int) {
            Iterator tmp = *this;
            --(*this);
            return tmp;
        }

        friend bool operator==(const Iterator& a, const Iterator& b) {
            return a.info_ == b.info_ && a.index_ == b.index_;
        };
        friend bool operator!=(const Iterator& a, const Iterator& b) {
            return a.info_ != b.info_ || a.index_ != b.index_;
        };

    private:
        std::shared_ptr<const FullInfo> info_;
        std::size_t                     index_ = 0;
    };

    FullInfo(std::vector<DeviceInfo::Data>&& data, uint32_t revision, std::shared_ptr<const uint8_t[]> raw_bson);

    DeviceInfoPtr getByUid(std::string_view uid) const;
    DeviceInfoPtr getBySessionId(DeviceSessionId session_id) const;
    DeviceInfoPtr getByHidDevicePath(std::string_view path) const;

    std::size_t getDeviceCount() const { return devices_.size; }

    DeviceInfoPtr     getByIndex(std::size_t index) const { return ptr(&devices_.data[index]); }
    const DeviceInfo& refByIndex(std::size_t index) const { return devices_.data[index]; }

    uint32_t getRevisionNumber() const { return rev_; }

    Iterator begin() const { return Iterator(shared_from_this(), 0); }
    Iterator end() const { return Iterator(shared_from_this(), devices_.size); }

    DeviceInfoPtr              findFirstByFilter(const std::function<bool(const DeviceInfo& info)>& filter) const;
    std::vector<DeviceInfoPtr> findAllByFilter(const std::function<bool(const DeviceInfo& info)>& filter) const;
    DeviceSessionId findFirstSessionIdByFilter(const std::function<bool(const DeviceInfo& info)>& filter) const;
    std::vector<DeviceSessionId> findAllSessionIdsByFilter(
        const std::function<bool(const DeviceInfo& info)>& filter) const;

    const uint8_t* getRawBson() const { return raw_bson_.get(); }

private:
    DeviceInfoPtr ptr(const DeviceInfo* dev) const;

    struct DeviceSpan {
        std::unique_ptr<DeviceInfo[]> data;
        std::size_t                   size = 0;

        const DeviceInfo* begin() const { return data.get(); }
        const DeviceInfo* end() const { return &data[size]; }
    } devices_;

    uint32_t                         rev_;
    std::shared_ptr<const uint8_t[]> raw_bson_;
};

}  // namespace device_info

}  // namespace sc_api

#endif  // SC_API_DEVICE_INFO_H_
