#include "ui/LevelLegendComponent.h"
#include "raytracing/AcousticSimulation.h"
#include "ui/Palette.h"

#include <cmath>

namespace nowave
{
LevelLegendComponent::LevelLegendComponent(const std::atomic<float>* soundViewParameter)
    : soundView(soundViewParameter)
{
    setInterceptsMouseClicks(false, false);
    setVisible(false);
    startTimerHz(10);
}

LevelLegendComponent::~LevelLegendComponent() = default;

void LevelLegendComponent::timerCallback()
{
    const float value = soundView != nullptr ? soundView->load(std::memory_order_relaxed)
                                             : static_cast<float>(SoundViewMode::Cloud);
    const bool cloud = static_cast<int>(std::round(value)) == static_cast<int>(SoundViewMode::Cloud);
    if (cloud != isVisible())
        setVisible(cloud);
}

void LevelLegendComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds();
    g.setColour(palette::overlay);
    g.fillRoundedRectangle(area.toFloat(), 4.0f);
    area.reduce(8, 6);

    g.setColour(palette::textStrong);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText("Level (dB)", area.removeFromTop(16), juce::Justification::centredLeft);
    area.removeFromTop(4);

    constexpr float floorDb = palette::kLevelFloorDb;
    constexpr float ceilingDb = palette::kLevelCeilingDb;
    auto bar = area.removeFromLeft(14);
    const int barHeight = bar.getHeight();
    for (int y = 0; y < barHeight; ++y)
    {
        const float t = 1.0f - static_cast<float>(y) / static_cast<float>(juce::jmax(1, barHeight - 1));
        g.setColour(palette::levelColour(t));
        g.fillRect(bar.getX(), bar.getY() + y, bar.getWidth(), 1);
    }
    g.setColour(palette::border);
    g.drawRect(bar.expanded(1), 1);

    g.setColour(palette::textLabel);
    g.setFont(juce::FontOptions(11.0f));
    area.removeFromLeft(6);
    for (float db = ceilingDb; db >= floorDb - 0.1f; db -= 20.0f)
    {
        const float t = (db - floorDb) / (ceilingDb - floorDb);
        const int y = bar.getY() + static_cast<int>((1.0f - t) * static_cast<float>(barHeight - 1));
        g.drawText(juce::String(static_cast<int>(db)), area.getX(), y - 8, area.getWidth(), 16,
                   juce::Justification::centredLeft);
    }
}
} // namespace nowave
