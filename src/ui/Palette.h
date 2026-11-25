#pragma once

#include <cstdint>
#include <glm/vec3.hpp>
#include <juce_graphics/juce_graphics.h>

namespace nowave::palette
{
// Single source of truth for UI and 3D view colours (0xAARRGGBB).
namespace argb
{
constexpr uint32_t background = 0xff11141a;
constexpr uint32_t panel = 0xff141820;
constexpr uint32_t surface = 0xff171c24;
constexpr uint32_t surfaceRaised = 0xff1e2430;
constexpr uint32_t surfaceActive = 0xff2b3547;
constexpr uint32_t viewportButton = 0xff1e293b;
constexpr uint32_t dropHighlight = 0xff1d2b3f;
constexpr uint32_t field = 0xff202733;
constexpr uint32_t fieldOutline = 0xff4b5563;
constexpr uint32_t divider = 0xff262e3d;
constexpr uint32_t border = 0xff334155;
constexpr uint32_t overlay = 0xaa11141a;

constexpr uint32_t textStrong = 0xfff1f5f9;
constexpr uint32_t text = 0xffe2e8f0;
constexpr uint32_t textLabel = 0xffcbd5e1;
constexpr uint32_t textMuted = 0xffa3b1c6;
constexpr uint32_t textSubtle = 0xff94a3b8;

constexpr uint32_t source = 0xfff97316;
constexpr uint32_t sourceAlt = 0xfffb923c;
constexpr uint32_t sourceDirection = 0xfffcd34d;
constexpr uint32_t receiver = 0xff22d3ee;
constexpr uint32_t receiverAlt = 0xff3b82f6;
constexpr uint32_t receiverDirection = 0xffa5f3fc;
constexpr uint32_t receiverDirectionAlt = 0xff7dd3fc;
constexpr uint32_t accentSky = 0xff38bdf8;
constexpr uint32_t accentViolet = 0xffa78bfa;
constexpr uint32_t error = 0xfff87171;

constexpr uint32_t roomWireframe = 0xff7c9ad6;
constexpr uint32_t rayPath = receiver;
} // namespace argb

inline const juce::Colour background { argb::background };
inline const juce::Colour panel { argb::panel };
inline const juce::Colour surface { argb::surface };
inline const juce::Colour surfaceRaised { argb::surfaceRaised };
inline const juce::Colour surfaceActive { argb::surfaceActive };
inline const juce::Colour viewportButton { argb::viewportButton };
inline const juce::Colour dropHighlight { argb::dropHighlight };
inline const juce::Colour field { argb::field };
inline const juce::Colour fieldOutline { argb::fieldOutline };
inline const juce::Colour divider { argb::divider };
inline const juce::Colour border { argb::border };
inline const juce::Colour overlay { argb::overlay };

inline const juce::Colour textStrong { argb::textStrong };
inline const juce::Colour text { argb::text };
inline const juce::Colour textLabel { argb::textLabel };
inline const juce::Colour textMuted { argb::textMuted };
inline const juce::Colour textSubtle { argb::textSubtle };

inline const juce::Colour source { argb::source };
inline const juce::Colour sourceAlt { argb::sourceAlt };
inline const juce::Colour receiver { argb::receiver };
inline const juce::Colour accentSky { argb::accentSky };
inline const juce::Colour receiverDirectionAlt { argb::receiverDirectionAlt };
inline const juce::Colour accentViolet { argb::accentViolet };
inline const juce::Colour error { argb::error };

// Linear 0..1 RGB for OpenGL uniforms.
constexpr glm::vec3 rgb(uint32_t colour) noexcept
{
    return { static_cast<float>((colour >> 16) & 0xff) / 255.0f,
             static_cast<float>((colour >> 8) & 0xff) / 255.0f,
             static_cast<float>(colour & 0xff) / 255.0f };
}
} // namespace nowave::palette
