#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <glm/vec3.hpp>

namespace nowave
{
enum class OutputType : int32_t
{
    Vocal = 0,
    SpeakerBox = 1,
    PA = 2
};

enum class InputType : int32_t
{
    Microphone = 0,
    Ears = 1,
    PA = 2
};

enum class MicrophonePattern : int32_t
{
    Omnidirectional = 0,
    Cardioid = 1,
    Supercardioid = 2,
    FigureEight = 3
};

struct alignas(16) RaytraceParameters
{
    float sourcePositions[2][4] {};
    float sourceDirections[2][4] {};
    float receiverPositions[2][4] {};
    float receiverDirections[2][4] {};
    int32_t configuration[4] {};
    int32_t patterns[4] {};
    float physics[4] {};
    int32_t limits[4] {};
};
static_assert(sizeof(RaytraceParameters) == 192);

inline glm::vec3 directionFromYawPitch(float yawDegrees, float pitchDegrees) noexcept
{
    const float yaw = yawDegrees * 0.01745329251994329577f;
    const float pitch = pitchDegrees * 0.01745329251994329577f;
    return {
        std::cos(pitch) * std::sin(yaw),
        std::sin(pitch),
        -std::cos(pitch) * std::cos(yaw)
    };
}

inline glm::vec3 rightFromYaw(float yawDegrees) noexcept
{
    const float yaw = yawDegrees * 0.01745329251994329577f;
    return { std::cos(yaw), 0.0f, std::sin(yaw) };
}

inline float patternGain(MicrophonePattern pattern, float forwardDot) noexcept
{
    const float cosine = std::clamp(forwardDot, -1.0f, 1.0f);
    switch (pattern)
    {
        case MicrophonePattern::Omnidirectional:
            return 1.0f;
        case MicrophonePattern::Cardioid:
            return std::max(0.0f, 0.5f * (1.0f + cosine));
        case MicrophonePattern::Supercardioid:
            return std::max(0.0f, 0.37f + 0.63f * cosine);
        case MicrophonePattern::FigureEight:
            return std::abs(cosine);
    }
    return 1.0f;
}
} // namespace nowave
