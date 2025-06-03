#include "sc-api/core/device_info.h"

#include <cassert>
#include <charconv>

#include "device_info_internal.h"
#include "sc-api/core/util/bson_reader.h"

namespace sc_api::core::internal {

DeviceInfoProvider::DeviceInfoProvider() {}

DeviceInfoProvider::~DeviceInfoProvider() {}

void DeviceInfoProvider::initialize(const void* shm_buffer, size_t shm_buffer_size) {
    setShmBuffer((const uint8_t*)shm_buffer, shm_buffer_size);
}

std::shared_ptr<device_info::FullInfo> DeviceInfoProvider::parseDeviceInfo() {
    std::shared_ptr<const uint8_t[]> bson_buffer;
    uint32_t                         buf_size = 0;
    uint32_t                         revision = 0;
    {
        std::shared_lock lock(mutex_);
        if (cached_device_info_ && cached_device_info_->getRevisionNumber() == getActiveBufferRevision()) {
            return cached_device_info_;
        }

        bson_buffer = getActiveBuffer();
        buf_size    = getActiveBufferSize();
        revision    = getActiveBufferRevision();
    }

    util::BsonReader r(bson_buffer.get(), buf_size);

    using device_info::DeviceInfo;
    std::vector<DeviceInfo::Data> devs;

    while (!r.atEnd()) {
        using EType = util::BsonReader::ElementType;
        EType e     = r.next();
        if (e == EType::ELEMENT_DOC) {
            devs.emplace_back(DeviceInfo::Data{});
            auto& dev = devs.back();
            if (!dev.parse(r.subdocument().first)) {
                devs.pop_back();
            }
        }
    }

    std::shared_ptr<device_info::FullInfo> info =
        std::make_shared<device_info::FullInfo>(std::move(devs), revision, std::move(bson_buffer));

    {
        std::lock_guard lock(mutex_);

        // We theoretically could replace newer device info if multiple threads call this at the same time and get
        // different revisions of the data, and then do the parsing with different speed, but it is really unlikely
        // to happen and only results in duplicate work that we are not making any special checks for that
        cached_device_info_ = info;
    }

    return info;
}

bool DeviceInfoProvider::parseAndValidateNewData(const uint8_t* new_data, size_t size) {
    util::BsonReader reader(new_data, size);
    return reader.validate();
}

}  // namespace sc_api::core::internal

namespace sc_api::core::device_info {

DeviceInfo::DeviceInfo() : full_info_(nullptr) {}

void DeviceInfo::construct(Data&& data, FullInfo* info) {
    d_         = std::move(data);
    full_info_ = info;
}

static std::vector<Control> parseControls(util::BsonReader& r) {
    using E = util::BsonReader::ElementType;
    std::vector<Control> controls;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e != E::ELEMENT_DOC) continue;

        Control c;
        c.id = r.key();

        r.beginSub();
        while (true) {
            e = r.next();
            if (util::BsonReader::isEndOrError(e)) break;

            if (e == E::ELEMENT_STR) {
                if (r.key() == "name") {
                    c.name = r.stringValue();
                } else if (r.key() == "role") {
                    c.type = controlTypeFromString(r.stringValue());
                } else if (r.key() == "parent") {
                    c.parent_id = r.stringValue();
                }
            }
        }
        r.endSub();
        controls.push_back(c);
    }
    return controls;
}

static std::vector<Input> parseInputs(util::BsonReader& r, DeviceSessionId this_device_id) {
    using E = util::BsonReader::ElementType;
    std::vector<Input> inputs;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e != E::ELEMENT_DOC) continue;

        Input c;
        c.id = r.key();

        r.beginSub();
        while (true) {
            e = r.next();
            if (util::BsonReader::isEndOrError(e)) break;

            if (e == E::ELEMENT_STR) {
                if (r.key() == "variable") {
                    std::string_view var                   = r.stringValue();

                    std::string_view::size_type dev_id_pos = var.find(':');
                    if (dev_id_pos == std::string_view::npos) {
                        c.variable.id                = var;
                        c.variable.device_session_id = this_device_id;
                    } else {
                        if (std::from_chars(var.data(), var.data() + dev_id_pos, c.variable.device_session_id.id).ec ==
                            std::errc()) {
                            c.variable.id = var.substr(dev_id_pos + 1);
                        } else {
                            // Failed?
                            c.variable.device_session_id = this_device_id;
                            c.variable.id                = var;
                        }
                    }
                } else if (r.key() == "role") {
                    c.role = inputRoleFromString(r.stringValue());
                } else if (r.key() == "type") {
                    c.type = inputTypeFromString(r.stringValue());
                } else if (r.key() == "control") {
                    c.control = r.stringValue();
                }
            }
        }
        r.endSub();
        inputs.push_back(c);
    }
    return inputs;
}

static std::vector<Feedback> parseFeedbacks(util::BsonReader& r) {
    using E = util::BsonReader::ElementType;
    std::vector<Feedback> controls;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e != E::ELEMENT_DOC) continue;

        Feedback c;
        c.id = r.key();

        r.beginSub();
        while (true) {
            e = r.next();
            if (util::BsonReader::isEndOrError(e)) break;

            if (e == E::ELEMENT_STR) {
                if (r.key() == "control") {
                    c.control = r.stringValue();
                } else if (r.key() == "type") {
                    c.type = feedbackTypeFromString(r.stringValue());
                }
            } else if (e == E::ELEMENT_DOC) {
                c.parameters = r.subdocument().first;
            }
        }
        r.endSub();
        controls.push_back(c);
    }
    return controls;
}

static std::vector<InputMapping> parseMappings(util::BsonReader& r) {
    using E = util::BsonReader::ElementType;
    std::vector<InputMapping> mappings;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e == E::ELEMENT_DOC) {
            InputMapping m;
            r.beginSub();
            while (true) {
                e = r.next();
                if (util::BsonReader::isEndOrError(e)) break;

                if (e == E::ELEMENT_STR && r.key() == "input") {
                    m.input_id = r.stringValue();
                } else if (e == util::BsonReader::ELEMENT_I32) {
                    m.device_id.id = (uint16_t)r.int32Value();
                }
            }
            r.endSub();
            mappings.push_back(m);
        }
    }

    return mappings;
}

static std::vector<HidAxisInput> parseHidAxisInputs(util::BsonReader& r) {
    using E = util::BsonReader::ElementType;
    std::vector<HidAxisInput> axis;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e != E::ELEMENT_DOC) continue;

        HidAxisInput c;

        r.beginSub();
        while (true) {
            e = r.next();
            if (util::BsonReader::isEndOrError(e)) break;

            if (e == E::ELEMENT_STR && r.key() == "role") {
                c.role = inputRoleFromString(r.stringValue());
            } else if (e == E::ELEMENT_ARRAY && r.key() == "mappings") {
                r.beginSub();
                c.mappings = parseMappings(r);
                r.endSub();
            } else if (e == E::ELEMENT_ARRAY && r.key() == "range") {
                r.beginSub();
                if (r.next() == E::ELEMENT_I32) {
                    int32_t low = r.int32Value();
                    if (r.next() == E::ELEMENT_I32) {
                        c.range_high = r.int32Value();
                        c.range_low  = low;
                    }
                }
                r.endSub();
            }
        }
        r.endSub();
        axis.push_back(c);
    }

    return axis;
}
static std::vector<HidButtonInput> parseHidButtonInputs(util::BsonReader& r) {
    using E = util::BsonReader::ElementType;
    std::vector<HidButtonInput> buttons;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;
        if (e != E::ELEMENT_DOC) continue;

        HidButtonInput c;

        r.beginSub();
        while (true) {
            e = r.next();
            if (util::BsonReader::isEndOrError(e)) break;

            if (e == E::ELEMENT_STR && r.key() == "role") {
                c.role = inputRoleFromString(r.stringValue());
            } else if (e == E::ELEMENT_ARRAY && r.key() == "mappings") {
                r.beginSub();
                c.mappings = parseMappings(r);
                r.endSub();
            }
        }
        r.endSub();

        buttons.push_back(c);
    }

    return buttons;
}

static std::pair<std::vector<HidAxisInput>, std::vector<HidButtonInput>> parseHidInputs(util::BsonReader& r) {
    std::pair<std::vector<HidAxisInput>, std::vector<HidButtonInput>> hid_inputs;

    using E = util::BsonReader::ElementType;
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;

        if (e == E::ELEMENT_ARRAY) {
            if (r.key() == "axis") {
                r.beginSub();
                hid_inputs.first = parseHidAxisInputs(r);
                r.endSub();
            } else if (r.key() == "buttons") {
                r.beginSub();
                hid_inputs.second = parseHidButtonInputs(r);
                r.endSub();
            }
        }
    }

    return hid_inputs;
}

bool DeviceInfo::Data::parse(const uint8_t* bson) {
    bson_   = bson;

    using E = util::BsonReader::ElementType;
    util::BsonReader r(bson);
    int32_t          this_device_id;
    std::string_view uid;
    if (!r.tryFindAndGet("logical_id", this_device_id) || !r.tryFindAndGet("device_uid", uid)) return false;
    session_id_.id = this_device_id;
    uid_           = uid;

    UsbDeviceInfo usb_info{};
    while (true) {
        E e = r.next();
        if (util::BsonReader::isEndOrError(e)) break;

        if (e == E::ELEMENT_DOC) {
            r.beginSub();
            if (r.key() == "control") {
                controls_ = parseControls(r);
            } else if (r.key() == "input") {
                inputs_ = parseInputs(r, session_id_);
            } else if (r.key() == "feedback") {
                feedbacks_ = parseFeedbacks(r);
            } else if (r.key() == "hid_input") {
                auto [axis, buttons] = parseHidInputs(r);
                hid_axis_            = std::move(axis);
                hid_buttons_         = std::move(buttons);
            }
            r.endSub();
        } else if (e == E::ELEMENT_STR) {
            if (r.key() == "role") {
                role_ = deviceRoleFromString(r.stringValue());
            } else if (r.key() == "usb_path") {
                usb_info.hid_device_path = r.stringValue();
            }
        } else if (e == E::ELEMENT_I32) {
            if (r.key() == "usb_pid") {
                usb_info.pid = r.int32Value();
            } else if (r.key() == "usb_vid") {
                usb_info.vid = r.int32Value();
            }
        } else if (e == E::ELEMENT_BOOL) {
            if (r.key() == "is_connected") {
                is_connected_ = r.boolValue();
            }
        }
    }

    if (!usb_info.hid_device_path.empty()) {
        usb_info_ = usb_info;
    } else {
        usb_info_.reset();
    }

    return !r.error();
}

void DeviceInfo::resolveReferences(FullInfo* info) { full_info_ = info; }

const Control& DeviceInfo::getControl(std::string_view id) const {
    static constexpr Control k_default = {};
    for (const auto& control : d_.controls_) {
        if (control.id == id) return control;
    }
    return k_default;
}

const Input& DeviceInfo::getInput(std::string_view id) const {
    static constexpr Input k_default = {};
    for (const auto& input : d_.inputs_) {
        if (input.id == id) return input;
    }
    return k_default;
}

const Feedback& DeviceInfo::getFeedback(std::string_view id) const {
    static constexpr Feedback k_default = {};
    for (const auto& feedback : d_.feedbacks_) {
        if (feedback.id == id) return feedback;
    }
    return k_default;
}

bool DeviceInfo::hasFeedbackType(FeedbackType type) const {
    for (const auto& feedback : d_.feedbacks_) {
        if (feedback.type == type) return true;
    }
    return false;
}

BsonBuffer DeviceInfo::getRawBson() const {
    assert(full_info_);
    return BsonBuffer{std::shared_ptr<const uint8_t[]>(full_info_->shared_from_this(), d_.bson_)};
}

std::shared_ptr<DeviceInfo> DeviceInfo::shared_from_this() {
    assert(full_info_);
    return std::shared_ptr<DeviceInfo>(full_info_->shared_from_this(), this);
}

std::shared_ptr<const DeviceInfo> DeviceInfo::shared_from_this() const {
    assert(full_info_);
    return std::shared_ptr<const DeviceInfo>(full_info_->shared_from_this(), this);
}

FullInfo::FullInfo(std::vector<DeviceInfo::Data>&& data, uint32_t revision, std::shared_ptr<const uint8_t[]> raw_bson)
    : rev_(revision), raw_bson_(std::move(raw_bson)) {
    devices_.data = std::unique_ptr<DeviceInfo[]>(new DeviceInfo[data.size()]);
    devices_.size = data.size();

    auto out_it   = devices_.data.get();
    for (DeviceInfo::Data& d : data) {
        out_it->construct(std::move(d), this);
        ++out_it;
    }
}

DeviceInfoPtr FullInfo::getByUid(std::string_view uid) const {
    for (const auto& dev : devices_) {
        if (dev.getUid() == uid) return ptr(&dev);
    }

    return nullptr;
}

DeviceInfoPtr FullInfo::getBySessionId(DeviceSessionId session_id) const {
    for (const auto& dev : devices_) {
        if (dev.getSessionId() == session_id) return ptr(&dev);
    }

    return nullptr;
}

DeviceInfoPtr FullInfo::getByHidDevicePath(std::string_view path) const {
    for (const auto& dev : devices_) {
        auto opt_usb_info = dev.getUsbInfo();
        if (opt_usb_info && opt_usb_info->hid_device_path == path) return ptr(&dev);
    }
    return nullptr;
}

DeviceInfoPtr FullInfo::findFirstByFilter(const std::function<bool(const DeviceInfo&)>& filter) const {
    for (const auto& dev : devices_) {
        if (filter(dev)) return ptr(&dev);
    }

    return nullptr;
}

std::vector<DeviceInfoPtr> FullInfo::findAllByFilter(const std::function<bool(const DeviceInfo&)>& filter) const {
    std::vector<DeviceInfoPtr> devs;
    for (const auto& dev : devices_) {
        if (filter(dev)) {
            devs.push_back(ptr(&dev));
        }
    }

    return devs;
}

DeviceSessionId FullInfo::findFirstSessionIdByFilter(const std::function<bool(const DeviceInfo&)>& filter) const {
    for (const auto& dev : devices_) {
        if (filter(dev)) return dev.getSessionId();
    }

    return k_invalid_device_session_id;
}

std::vector<DeviceSessionId> FullInfo::findAllSessionIdsByFilter(
    const std::function<bool(const DeviceInfo&)>& filter) const {
    std::vector<DeviceSessionId> devs;
    for (const auto& dev : devices_) {
        if (filter(dev)) {
            devs.push_back(dev.getSessionId());
        }
    }

    return devs;
}

DeviceInfoPtr FullInfo::ptr(const DeviceInfo* dev) const {
    return std::shared_ptr<const DeviceInfo>(shared_from_this(), dev);
}

}  // namespace sc_api::core::device_info
