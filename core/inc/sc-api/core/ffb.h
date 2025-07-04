/**
 * @file
 * @brief
 *
 */

#ifndef SC_API_CORE_FFB_H_
#define SC_API_CORE_FFB_H_

#include <cassert>

#include "action.h"
#include "device.h"
#include "events.h"
#include "time.h"

namespace sc_api::core {

struct EffectPipelineRef {
    uint16_t device_logical_id;
    uint8_t  pipeline_id;
};

/** How sample values are interpreted */
/**
 * @brief Force feedback effect offset type
 */
enum class OffsetType {
    /** Torque in newton meters, positive values rotate clockwise */
    torque_Nm,

    /** Pedal face force offset, positive values push towards the driver */
    force_N,

    /** Force offset that is relative to the currently measured force
     *
     * -0.5 makes pedal 50% more soft as the target force is cut in half
     * 0.5 increases force by 50%
     */
    force_relative,

    /** Offsets pedal force curve by millimeter position offset
     *
     * Positive offset moves pedal towards the driver.
     *
     * @note Maximum position offset changes with the pedal position and the configuration, because of physical
     * limitations of the movement range.
     */
    position_mm,
};

/**
 * @brief Force feedback effect interpolation type
 */
enum class InterpolationType {
    /** Value sampling uses nearest timestamped sample */
    none,

    /** Value sampling does linear interpolation between samples */
    linear,
};

/** Filtering that is applied to the sampled value post-interpolation
 *
 *  filter_parameter meaning depends on this type
 */
enum class FilterType {
    /** No filter is applied to the sampled values */
    none,

    /** Simple first order low-pass filter is applied that has cut-off frequency filter_parameter (Hz) */
    low_pass,

    /** Output offset change is limited to filter_parameter (offset_unit/s). */
    slew_rate_limit,
};

/**
 * @struct PipelineConfig
 * @brief Configuration for force feedback effect pipeline
 */
struct PipelineConfig {
    /** Type of offset that is generated by this pipeline
     *
     * Supported offset types depend on the device type.
     */
    OffsetType        offset_type;
    InterpolationType interpolation_type = InterpolationType::linear;

    /** Gain multiplier that is applied to the offset samples */
    float gain                           = 1.0f;

    /** @see FilterType */
    FilterType filter_type               = FilterType::none;

    /** @see FilterType */
    float filter_parameter               = 1.0f;

    constexpr bool operator==(const PipelineConfig& b) const {
        return offset_type == b.offset_type && interpolation_type == b.interpolation_type && gain == b.gain &&
               filter_type == b.filter_type && filter_parameter == b.filter_parameter;
    }
    constexpr bool operator!=(const PipelineConfig& b) const { return !(*this == b); }
};

bool buildEffectOffsetDataAction(ActionBuilder& builder, EffectPipelineRef pipeline, Clock::time_point start_timestamp,
                                 Clock::duration sample_time, const float* samples, unsigned sample_count);
bool buildEffectClearAction(ActionBuilder& builder, EffectPipelineRef pipeline);

/** Handle to a single effect pipeline
 */
class FfbPipeline {
public:
    /** Construct pipeline handle for a device
     *
     *  @param session Api session
     *  @param device Device session specific id which this pipeline should affect
     */
    explicit FfbPipeline(const std::shared_ptr<Session>& session, DeviceSessionId device);
    FfbPipeline(const FfbPipeline&) = delete;
    ~FfbPipeline();

    FfbPipeline& operator=(const FfbPipeline&)     = delete;
    FfbPipeline& operator=(FfbPipeline&&) noexcept = delete;

    /** Configure pipeline settings
     *
     * Will pick one of the currently free pipeline slots.
     * If this handle is already configured, then previous pipeline is first freed and then new configuration is applied
     */
    bool configure(const PipelineConfig& config);

    /** Send new sample set to the configured pipeline
     *
     * Consecutive sample set timestamps must always be increasing.
     * Any samples that have timestamp earlier than the current time on the device, will be ignored.
     * Later arriving sample set will overwrite earlier samples if they overlap.
     *
     * Total length of the sample set is (sample_time * sample_count) / sc_api::core::time::getTimestampFrequencyHz() seconds.
     *
     * @param start_timestamp Timestamp when the effect should start
     * @param sample_time Duration that each sample should take
     * @param samples Pointer to a buffer that contains sample_count samples that should be sent
     * @param sample_count Number of samples in the samples buffer. Buffer only has to be valid during this call as
     *                     values are copied to separate buffer
     * @return true, if the samples were sent to the API backend and should be played by the device
     *         false, if pipeline configuration hasn't completed yet or is invalid (eg. device is not connected)
     */
    bool generateEffect(Clock::time_point start_timestamp, Clock::duration sample_time, const float* samples,
                        unsigned sample_count);

    /** Will immediately stop currently active effect and clear all buffered samples, but won't clear pipeline
     * configuration */
    bool stop();

    /** Is pipeline currently connected to a device and generateEffect calls will actually affect something */
    bool isActive() const;

    /**
     * @brief Remove pipeline. Stop any currently generated effect
     *
     * Pipeline must be configured again to re-enable it
     */
    bool remove();

    /**
     * Get id of the pipeline configured to device
     * @return pipeline id
     */
    int8_t getPipelineId() { return pipeline_id_; }

    PipelineConfig getConfig() { return config_; }

    /** Get DeviceSessionId of the device this EffectPipelineHandle is assigned for
     */
    DeviceSessionId getDevice() const { return device_; }

    std::shared_ptr<Session> getSession() const { return action_builder_.getSession(); }

private:
    ActionBuilder action_builder_;

    DeviceSessionId device_      = k_invalid_device_session_id;
    int8_t          pipeline_id_ = -1;
    PipelineConfig  config_;
};

}  // namespace sc_api::core

#endif  // SC_API_CORE_FFB_H_
