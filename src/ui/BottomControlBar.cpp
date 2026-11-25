#include "ui/BottomControlBar.h"
#include "parameters/ParameterLayout.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace nowave
{
namespace
{
void configureRotary(juce::Slider& slider,
                     juce::Label& label,
                     const juce::String& text,
                     juce::Colour accent,
                     juce::Component& parent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, 18);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::thumbColourId, accent);
    parent.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.5f));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    label.setJustificationType(juce::Justification::centred);
    parent.addAndMakeVisible(label);
}

void configureAxisSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         juce::Colour accent,
                         juce::Component& parent)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, 22);
    slider.setColour(juce::Slider::trackColourId, accent.withAlpha(0.7f));
    slider.setColour(juce::Slider::thumbColourId, accent);
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffe2e8f0));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff202733));
    parent.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    label.setJustificationType(juce::Justification::centredLeft);
    parent.addAndMakeVisible(label);
}

void configureChoice(juce::ComboBox& box,
                     juce::Label& label,
                     const juce::String& text,
                     juce::Component& parent)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    parent.addAndMakeVisible(label);

    box.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff202733));
    box.setColour(juce::ComboBox::textColourId, juce::Colour(0xffe2e8f0));
    box.setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff374151));
    parent.addAndMakeVisible(box);
}
} // namespace

BottomControlBar::BottomControlBar(juce::AudioProcessorValueTreeState& apvts,
                                   std::function<void()> layoutChanged)
    : onLayoutChanged(std::move(layoutChanged))
{
    initObjectPanel(sourcePanel,
                    apvts,
                    "SOURCE (EMITTER)",
                    juce::Colour(0xfff97316),
                    ParamIDs::srcX,
                    ParamIDs::srcY,
                    ParamIDs::srcZ,
                    ParamIDs::srcYaw,
                    ParamIDs::srcPitch,
                    ParamIDs::srcEqLowGain,
                    ParamIDs::srcEqMidGain,
                    ParamIDs::srcEqMidFreq,
                    ParamIDs::srcEqHighGain);

    initObjectPanel(listenerPanel,
                    apvts,
                    "LISTENER (RECEIVER)",
                    juce::Colour(0xff38bdf8),
                    ParamIDs::dstX,
                    ParamIDs::dstY,
                    ParamIDs::dstZ,
                    ParamIDs::dstYaw,
                    ParamIDs::dstPitch,
                    ParamIDs::dstEqLowGain,
                    ParamIDs::dstEqMidGain,
                    ParamIDs::dstEqMidFreq,
                    ParamIDs::dstEqHighGain);

    configureRotary(inputGainSlider, inputGainLabel, "Input", juce::Colour(0xffa78bfa), *this);
    inputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::inputGain, inputGainSlider);
    inputGainEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202733));
    inputGainEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    inputGainEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff374151));
    inputGainEditor.setJustification(juce::Justification::centred);
    inputGainEditor.setFont(juce::FontOptions(11.0f));
    inputGainEditor.setTooltip(
        "Input gain in dB. Values are internally limited to +/-120 dB; no limiter is applied.");
    inputGainEditor.setText(juce::String(inputGainSlider.getValue(), 1) + " dB",
                            juce::dontSendNotification);
    inputGainEditor.onReturnKey = [this]
    {
        commitGainEntry(inputGainEditor, inputGainSlider);
    };
    inputGainEditor.onFocusLost = [this]
    {
        commitGainEntry(inputGainEditor, inputGainSlider);
    };
    addAndMakeVisible(inputGainEditor);
    inputGainSlider.onValueChange = [this]
    {
        if (!inputGainEditor.hasKeyboardFocus(true))
            inputGainEditor.setText(juce::String(inputGainSlider.getValue(), 1) + " dB",
                                    juce::dontSendNotification);
    };

    configureRotary(dryWetSlider, dryWetLabel, "Dry/Wet", juce::Colour(0xffa78bfa), *this);
    dryWetSlider.setTextValueSuffix(" %");
    dryWetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::dryWet, dryWetSlider);

    configureRotary(gainSlider, gainLabel, "Output", juce::Colour(0xffa78bfa), *this);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::gain, gainSlider);
    outputGainEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff202733));
    outputGainEditor.setColour(juce::TextEditor::textColourId, juce::Colour(0xffe2e8f0));
    outputGainEditor.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff374151));
    outputGainEditor.setJustification(juce::Justification::centred);
    outputGainEditor.setFont(juce::FontOptions(11.0f));
    outputGainEditor.setTooltip(
        "Output gain in dB. Values are internally limited to +/-120 dB; no limiter is applied.");
    outputGainEditor.setText(juce::String(gainSlider.getValue(), 1) + " dB",
                             juce::dontSendNotification);
    outputGainEditor.onReturnKey = [this]
    {
        commitGainEntry(outputGainEditor, gainSlider);
    };
    outputGainEditor.onFocusLost = [this]
    {
        commitGainEntry(outputGainEditor, gainSlider);
    };
    addAndMakeVisible(outputGainEditor);
    gainSlider.onValueChange = [this]
    {
        if (!outputGainEditor.hasKeyboardFocus(true))
            outputGainEditor.setText(juce::String(gainSlider.getValue(), 1) + " dB",
                                     juce::dontSendNotification);
    };

    configureChoice(outputTypeBox, outputTypeLabel, "Output", *this);
    outputTypeBox.addItem("Vocal", 1);
    outputTypeBox.addItem("Speaker Box", 2);
    outputTypeBox.addItem("PA", 3);
    outputTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::outputType, outputTypeBox);
    configureChoice(inputTypeBox, inputTypeLabel, "Input", *this);
    inputTypeBox.addItem("Microphone", 1);
    inputTypeBox.addItem("Ears", 2);
    inputTypeBox.addItem("PA", 3);
    inputTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::inputType, inputTypeBox);
    configureChoice(microphonePatternBox, microphonePatternLabel, "Mic Pattern", *this);
    const juce::StringArray patterns {
        "Omnidirectional", "Cardioid", "Supercardioid", "Figure-Eight"
    };
    for (int i = 0; i < patterns.size(); ++i)
        microphonePatternBox.addItem(patterns[i], i + 1);
    microphonePatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::microphonePattern, microphonePatternBox);
    configureChoice(leftPatternBox, leftPatternLabel, "PA Left Mic", *this);
    for (int i = 0; i < patterns.size(); ++i)
        leftPatternBox.addItem(patterns[i], i + 1);
    leftPatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::leftMicrophonePattern, leftPatternBox);
    configureChoice(rightPatternBox, rightPatternLabel, "PA Right Mic", *this);
    for (int i = 0; i < patterns.size(); ++i)
        rightPatternBox.addItem(patterns[i], i + 1);
    rightPatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::rightMicrophonePattern, rightPatternBox);

    showRayPathsButton.setButtonText("Show Rays");
    showRayPathsButton.setColour(juce::ToggleButton::textColourId,
                                 juce::Colour(0xffcbd5e1));
    showRayPathsButton.setTooltip(
        "Show the ray paths used by the acoustic simulation in the room view.");
    addAndMakeVisible(showRayPathsButton);
    showRayPathsAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, ParamIDs::showRayPaths, showRayPathsButton);

    configureAxisSlider(rayDensitySlider, rayDensityLabel, "Ray Density",
                        juce::Colour(0xff22d3ee), *this);
    rayDensitySlider.setRange(kMinRayCount, kMaxRayCount, 1.0);
    rayDensitySlider.setNumDecimalPlacesToDisplay(0);
    rayDensitySlider.setTextValueSuffix(" rays");
    rayDensitySlider.setTooltip(
        "Number of emitted rays. Higher values improve ray-tracing quality but take longer.");
    rayDensityAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ParamIDs::rayDensity, rayDensitySlider);

    configureAxisSlider(rayBouncesSlider, rayBouncesLabel, "Max Bounces",
                        juce::Colour(0xff22d3ee), *this);
    rayBouncesSlider.setRange(0, kMaxRayBounces, 1.0);
    rayBouncesSlider.setNumDecimalPlacesToDisplay(0);
    rayBouncesSlider.setTextValueSuffix(" bounces");
    rayBouncesSlider.setTooltip(
        "Maximum number of reflections traced for each ray.");
    rayBouncesAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ParamIDs::rayBounces, rayBouncesSlider);

    inputTypeBox.onChange = [this]
    {
        updateInputMenuAvailability();
        resized();
    };
    updateInputMenuAvailability();
}

void BottomControlBar::initObjectPanel(ObjectPanel& panel,
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
                                       const char* idHigh)
{
    panel.accentColour = accent;
    panel.header.setText(title, juce::dontSendNotification);
    panel.header.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    panel.header.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(panel.header);

    panel.positionToggle.setButtonText("Position +");
    panel.positionToggle.setColour(juce::ToggleButton::textColourId,
                                  juce::Colour(0xffcbd5e1));
    panel.positionToggle.setColour(juce::ToggleButton::tickColourId, accent);
    panel.positionToggle.onClick = [this, &panel]
    {
        panel.positionExpanded = panel.positionToggle.getToggleState();
        panel.positionToggle.setButtonText(panel.positionExpanded ? "Position -" : "Position +");
        resized();
        if (onLayoutChanged)
            onLayoutChanged();
    };
    addAndMakeVisible(panel.positionToggle);

    configureAxisSlider(panel.xSlider, panel.xLabel, "X", accent, *this);
    configureAxisSlider(panel.ySlider, panel.yLabel, "Y", accent, *this);
    configureAxisSlider(panel.zSlider, panel.zLabel, "Z", accent, *this);
    panel.xSlider.setTextValueSuffix(" m");
    panel.ySlider.setTextValueSuffix(" m");
    panel.zSlider.setTextValueSuffix(" m");
    panel.xSlider.setVisible(false);
    panel.ySlider.setVisible(false);
    panel.zSlider.setVisible(false);
    panel.xLabel.setVisible(false);
    panel.yLabel.setVisible(false);
    panel.zLabel.setVisible(false);

    configureRotary(panel.yawSlider, panel.yawLabel, "Yaw", accent, *this);
    configureRotary(panel.pitchSlider, panel.pitchLabel, "Pitch", accent, *this);
    configureRotary(panel.eqLowSlider, panel.eqLowLabel, "EQ Low", accent, *this);
    configureRotary(panel.eqMidSlider, panel.eqMidLabel, "EQ Mid", accent, *this);
    configureRotary(panel.eqMidFreqSlider, panel.eqMidFreqLabel, "Mid Hz", accent, *this);
    configureRotary(panel.eqHighSlider, panel.eqHighLabel, "EQ High", accent, *this);

    panel.xAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idX, panel.xSlider);
    panel.yAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idY, panel.ySlider);
    panel.zAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idZ, panel.zSlider);
    panel.yawAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idYaw, panel.yawSlider);
    panel.pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idPitch, panel.pitchSlider);
    panel.eqLowAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idLow, panel.eqLowSlider);
    panel.eqMidAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idMid, panel.eqMidSlider);
    panel.eqMidFreqAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idMidFreq, panel.eqMidFreqSlider);
    panel.eqHighAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idHigh, panel.eqHighSlider);
}

void BottomControlBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff141820));

    g.setColour(juce::Colour(0xff262e3d));
    g.drawLine(0.0f, 0.5f, static_cast<float>(getWidth()), 0.5f, 1.0f);

    const int masterWidth = 190;
    const int sideWidth = (getWidth() - masterWidth) / 2;

    g.drawLine(static_cast<float>(sideWidth), 50.0f,
               static_cast<float>(sideWidth), static_cast<float>(getHeight() - 10), 1.0f);
    g.drawLine(static_cast<float>(sideWidth + masterWidth), 50.0f,
               static_cast<float>(sideWidth + masterWidth), static_cast<float>(getHeight() - 10), 1.0f);
}

int BottomControlBar::getPreferredHeight() const noexcept
{
    return sourcePanel.positionExpanded || listenerPanel.positionExpanded ? 304 : 210;
}

void BottomControlBar::resized()
{
    auto area = getLocalBounds().reduced(8, 8);
    auto choices = area.removeFromTop(38);
    juce::ComboBox* choiceBoxes[] = {
        &outputTypeBox, &inputTypeBox, &microphonePatternBox,
        &leftPatternBox, &rightPatternBox
    };
    juce::Label* choiceLabels[] = {
        &outputTypeLabel, &inputTypeLabel, &microphonePatternLabel,
        &leftPatternLabel, &rightPatternLabel
    };
    int visibleChoiceCount = 0;
    for (const auto* box : choiceBoxes)
        if (box->isVisible())
            ++visibleChoiceCount;

    int remainingControls = visibleChoiceCount + 3;
    const auto takeControlCell = [&choices, &remainingControls]
    {
        const int cellWidth = choices.getWidth() / remainingControls;
        --remainingControls;
        return choices.removeFromLeft(cellWidth).reduced(4, 0);
    };

    for (int i = 0; i < 5; ++i)
    {
        if (!choiceBoxes[i]->isVisible())
            continue;

        auto cell = takeControlCell();
        choiceLabels[i]->setBounds(cell.removeFromTop(14));
        choiceBoxes[i]->setBounds(cell);
    }

    showRayPathsButton.setBounds(takeControlCell());
    auto densityCell = takeControlCell();
    rayDensityLabel.setBounds(densityCell.removeFromTop(14));
    rayDensitySlider.setBounds(densityCell);
    auto bouncesCell = takeControlCell();
    rayBouncesLabel.setBounds(bouncesCell.removeFromTop(14));
    rayBouncesSlider.setBounds(bouncesCell);
    area.removeFromTop(6);

    const int masterWidth = 190;
    const int sideWidth = (area.getWidth() - masterWidth) / 2;

    layoutObjectPanel(sourcePanel, area.removeFromLeft(sideWidth).reduced(6, 0));

    auto masterArea = area.removeFromLeft(masterWidth).reduced(6, 0);
    constexpr int masterControlsHeight = 132;
    masterArea.removeFromTop(std::max(0, (masterArea.getHeight() - masterControlsHeight) / 2));
    const int masterCellWidth = masterArea.getWidth() / 3;
    auto inputCell = masterArea.removeFromLeft(masterCellWidth).reduced(1, 0);
    inputGainLabel.setBounds(inputCell.removeFromTop(16));
    inputGainEditor.setBounds(inputCell.removeFromBottom(20));
    inputGainSlider.setBounds(inputCell);

    auto dryWetCell = masterArea.removeFromLeft(masterCellWidth).reduced(1, 0);
    dryWetLabel.setBounds(dryWetCell.removeFromTop(16));
    dryWetSlider.setBounds(dryWetCell);

    auto outputCell = masterArea.reduced(1, 0);
    gainLabel.setBounds(outputCell.removeFromTop(16));
    outputGainEditor.setBounds(outputCell.removeFromBottom(20));
    gainSlider.setBounds(outputCell);

    layoutObjectPanel(listenerPanel, area.reduced(6, 0));
}

void BottomControlBar::updateInputMenuAvailability()
{
    const int inputType = inputTypeBox.getSelectedId();
    const bool showMicrophonePattern = inputType == static_cast<int>(InputType::Microphone) + 1;
    const bool showPaPatterns = inputType == static_cast<int>(InputType::PA) + 1;

    microphonePatternBox.setEnabled(showMicrophonePattern);
    microphonePatternLabel.setEnabled(showMicrophonePattern);
    microphonePatternBox.setVisible(showMicrophonePattern);
    microphonePatternLabel.setVisible(showMicrophonePattern);

    leftPatternBox.setEnabled(showPaPatterns);
    leftPatternLabel.setEnabled(showPaPatterns);
    leftPatternBox.setVisible(showPaPatterns);
    leftPatternLabel.setVisible(showPaPatterns);

    rightPatternBox.setEnabled(showPaPatterns);
    rightPatternLabel.setEnabled(showPaPatterns);
    rightPatternBox.setVisible(showPaPatterns);
    rightPatternLabel.setVisible(showPaPatterns);
}

void BottomControlBar::commitGainEntry(juce::TextEditor& editor, juce::Slider& slider)
{
    auto input = editor.getText().trim();
    if (input.endsWithIgnoreCase("dB"))
        input = input.dropLastCharacters(2).trim();

    const auto text = input.toStdString();
    char* end = nullptr;
    const double requestedDb = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || end == nullptr || *end != '\0' || std::isnan(requestedDb))
    {
        editor.setText(juce::String(slider.getValue(), 1) + " dB",
                       juce::dontSendNotification);
        editor.setTooltip("Invalid gain. Enter a numeric value in dB.");
        return;
    }

    const float boundedDb = static_cast<float>(std::clamp(requestedDb, -120.0, 120.0));
    slider.setValue(boundedDb, juce::sendNotificationSync);
    editor.setText(juce::String(boundedDb, 1) + " dB", juce::dontSendNotification);
    editor.setTooltip(
        "Gain is internally limited to +/-120 dB; no limiter is applied.");
}

void BottomControlBar::layoutObjectPanel(ObjectPanel& panel, juce::Rectangle<int> bounds)
{
    auto headerArea = bounds.removeFromTop(20);
    panel.positionToggle.setBounds(headerArea.removeFromRight(102));
    headerArea.removeFromRight(4);
    panel.header.setBounds(headerArea);
    bounds.removeFromTop(4);

    juce::Slider* axisSliders[] = { &panel.xSlider, &panel.ySlider, &panel.zSlider };
    juce::Label* axisLabels[] = { &panel.xLabel, &panel.yLabel, &panel.zLabel };
    for (int i = 0; i < 3; ++i)
    {
        axisLabels[i]->setVisible(panel.positionExpanded);
        axisSliders[i]->setVisible(panel.positionExpanded);
        if (panel.positionExpanded)
        {
            auto row = bounds.removeFromTop(30);
            axisLabels[i]->setBounds(row.removeFromLeft(22));
            axisSliders[i]->setBounds(row);
        }
    }

    if (panel.positionExpanded)
        bounds.removeFromTop(4);

    juce::Slider* sliders[] = {
        &panel.yawSlider,
        &panel.pitchSlider,
        &panel.eqLowSlider,
        &panel.eqMidSlider,
        &panel.eqMidFreqSlider,
        &panel.eqHighSlider
    };
    juce::Label* labels[] = {
        &panel.yawLabel,
        &panel.pitchLabel,
        &panel.eqLowLabel,
        &panel.eqMidLabel,
        &panel.eqMidFreqLabel,
        &panel.eqHighLabel
    };

    constexpr int count = 6;
    const int cellWidth = bounds.getWidth() / count;
    constexpr int controlsHeight = 116;
    bounds.removeFromTop(std::max(0, (bounds.getHeight() - controlsHeight) / 2));
    for (int i = 0; i < count; ++i)
    {
        auto cell = bounds.removeFromLeft(cellWidth).reduced(2, 0);
        labels[i]->setBounds(cell.removeFromTop(16));
        sliders[i]->setBounds(cell);
    }
}
} // namespace nowave
