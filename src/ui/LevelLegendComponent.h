#pragma once

#include <atomic>
#include <juce_gui_basics/juce_gui_basics.h>

namespace nowave
{
// dB colour scale for the sound cloud. Only visible while the Cloud view is active.
class LevelLegendComponent final : public juce::Component,
                                   private juce::Timer
{
public:
    explicit LevelLegendComponent(const std::atomic<float>* soundViewParameter);
    ~LevelLegendComponent() override;

    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    const std::atomic<float>* soundView;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LevelLegendComponent)
};
} // namespace nowave
