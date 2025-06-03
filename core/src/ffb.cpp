#include "sc-api/core/ffb.h"

#include <cassert>
#include <cstring>
#include <functional>

#include "crypto/gcm.h"
#include "sc-api/core/command.h"
#include "sc-api/core/protocol/actions.h"
#include "sc-api/core/session.h"
#include "sc-api/core/util/bson_reader.h"

namespace sc_api::core {

bool buildEffectOffsetDataAction(ActionBuilder& builder, EffectPipelineRef pipeline, Clock::time_point timestamp,
                                 Clock::duration sample_time, const float* samples, unsigned sample_count) {
    if (sample_count == 0 || sample_count > 256) return false;

    struct Header {
        SC_API_PROTOCOL_ActionFbEffect_AAD_s aad;
        uint16_t                             device;
        SC_API_PROTOCOL_ActionFbEffect_Enc_t data;
    };

    uint8_t* payload                       = nullptr;

    Header* hdr                            = nullptr;

    uint32_t samples_size                  = sizeof(float) * sample_count;

    bool                    encrypt        = false;
    SecureSessionInterface* secure_session = builder.getSession()->getSecureSession();

    if (secure_session) {
        encrypt = true;
    }

    if (encrypt) {
        if (samples_size % 16 > 0) {
            samples_size += 16 - (samples_size % 16);
        }

        payload =
            builder.startBuilding(SC_API_PROTOCOL_ACTION_FB_EFFECT,
                                  sizeof(Header) + samples_size + sizeof(SC_API_PROTOCOL_EncryptedActionHeader_t) +
                                      sizeof(SC_API_PROTOCOL_EncryptedActionFooter_t),
                                  SC_API_PROTOCOL_ACTION_FLAG_ENCRYPTED);

        if (!payload) return false;

        hdr = new (payload + sizeof(SC_API_PROTOCOL_EncryptedActionHeader_t)) Header();
    } else {
        payload =
            builder.startBuilding(SC_API_PROTOCOL_ACTION_FB_EFFECT, sizeof(Header) + sizeof(float) * sample_count);

        if (!payload) return false;

        hdr = new (payload) Header();
    }

    hdr->aad.fb_pipeline_idx       = pipeline.pipeline_id;
    hdr->aad.flags                 = 0;

    hdr->device                    = pipeline.device_logical_id;

    hdr->data.sample_format        = SC_API_PROTOCOL_FB_SAMPLE_FORMAT_F32;
    hdr->data.sample_count_minus_1 = sample_count - 1;
    int64_t raw_timestamp          = timestamp.time_since_epoch().count();
    hdr->data.start_time_low       = raw_timestamp & 0xffffffffu;
    hdr->data.start_time_high      = raw_timestamp >> 32;
    hdr->data.sample_duration      = sample_time.count() & 0xffffffff;
    hdr->data.sample_duration_high = (sample_time.count() >> 32) & 0xff;

    if (encrypt) {
        std::memcpy(&payload[sizeof(SC_API_PROTOCOL_EncryptedActionHeader_t) + sizeof(Header)], samples, samples_size);
        secure_session->encrypt(payload, (uint8_t*)&hdr->aad, sizeof(SC_API_PROTOCOL_ActionFbEffect_AAD_t),
                                (uint8_t*)&hdr->data, sizeof(SC_API_PROTOCOL_ActionFbEffect_Enc_t) + samples_size,
                                (uint8_t*)&hdr->data + sizeof(SC_API_PROTOCOL_ActionFbEffect_Enc_t) + samples_size);
    } else {
        std::memcpy(&payload[sizeof(Header)], samples, samples_size);
    }

    return true;
}

bool buildEffectClearAction(ActionBuilder& builder, EffectPipelineRef pipeline) {
    struct Payload {
        SC_API_PROTOCOL_ActionFbEffect_AAD_s aad;
        uint16_t                             device;
        SC_API_PROTOCOL_ActionFbClear_Enc_t  data;
    } payload;

    payload.device                      = pipeline.device_logical_id;
    payload.data.cleared_pipeline_count = 1;
    payload.data.fb_pipelines[0]        = pipeline.pipeline_id;

    uint8_t* payload_buf = builder.startBuilding(SC_API_PROTOCOL_ACTION_FB_EFFECT_CLEAR, sizeof(Payload));
    if (!payload_buf) return false;
    std::memcpy(payload_buf, &payload, sizeof(payload));
    return true;
}

FfbPipeline::FfbPipeline(const std::shared_ptr<Session>& session, DeviceSessionId device)
    : action_builder_(session), device_(device) {
    assert(device);
}

FfbPipeline::~FfbPipeline() {
    if (isActive()) {
        sc_api::core::CommandRequest req("ffb", "free_pipeline");
        req.docAddElement("device_session_id", device_.id);
        req.docAddElement("pipeline_id", pipeline_id_);
        action_builder_.getSession()->asyncCommand(std::move(req), [](const sc_api::core::AsyncCommandResult&) {});
    }
}

bool FfbPipeline::configure(const PipelineConfig& config) {
    std::string offset_type_str, interpolation_type_str, filter_type_str;

    switch (config.offset_type) {
        case OffsetType::force_N:
            offset_type_str = "force";
            break;
        case OffsetType::force_relative:
            offset_type_str = "force_relative";
            break;
        case OffsetType::position_mm:
            offset_type_str = "position";
            break;
        case OffsetType::torque_Nm:
            offset_type_str = "torque";
            break;
    }

    switch (config.interpolation_type) {
        case InterpolationType::linear:
            interpolation_type_str = "linear";
            break;
        case InterpolationType::none:
            interpolation_type_str = "none";
            break;
    }

    switch (config.filter_type) {
        case FilterType::low_pass:
            filter_type_str = "low_pass";
            break;
        case FilterType::none:
            filter_type_str = "none";
            break;
        case FilterType::slew_rate_limit:
            filter_type_str = "slew_rate_limit";
            break;
    }

    CommandRequest req{"ffb", "configure_pipeline"};
    req.docAddElement("device_session_id", device_.id);
    req.docAddElement("offset_mode", offset_type_str);
    req.docAddElement("interpolation_mode", interpolation_type_str);
    req.docAddElement("filter_mode", filter_type_str);
    req.docAddElement("filter_parameter", config.filter_parameter);

    if (pipeline_id_ >= 0) {
        req.docAddElement("pipeline_id", pipeline_id_);
    }

    sc_api::core::CommandResult result = action_builder_.getSession()->blockingCommand(std::move(req));
    if (result.isSuccess()) {
        sc_api::core::util::BsonReader reader(result.getPayload().data(), result.getPayload().size());

        int32_t pipeline_id = -1;
        reader.tryFindAndGet("pipeline_id", pipeline_id);

        pipeline_id_ = (int8_t)pipeline_id;
        config_      = config;
        return true;
    } else {
        return false;
    }
}

bool FfbPipeline::generateEffect(Clock::time_point start_timestamp, Clock::duration sample_time, const float* samples,
                                 unsigned sample_count) {
    assert(sample_count <= 256);
    if (pipeline_id_ < 0) return false;

    EffectPipelineRef ref = {device_.id, (uint8_t)pipeline_id_};

    if (!sc_api::core::buildEffectOffsetDataAction(action_builder_, ref, start_timestamp, sample_time, samples,
                                             sample_count)) {
        return false;
    }

    return action_builder_.sendNonBlocking() == ActionResult::complete;
}

bool FfbPipeline::stop() {
    if (pipeline_id_ < 0) return false;

    EffectPipelineRef ref = {device_.id, (uint8_t)pipeline_id_};
    sc_api::core::buildEffectClearAction(action_builder_, ref);
    return action_builder_.sendNonBlocking() == ActionResult::complete;
}

bool FfbPipeline::isActive() const {
    return pipeline_id_ != -1 && action_builder_.getSession()->getState() == SessionState::connected_control;
}

bool FfbPipeline::remove() {
    if (pipeline_id_ < 0) return true;

    sc_api::core::CommandRequest req("ffb", "free_pipeline");
    req.docAddElement("device_session_id", device_.id);
    req.docAddElement("pipeline_id", pipeline_id_);
    CommandResult result = action_builder_.getSession()->blockingCommand(std::move(req));

    if (result.isSuccess()) {
        pipeline_id_ = -1;
        return true;
    }
    return false;
}

}  // namespace sc_api::core
