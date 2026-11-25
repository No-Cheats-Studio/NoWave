#pragma once

#include "PluginProcessor.h"
#include "ui/BottomControlBar.h"
#include "ui/StartScreenComponent.h"
#include "ui/Viewport3DComponent.h"

#include <juce_gui_basics/juce_gui_basics.h>

class NoWaveAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit NoWaveAudioProcessorEditor(NoWaveAudioProcessor& processor);
    ~NoWaveAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void loadRoomFromFile(const juce::File& file);
    void loadRoomPreset(nowave::RoomPresetId presetId);
    void showRoomView(std::shared_ptr<const nowave::RoomMesh> mesh);
    void showStartScreen();

    NoWaveAudioProcessor& audioProcessor;

    nowave::StartScreenComponent startScreen;
    nowave::Viewport3DComponent viewport3D;
    nowave::BottomControlBar bottomControlBar;
    juce::TooltipWindow tooltipWindow { this, 700 };

    bool isRoomLoaded { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoWaveAudioProcessorEditor)
};
