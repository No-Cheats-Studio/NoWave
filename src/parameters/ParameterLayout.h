#pragma once

#include "raytracing/AcousticSimulation.h"

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>

namespace nowave
{
namespace ParamIDs
{
inline constexpr const char* gain = "gain";
inline constexpr const char* inputGain = "input_gain";
inline constexpr const char* dryWet = "dry_wet";
inline constexpr const char* rayDensity = "ray_density";
inline constexpr const char* rayBounces = "ray_bounces";
inline constexpr const char* showRayPaths = "show_ray_paths";
inline constexpr const char* soundView = "sound_view";
inline constexpr const char* soundViewBand = "sound_view_band";
inline constexpr const char* soundViewArrival = "sound_view_arrival";
inline constexpr const char* outputType = "output_type";
inline constexpr const char* inputType = "input_type";
inline constexpr const char* microphonePattern = "microphone_pattern";
inline constexpr const char* leftMicrophonePattern = "left_microphone_pattern";
inline constexpr const char* rightMicrophonePattern = "right_microphone_pattern";

inline constexpr const char* srcX = "src_x";
inline constexpr const char* srcY = "src_y";
inline constexpr const char* srcZ = "src_z";
inline constexpr const char* srcYaw = "src_yaw";
inline constexpr const char* srcPitch = "src_pitch";
inline constexpr const char* srcEqLowGain = "src_eq_low_gain";
inline constexpr const char* srcEqMidGain = "src_eq_mid_gain";
inline constexpr const char* srcEqMidFreq = "src_eq_mid_freq";
inline constexpr const char* srcEqHighGain = "src_eq_high_gain";

inline constexpr const char* dstX = "dst_x";
inline constexpr const char* dstY = "dst_y";
inline constexpr const char* dstZ = "dst_z";
inline constexpr const char* dstYaw = "dst_yaw";
inline constexpr const char* dstPitch = "dst_pitch";
inline constexpr const char* dstEqLowGain = "dst_eq_low_gain";
inline constexpr const char* dstEqMidGain = "dst_eq_mid_gain";
inline constexpr const char* dstEqMidFreq = "dst_eq_mid_freq";
inline constexpr const char* dstEqHighGain = "dst_eq_high_gain";

// Per-channel placement, used only when Output Type is PA (left/right speaker).
inline constexpr const char* paSrcLeftX = "pa_src_left_x";
inline constexpr const char* paSrcLeftY = "pa_src_left_y";
inline constexpr const char* paSrcLeftZ = "pa_src_left_z";
inline constexpr const char* paSrcLeftYaw = "pa_src_left_yaw";
inline constexpr const char* paSrcLeftPitch = "pa_src_left_pitch";
inline constexpr const char* paSrcRightX = "pa_src_right_x";
inline constexpr const char* paSrcRightY = "pa_src_right_y";
inline constexpr const char* paSrcRightZ = "pa_src_right_z";
inline constexpr const char* paSrcRightYaw = "pa_src_right_yaw";
inline constexpr const char* paSrcRightPitch = "pa_src_right_pitch";

// Per-channel placement, used only when Input Type is PA (left/right microphone).
inline constexpr const char* paDstLeftX = "pa_dst_left_x";
inline constexpr const char* paDstLeftY = "pa_dst_left_y";
inline constexpr const char* paDstLeftZ = "pa_dst_left_z";
inline constexpr const char* paDstLeftYaw = "pa_dst_left_yaw";
inline constexpr const char* paDstLeftPitch = "pa_dst_left_pitch";
inline constexpr const char* paDstRightX = "pa_dst_right_x";
inline constexpr const char* paDstRightY = "pa_dst_right_y";
inline constexpr const char* paDstRightZ = "pa_dst_right_z";
inline constexpr const char* paDstRightYaw = "pa_dst_right_yaw";
inline constexpr const char* paDstRightPitch = "pa_dst_right_pitch";
} // namespace ParamIDs

namespace StateProps
{
inline constexpr const char* roomSourceType = "roomSourceType";
inline constexpr const char* roomIdentifier = "roomIdentifier";
inline constexpr const char* gpuDevice = "gpuDevice";
} // namespace StateProps

struct ParameterPointers
{
    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* dryWetPercent = nullptr;
    std::atomic<float>* rayDensity = nullptr;
    std::atomic<float>* rayBounces = nullptr;
    std::atomic<float>* showRayPaths = nullptr;
    std::atomic<float>* soundView = nullptr;
    std::atomic<float>* soundViewBand = nullptr;
    std::atomic<float>* soundViewArrival = nullptr;
    std::atomic<float>* outputType = nullptr;
    std::atomic<float>* inputType = nullptr;
    std::atomic<float>* microphonePattern = nullptr;
    std::atomic<float>* leftMicrophonePattern = nullptr;
    std::atomic<float>* rightMicrophonePattern = nullptr;

    std::atomic<float>* srcX = nullptr;
    std::atomic<float>* srcY = nullptr;
    std::atomic<float>* srcZ = nullptr;
    std::atomic<float>* srcYawDeg = nullptr;
    std::atomic<float>* srcPitchDeg = nullptr;
    std::atomic<float>* srcEqLowGainDb = nullptr;
    std::atomic<float>* srcEqMidGainDb = nullptr;
    std::atomic<float>* srcEqMidFreqHz = nullptr;
    std::atomic<float>* srcEqHighGainDb = nullptr;

    std::atomic<float>* dstX = nullptr;
    std::atomic<float>* dstY = nullptr;
    std::atomic<float>* dstZ = nullptr;
    std::atomic<float>* dstYawDeg = nullptr;
    std::atomic<float>* dstPitchDeg = nullptr;
    std::atomic<float>* dstEqLowGainDb = nullptr;
    std::atomic<float>* dstEqMidGainDb = nullptr;
    std::atomic<float>* dstEqMidFreqHz = nullptr;
    std::atomic<float>* dstEqHighGainDb = nullptr;

    // PA per-channel placement (index 0 = left, 1 = right).
    std::atomic<float>* paSrcX[2] {};
    std::atomic<float>* paSrcY[2] {};
    std::atomic<float>* paSrcZ[2] {};
    std::atomic<float>* paSrcYawDeg[2] {};
    std::atomic<float>* paSrcPitchDeg[2] {};

    std::atomic<float>* paDstX[2] {};
    std::atomic<float>* paDstY[2] {};
    std::atomic<float>* paDstZ[2] {};
    std::atomic<float>* paDstYawDeg[2] {};
    std::atomic<float>* paDstPitchDeg[2] {};

    void bind(juce::AudioProcessorValueTreeState& apvts) noexcept;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace nowave

