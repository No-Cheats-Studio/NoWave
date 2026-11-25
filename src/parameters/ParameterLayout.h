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
} // namespace ParamIDs

namespace StateProps
{
inline constexpr const char* roomSourceType = "roomSourceType";
inline constexpr const char* roomIdentifier = "roomIdentifier";
} // namespace StateProps

struct ParameterPointers
{
    std::atomic<float>* gainDb = nullptr;
    std::atomic<float>* inputGainDb = nullptr;
    std::atomic<float>* dryWetPercent = nullptr;
    std::atomic<float>* rayDensity = nullptr;
    std::atomic<float>* rayBounces = nullptr;
    std::atomic<float>* showRayPaths = nullptr;
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

    void bind(juce::AudioProcessorValueTreeState& apvts) noexcept;
};

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
} // namespace nowave

