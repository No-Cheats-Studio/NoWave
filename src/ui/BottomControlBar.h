#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>

namespace nowave
{
class BottomControlBar final : public juce::Component
{
public:
    explicit BottomControlBar(juce::AudioProcessorValueTreeState& apvts,
                              std::function<void()> onLayoutChanged);
    ~BottomControlBar() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;
    int getPreferredHeight() const noexcept;

private:
    struct ObjectPanel
    {
        juce::Label header;
        juce::Colour accentColour;
        juce::ToggleButton positionToggle;
        bool positionExpanded { false };

        juce::Slider xSlider, ySlider, zSlider;
        juce::Label xLabel, yLabel, zLabel;

        juce::Slider yawSlider, pitchSlider;
        juce::Label yawLabel, pitchLabel;

        juce::Slider eqLowSlider, eqMidSlider, eqMidFreqSlider, eqHighSlider;
        juce::Label eqLowLabel, eqMidLabel, eqMidFreqLabel, eqHighLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> xAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> yAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> zAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> yawAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> pitchAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqLowAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqMidAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqMidFreqAtt;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> eqHighAtt;
    };

    void initObjectPanel(ObjectPanel& panel,
                         juce::AudioProcessorValueTreeState& apvts,
                         const juce::String& title,
                         juce::Colour accent,
                         const char* idX,
                         const char* idY,
                         const char* idZ,
                         const char* idYaw,
                         const char* idPitch,
                         const char* idLow,
                         const char* idMid,
                         const char* idMidFreq,
                         const char* idHigh);

    void layoutObjectPanel(ObjectPanel& panel, juce::Rectangle<int> bounds);
    void updateInputMenuAvailability();

    ObjectPanel sourcePanel;
    ObjectPanel listenerPanel;
    std::function<void()> onLayoutChanged;

    juce::TextEditor inputGainEditor;
    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAtt;

    juce::Slider dryWetSlider;
    juce::Label dryWetLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> dryWetAtt;

    juce::TextEditor outputGainEditor;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt;

    void commitGainEntry(juce::TextEditor& editor, juce::Slider& slider);

    juce::Label outputTypeLabel, inputTypeLabel;
    juce::Label microphonePatternLabel, leftPatternLabel, rightPatternLabel;
    juce::ComboBox outputTypeBox, inputTypeBox;
    juce::ComboBox microphonePatternBox, leftPatternBox, rightPatternBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> outputTypeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> inputTypeAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> microphonePatternAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> leftPatternAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> rightPatternAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BottomControlBar)
};
} // namespace nowave
