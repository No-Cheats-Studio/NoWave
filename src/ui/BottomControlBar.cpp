#include "ui/BottomControlBar.h"
#include "parameters/ParameterLayout.h"
#include "raytracing/VulkanContext.h"
#include "ui/Palette.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace nowave
{
namespace
{
constexpr int kMinClickTarget = 24;
constexpr int kCaptionHeight = 16;
constexpr int kTopRowHeight = 60;
constexpr int kTopRowGap = 6;
constexpr int kBaseHeight = 232;
constexpr int kPositionRowsHeight = 94;
// Height reserved for one row of PA channel panels (title + 3 position rows + 2 rotary knobs).
constexpr int kPaChannelHeight = 150;

const juce::Colour& kLabelColour = palette::textLabel;
const juce::Colour& kMutedLabelColour = palette::textMuted;
const juce::Colour& kGroupColour = palette::textSubtle;
const juce::Colour& kFieldColour = palette::field;
const juce::Colour& kFieldOutlineColour = palette::fieldOutline;
const juce::Colour& kTextColour = palette::text;

void configureRotary(juce::Slider& slider,
                     juce::Label& label,
                     const juce::String& text,
                     const juce::String& tooltip,
                     juce::Colour accent,
                     juce::Component& parent)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 58, kMinClickTarget);
    slider.setColour(juce::Slider::rotarySliderFillColourId, accent);
    slider.setColour(juce::Slider::thumbColourId, accent);
    slider.setTooltip(tooltip);
    slider.setWantsKeyboardFocus(true);
    parent.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.5f));
    label.setColour(juce::Label::textColourId, kMutedLabelColour);
    label.setJustificationType(juce::Justification::centred);
    label.setTooltip(tooltip);
    parent.addAndMakeVisible(label);
}

void configureAxisSlider(juce::Slider& slider,
                         juce::Label& label,
                         const juce::String& text,
                         const juce::String& tooltip,
                         juce::Colour accent,
                         juce::Component& parent)
{
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 68, kMinClickTarget);
    slider.setColour(juce::Slider::trackColourId, accent.withAlpha(0.7f));
    slider.setColour(juce::Slider::thumbColourId, accent);
    slider.setColour(juce::Slider::textBoxTextColourId, kTextColour);
    slider.setColour(juce::Slider::textBoxBackgroundColourId, kFieldColour);
    slider.setTooltip(tooltip);
    slider.setWantsKeyboardFocus(true);
    parent.addAndMakeVisible(slider);

    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kLabelColour);
    label.setJustificationType(juce::Justification::centredLeft);
    label.setTooltip(tooltip);
    parent.addAndMakeVisible(label);
}

void configureChoice(juce::ComboBox& box,
                     juce::Label& label,
                     const juce::String& text,
                     const juce::String& tooltip,
                     juce::Component& parent)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kMutedLabelColour);
    label.setTooltip(tooltip);
    parent.addAndMakeVisible(label);

    box.setColour(juce::ComboBox::backgroundColourId, kFieldColour);
    box.setColour(juce::ComboBox::textColourId, kTextColour);
    box.setColour(juce::ComboBox::outlineColourId, kFieldOutlineColour);
    box.setTooltip(tooltip);
    box.setWantsKeyboardFocus(true);
    parent.addAndMakeVisible(box);
}

void configureGroupCaption(juce::Label& label, const juce::String& text, juce::Component& parent)
{
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    label.setColour(juce::Label::textColourId, kGroupColour);
    label.setJustificationType(juce::Justification::centredLeft);
    parent.addAndMakeVisible(label);
}

void configureGainEditor(juce::TextEditor& editor, const juce::String& tooltip, int focusOrder,
                         juce::Component& parent)
{
    editor.setColour(juce::TextEditor::backgroundColourId, kFieldColour);
    editor.setColour(juce::TextEditor::textColourId, kTextColour);
    editor.setColour(juce::TextEditor::outlineColourId, kFieldOutlineColour);
    editor.setJustification(juce::Justification::centred);
    editor.setFont(juce::FontOptions(11.0f));
    editor.setTooltip(tooltip);
    editor.setWantsKeyboardFocus(true);
    editor.setExplicitFocusOrder(focusOrder);
    parent.addAndMakeVisible(editor);
}

void placeChoice(juce::Rectangle<int>& row, int width, juce::Label& label, juce::Component& control)
{
    auto cell = row.removeFromLeft(width).reduced(4, 0);
    label.setBounds(cell.removeFromTop(kCaptionHeight));
    control.setBounds(cell);
}
} // namespace

BottomControlBar::BottomControlBar(juce::AudioProcessorValueTreeState& apvts,
                                   std::function<void()> layoutChanged,
                                   juce::String initialGpu,
                                   std::function<void(const juce::String&)> gpuSelected)
    : onLayoutChanged(std::move(layoutChanged)),
      onGpuSelected(std::move(gpuSelected))
{
    configureGroupCaption(sourceGroupLabel, "SOURCE", *this);
    configureGroupCaption(listenerGroupLabel, "LISTENER", *this);
    configureGroupCaption(raytracingGroupLabel, "RAYTRACING", *this);
    configureGroupCaption(roomGroupLabel, "ROOM / OUTPUT", *this);

    configureChoice(outputTypeBox, outputTypeLabel, "Source type",
                    "Type of sound source: Vocal (one source), Speaker Box or PA (two sources).",
                    *this);
    outputTypeBox.addItem("Vocal (single source)", 1);
    outputTypeBox.addItem("Speaker Box (two sources)", 2);
    outputTypeBox.addItem("PA (two sources)", 3);
    outputTypeBox.setExplicitFocusOrder(10);
    outputTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::outputType, outputTypeBox);

    configureChoice(inputTypeBox, inputTypeLabel, "Listener type",
                    "Type of receiver: Microphone (one receiver), Ears (two receivers) or PA (two microphones).",
                    *this);
    inputTypeBox.addItem("Microphone (single receiver)", 1);
    inputTypeBox.addItem("Ears (two receivers)", 2);
    inputTypeBox.addItem("PA (two microphones)", 3);
    inputTypeBox.setExplicitFocusOrder(11);
    inputTypeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::inputType, inputTypeBox);

    const juce::StringArray patterns {
        "Omnidirectional", "Cardioid", "Supercardioid", "Figure-Eight"
    };

    configureChoice(microphonePatternBox, microphonePatternLabel, "Mic pattern",
                    "Pickup pattern of the microphone when the listener type is Microphone.",
                    *this);
    for (int i = 0; i < patterns.size(); ++i)
        microphonePatternBox.addItem(patterns[i], i + 1);
    microphonePatternBox.setExplicitFocusOrder(12);
    microphonePatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::microphonePattern, microphonePatternBox);

    configureChoice(leftPatternBox, leftPatternLabel, "PA left mic",
                    "Pickup pattern of the left PA microphone (listener type PA).",
                    *this);
    for (int i = 0; i < patterns.size(); ++i)
        leftPatternBox.addItem(patterns[i], i + 1);
    leftPatternBox.setExplicitFocusOrder(13);
    leftPatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::leftMicrophonePattern, leftPatternBox);

    configureChoice(rightPatternBox, rightPatternLabel, "PA right mic",
                    "Pickup pattern of the right PA microphone (listener type PA).",
                    *this);
    for (int i = 0; i < patterns.size(); ++i)
        rightPatternBox.addItem(patterns[i], i + 1);
    rightPatternBox.setExplicitFocusOrder(14);
    rightPatternAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::rightMicrophonePattern, rightPatternBox);

    configureGroupCaption(soundViewGroupLabel, "SOUND VIEW", *this);

    configureChoice(soundViewBox, soundViewLabel, "Cloud",
                    "Sound cloud: colour = level in dB (0 dB = direct sound at 1 m), "
                    "arrows = direction the sound travels.",
                    *this);
    soundViewBox.addItem("Off", 1);
    soundViewBox.addItem("On", 2);
    soundViewBox.setExplicitFocusOrder(15);
    soundViewAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::soundView, soundViewBox);

    showRayPathsButton.setButtonText("Show rays");
    showRayPathsButton.setColour(juce::ToggleButton::textColourId, kLabelColour);
    showRayPathsButton.setColour(juce::ToggleButton::tickColourId, palette::receiver);
    showRayPathsButton.setTooltip(
        "Debug view: draws the simulated ray paths in the 3D room view.");
    showRayPathsButton.setWantsKeyboardFocus(true);
    showRayPathsButton.setExplicitFocusOrder(20);
    addAndMakeVisible(showRayPathsButton);
    showRayPathsAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            apvts, ParamIDs::showRayPaths, showRayPathsButton);

    configureChoice(soundViewBandBox, soundViewBandLabel, "Band",
                    "Frequency band shown in the cloud: broadband or Low / Mid / High.",
                    *this);
    soundViewBandBox.addItem("Broadband", 1);
    soundViewBandBox.addItem("Low", 2);
    soundViewBandBox.addItem("Mid", 3);
    soundViewBandBox.addItem("High", 4);
    soundViewBandBox.setExplicitFocusOrder(18);
    soundViewBandAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::soundViewBand, soundViewBandBox);

    configureChoice(soundViewArrivalBox, soundViewArrivalLabel, "Arrival",
                    "Filter by arrival time: Direct sound, Early reflections (up to 3 reflections) "
                    "or Late reverb tail.",
                    *this);
    soundViewArrivalBox.addItem("All", 1);
    soundViewArrivalBox.addItem("Direct sound", 2);
    soundViewArrivalBox.addItem("Early reflections", 3);
    soundViewArrivalBox.addItem("Late reverb", 4);
    soundViewArrivalBox.setExplicitFocusOrder(19);
    soundViewArrivalAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        apvts, ParamIDs::soundViewArrival, soundViewArrivalBox);

    configureAxisSlider(rayDensitySlider, rayDensityLabel, "Ray density (rays)",
                        "Rays emitted per simulation. Higher values are more accurate "
                        "but need more computation time.",
                        palette::receiver, *this);
    rayDensitySlider.setRange(kMinRayCount, kMaxRayCount, 1.0);
    rayDensitySlider.setNumDecimalPlacesToDisplay(0);
    rayDensitySlider.setTextValueSuffix(" rays");
    rayDensitySlider.setExplicitFocusOrder(16);
    rayDensityAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ParamIDs::rayDensity, rayDensitySlider);

    configureAxisSlider(rayBouncesSlider, rayBouncesLabel, "Max bounces (per ray)",
                        "Maximum wall reflections per ray. Higher values capture longer "
                        "reverberation but need more computation time.",
                        palette::receiver, *this);
    rayBouncesSlider.setRange(0, kMaxRayBounces, 1.0);
    rayBouncesSlider.setNumDecimalPlacesToDisplay(0);
    rayBouncesSlider.setTextValueSuffix(" bounces");
    rayBouncesSlider.setExplicitFocusOrder(17);
    rayBouncesAtt =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            apvts, ParamIDs::rayBounces, rayBouncesSlider);

    configureChoice(gpuBox, gpuLabel, "GPU",
                    "Graphics card used for the physical sound simulation. "
                    "Automatic picks the fastest available GPU.",
                    *this);
    gpuBox.addItem("Automatic", 1);
    int gpuSelection = 1;
    int gpuItemId = 2;
    for (const auto& name : VulkanContext::listComputeDevices())
    {
        gpuBox.addItem(juce::String(name), gpuItemId);
        if (initialGpu == juce::String(name))
            gpuSelection = gpuItemId;
        ++gpuItemId;
    }
    gpuBox.setSelectedId(gpuSelection, juce::dontSendNotification);
    gpuBox.setExplicitFocusOrder(21);
    gpuBox.onChange = [this]
    {
        if (onGpuSelected)
            onGpuSelected(gpuBox.getSelectedId() == 1 ? juce::String() : gpuBox.getText());
    };

    initObjectPanel(sourcePanel,
                    apvts,
                    "SOURCE (EMITTER)",
                    "source",
                    palette::source,
                    ParamIDs::srcX,
                    ParamIDs::srcY,
                    ParamIDs::srcZ,
                    ParamIDs::srcYaw,
                    ParamIDs::srcPitch,
                    ParamIDs::srcEqLowGain,
                    ParamIDs::srcEqMidGain,
                    ParamIDs::srcEqMidFreq,
                    ParamIDs::srcEqHighGain,
                    20);

    initObjectPanel(listenerPanel,
                    apvts,
                    "LISTENER (RECEIVER)",
                    "listener",
                    palette::accentSky,
                    ParamIDs::dstX,
                    ParamIDs::dstY,
                    ParamIDs::dstZ,
                    ParamIDs::dstYaw,
                    ParamIDs::dstPitch,
                    ParamIDs::dstEqLowGain,
                    ParamIDs::dstEqMidGain,
                    ParamIDs::dstEqMidFreq,
                    ParamIDs::dstEqHighGain,
                    40);

    configureRotary(inputGainSlider, inputGainLabel, "Input (dB)",
                    "Input gain in dB, applied before the room effect. Range -120 to +120 dB.",
                    palette::accentViolet, *this);
    inputGainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    inputGainSlider.setExplicitFocusOrder(30);
    inputGainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::inputGain, inputGainSlider);
    configureGainEditor(inputGainEditor,
                        "Type an exact input gain in dB and press Enter. Limited to +/-120 dB; "
                        "no limiter is applied.",
                        31, *this);
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
    inputGainSlider.onValueChange = [this]
    {
        if (!inputGainEditor.hasKeyboardFocus(true))
            inputGainEditor.setText(juce::String(inputGainSlider.getValue(), 1) + " dB",
                                    juce::dontSendNotification);
    };

    configureRotary(dryWetSlider, dryWetLabel, "Dry/Wet (%)",
                    "Balance between the original (dry) and reverberant (wet) signal. "
                    "0 % is fully dry, 100 % is fully wet.",
                    palette::accentViolet, *this);
    dryWetSlider.setTextValueSuffix(" %");
    dryWetSlider.setExplicitFocusOrder(32);
    dryWetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::dryWet, dryWetSlider);

    configureRotary(gainSlider, gainLabel, "Output (dB)",
                    "Output gain in dB, applied after dry/wet mixing. Range -120 to +120 dB.",
                    palette::accentViolet, *this);
    gainSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    gainSlider.setExplicitFocusOrder(33);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        apvts, ParamIDs::gain, gainSlider);
    configureGainEditor(outputGainEditor,
                        "Type an exact output gain in dB and press Enter. Limited to +/-120 dB; "
                        "no limiter is applied.",
                        34, *this);
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
    gainSlider.onValueChange = [this]
    {
        if (!outputGainEditor.hasKeyboardFocus(true))
            outputGainEditor.setText(juce::String(gainSlider.getValue(), 1) + " dB",
                                     juce::dontSendNotification);
    };

    initPaChannel(paSourceLeft, apvts, "PA SPEAKER LEFT", palette::source, 50,
                  ParamIDs::paSrcLeftX, ParamIDs::paSrcLeftY, ParamIDs::paSrcLeftZ,
                  ParamIDs::paSrcLeftYaw, ParamIDs::paSrcLeftPitch);
    initPaChannel(paSourceRight, apvts, "PA SPEAKER RIGHT", palette::sourceAlt, 60,
                  ParamIDs::paSrcRightX, ParamIDs::paSrcRightY, ParamIDs::paSrcRightZ,
                  ParamIDs::paSrcRightYaw, ParamIDs::paSrcRightPitch);
    initPaChannel(paListenerLeft, apvts, "PA MIC LEFT", palette::accentSky, 70,
                  ParamIDs::paDstLeftX, ParamIDs::paDstLeftY, ParamIDs::paDstLeftZ,
                  ParamIDs::paDstLeftYaw, ParamIDs::paDstLeftPitch);
    initPaChannel(paListenerRight, apvts, "PA MIC RIGHT", palette::receiverDirectionAlt, 80,
                  ParamIDs::paDstRightX, ParamIDs::paDstRightY, ParamIDs::paDstRightZ,
                  ParamIDs::paDstRightYaw, ParamIDs::paDstRightPitch);

    inputTypeBox.onChange = [this]
    {
        updateInputMenuAvailability();
        updatePaPanelsAvailability();
        resized();
        if (onLayoutChanged)
            onLayoutChanged();
    };
    outputTypeBox.onChange = [this]
    {
        updatePaPanelsAvailability();
        resized();
        if (onLayoutChanged)
            onLayoutChanged();
    };
    updateInputMenuAvailability();
    updatePaPanelsAvailability();
}

void BottomControlBar::initPaChannel(PaChannel& channel,
                                     juce::AudioProcessorValueTreeState& apvts,
                                     const juce::String& title,
                                     juce::Colour accent,
                                     int focusBase,
                                     const char* idX,
                                     const char* idY,
                                     const char* idZ,
                                     const char* idYaw,
                                     const char* idPitch)
{
    channel.title.setText(title, juce::dontSendNotification);
    channel.title.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    channel.title.setColour(juce::Label::textColourId, accent);
    addChildComponent(channel.title);

    const juce::String tip = "Placement of the " + title.toLowerCase() + " in meters. ";
    configureAxisSlider(channel.x, channel.xLabel, "X (m)", tip + "X axis.", accent, *this);
    configureAxisSlider(channel.y, channel.yLabel, "Y (m)", tip + "Y axis (height).", accent, *this);
    configureAxisSlider(channel.z, channel.zLabel, "Z (m)", tip + "Z axis.", accent, *this);
    channel.x.setTextValueSuffix(" m");
    channel.y.setTextValueSuffix(" m");
    channel.z.setTextValueSuffix(" m");
    configureRotary(channel.yaw, channel.yawLabel, "Yaw (deg)",
                    "Horizontal facing direction of the " + title.toLowerCase() + " in degrees.",
                    accent, *this);
    channel.yaw.setTextValueSuffix(" deg");
    configureRotary(channel.pitch, channel.pitchLabel, "Pitch (deg)",
                    "Vertical tilt of the " + title.toLowerCase() + " in degrees.",
                    accent, *this);
    channel.pitch.setTextValueSuffix(" deg");

    channel.x.setExplicitFocusOrder(focusBase + 1);
    channel.y.setExplicitFocusOrder(focusBase + 2);
    channel.z.setExplicitFocusOrder(focusBase + 3);
    channel.yaw.setExplicitFocusOrder(focusBase + 4);
    channel.pitch.setExplicitFocusOrder(focusBase + 5);

    channel.xAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idX, channel.x);
    channel.yAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idY, channel.y);
    channel.zAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idZ, channel.z);
    channel.yawAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idYaw, channel.yaw);
    channel.pitchAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, idPitch, channel.pitch);
}

bool BottomControlBar::showingPaSources() const
{
    return outputTypeBox.getSelectedId() == static_cast<int>(OutputType::PA) + 1;
}

bool BottomControlBar::showingPaListeners() const
{
    return inputTypeBox.getSelectedId() == static_cast<int>(InputType::PA) + 1;
}

void BottomControlBar::updatePaPanelsAvailability()
{
    const bool sources = showingPaSources();
    const bool listeners = showingPaListeners();
    for (auto* channel : { &paSourceLeft, &paSourceRight })
    {
        channel->title.setVisible(sources);
        for (auto* slider : { &channel->x, &channel->y, &channel->z, &channel->yaw, &channel->pitch })
            slider->setVisible(sources);
        for (auto* label : { &channel->xLabel, &channel->yLabel, &channel->zLabel,
                             &channel->yawLabel, &channel->pitchLabel })
            label->setVisible(sources);
    }
    for (auto* channel : { &paListenerLeft, &paListenerRight })
    {
        channel->title.setVisible(listeners);
        for (auto* slider : { &channel->x, &channel->y, &channel->z, &channel->yaw, &channel->pitch })
            slider->setVisible(listeners);
        for (auto* label : { &channel->xLabel, &channel->yLabel, &channel->zLabel,
                             &channel->yawLabel, &channel->pitchLabel })
            label->setVisible(listeners);
    }
}

void BottomControlBar::layoutPaChannel(PaChannel& channel, juce::Rectangle<int> bounds)
{
    channel.title.setBounds(bounds.removeFromTop(kCaptionHeight));
    juce::Slider* axisSliders[] = { &channel.x, &channel.y, &channel.z };
    juce::Label* axisLabels[] = { &channel.xLabel, &channel.yLabel, &channel.zLabel };
    for (int i = 0; i < 3; ++i)
    {
        auto row = bounds.removeFromTop(kMinClickTarget + 4);
        axisLabels[i]->setBounds(row.removeFromLeft(44));
        axisSliders[i]->setBounds(row);
    }
    auto rotaryArea = bounds.reduced(2, 0);
    const int cellWidth = rotaryArea.getWidth() / 2;
    auto yawCell = rotaryArea.removeFromLeft(cellWidth).reduced(2, 0);
    channel.yawLabel.setBounds(yawCell.removeFromTop(kCaptionHeight));
    channel.yaw.setBounds(yawCell);
    auto pitchCell = rotaryArea.reduced(2, 0);
    channel.pitchLabel.setBounds(pitchCell.removeFromTop(kCaptionHeight));
    channel.pitch.setBounds(pitchCell);
}

void BottomControlBar::initObjectPanel(ObjectPanel& panel,
                                       juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& title,
                                       const juce::String& roleName,
                                       juce::Colour accent,
                                       const char* idX,
                                       const char* idY,
                                       const char* idZ,
                                       const char* idYaw,
                                       const char* idPitch,
                                       const char* idLow,
                                       const char* idMid,
                                       const char* idMidFreq,
                                       const char* idHigh,
                                       int focusBase)
{
    panel.accentColour = accent;
    panel.header.setText(title, juce::dontSendNotification);
    panel.header.setFont(juce::FontOptions(12.5f, juce::Font::bold));
    panel.header.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(panel.header);

    panel.positionToggle.setButtonText("Position +");
    panel.positionToggle.setColour(juce::ToggleButton::textColourId, kLabelColour);
    panel.positionToggle.setColour(juce::ToggleButton::tickColourId, accent);
    panel.positionToggle.setTooltip("Show or hide the X, Y and Z position sliders (in meters) of the "
                                    + roleName + ".");
    panel.positionToggle.setWantsKeyboardFocus(true);
    panel.positionToggle.setExplicitFocusOrder(focusBase);
    panel.positionToggle.onClick = [this, &panel]
    {
        panel.positionExpanded = panel.positionToggle.getToggleState();
        panel.positionToggle.setButtonText(panel.positionExpanded ? "Position -" : "Position +");
        resized();
        if (onLayoutChanged)
            onLayoutChanged();
    };
    addAndMakeVisible(panel.positionToggle);

    const juce::String positionTip = "Position of the " + roleName + " in meters. ";
    configureAxisSlider(panel.xSlider, panel.xLabel, "X (m)",
                        positionTip + "X axis.", accent, *this);
    configureAxisSlider(panel.ySlider, panel.yLabel, "Y (m)",
                        positionTip + "Y axis (height).", accent, *this);
    configureAxisSlider(panel.zSlider, panel.zLabel, "Z (m)",
                        positionTip + "Z axis.", accent, *this);
    panel.xSlider.setTextValueSuffix(" m");
    panel.ySlider.setTextValueSuffix(" m");
    panel.zSlider.setTextValueSuffix(" m");
    panel.xSlider.setVisible(false);
    panel.ySlider.setVisible(false);
    panel.zSlider.setVisible(false);
    panel.xLabel.setVisible(false);
    panel.yLabel.setVisible(false);
    panel.zLabel.setVisible(false);

    configureRotary(panel.yawSlider, panel.yawLabel, "Yaw (deg)",
                    "Horizontal facing direction of the " + roleName + " in degrees.",
                    accent, *this);
    panel.yawSlider.setTextValueSuffix(" deg");

    configureRotary(panel.pitchSlider, panel.pitchLabel, "Pitch (deg)",
                    "Vertical tilt of the " + roleName + " in degrees.",
                    accent, *this);
    panel.pitchSlider.setTextValueSuffix(" deg");

    configureRotary(panel.eqLowSlider, panel.eqLowLabel, "Low (dB)",
                    "Low-shelf EQ gain of the " + roleName + " (about 200 Hz and below), in dB.",
                    accent, *this);
    panel.eqLowSlider.setTextValueSuffix(" dB");
    configureRotary(panel.eqMidSlider, panel.eqMidLabel, "Mid (dB)",
                    "Mid-band EQ gain of the " + roleName + " at the frequency set by Mid (Hz), in dB.",
                    accent, *this);
    panel.eqMidSlider.setTextValueSuffix(" dB");
    configureRotary(panel.eqMidFreqSlider, panel.eqMidFreqLabel, "Mid (Hz)",
                    "Center frequency of the mid-band EQ of the " + roleName + " (200 to 5000 Hz).",
                    accent, *this);
    panel.eqMidFreqSlider.setTextValueSuffix(" Hz");
    configureRotary(panel.eqHighSlider, panel.eqHighLabel, "High (dB)",
                    "High-shelf EQ gain of the " + roleName + " (about 8 kHz and above), in dB.",
                    accent, *this);
    panel.eqHighSlider.setTextValueSuffix(" dB");

    panel.yawSlider.setExplicitFocusOrder(focusBase + 1);
    panel.pitchSlider.setExplicitFocusOrder(focusBase + 2);
    panel.xSlider.setExplicitFocusOrder(focusBase + 3);
    panel.ySlider.setExplicitFocusOrder(focusBase + 4);
    panel.zSlider.setExplicitFocusOrder(focusBase + 5);
    panel.eqLowSlider.setExplicitFocusOrder(focusBase + 6);
    panel.eqMidSlider.setExplicitFocusOrder(focusBase + 7);
    panel.eqMidFreqSlider.setExplicitFocusOrder(focusBase + 8);
    panel.eqHighSlider.setExplicitFocusOrder(focusBase + 9);

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
    g.fillAll(palette::panel);

    g.setColour(palette::divider);
    g.drawLine(0.0f, 0.5f, static_cast<float>(getWidth()), 0.5f, 1.0f);

    const int masterWidth = 190;
    const int sideWidth = (getWidth() - masterWidth) / 2;
    const float dividerTop = static_cast<float>(8 + kTopRowHeight + kTopRowGap);
    const float dividerBottom = static_cast<float>(getHeight() - 10);

    g.drawLine(static_cast<float>(sideWidth), dividerTop,
               static_cast<float>(sideWidth), dividerBottom, 1.0f);
    g.drawLine(static_cast<float>(sideWidth + masterWidth), dividerTop,
               static_cast<float>(sideWidth + masterWidth), dividerBottom, 1.0f);
}

int BottomControlBar::getPreferredHeight() const noexcept
{
    int height = kBaseHeight;
    if (sourcePanel.positionExpanded || listenerPanel.positionExpanded)
        height += kPositionRowsHeight;
    if (showingPaSources() || showingPaListeners())
        height += kPaChannelHeight;
    return height;
}

void BottomControlBar::resized()
{
    auto area = getLocalBounds().reduced(8, 8);
    auto topRow = area.removeFromTop(kTopRowHeight);

    const int patternCells = microphonePatternBox.isVisible() ? 1
                           : (leftPatternBox.isVisible() ? 2 : 0);
    const int totalCells = 1 + (1 + patternCells) + 4 + 4;
    const int cellWidth = juce::jmax(1, topRow.getWidth() / totalCells);

    const auto placeGroup = [&topRow, cellWidth](int cells, juce::Label& caption)
    {
        auto group = topRow.removeFromLeft(cellWidth * cells);
        caption.setBounds(group.removeFromTop(kCaptionHeight).reduced(4, 0));
        return group;
    };

    auto sourceRow = placeGroup(1, sourceGroupLabel);
    placeChoice(sourceRow, cellWidth, outputTypeLabel, outputTypeBox);

    auto listenerRow = placeGroup(1 + patternCells, listenerGroupLabel);
    placeChoice(listenerRow, cellWidth, inputTypeLabel, inputTypeBox);
    if (microphonePatternBox.isVisible())
        placeChoice(listenerRow, cellWidth, microphonePatternLabel, microphonePatternBox);
    if (leftPatternBox.isVisible())
        placeChoice(listenerRow, cellWidth, leftPatternLabel, leftPatternBox);
    if (rightPatternBox.isVisible())
        placeChoice(listenerRow, cellWidth, rightPatternLabel, rightPatternBox);

    auto raytracingRow = placeGroup(4, raytracingGroupLabel);
    placeChoice(raytracingRow, cellWidth, rayDensityLabel, rayDensitySlider);
    placeChoice(raytracingRow, cellWidth, rayBouncesLabel, rayBouncesSlider);
    placeChoice(raytracingRow, cellWidth * 2, gpuLabel, gpuBox);

    auto soundViewRow = placeGroup(4, soundViewGroupLabel);
    placeChoice(soundViewRow, cellWidth, soundViewLabel, soundViewBox);
    placeChoice(soundViewRow, cellWidth, soundViewBandLabel, soundViewBandBox);
    placeChoice(soundViewRow, cellWidth, soundViewArrivalLabel, soundViewArrivalBox);
    showRayPathsButton.setBounds(soundViewRow.removeFromLeft(cellWidth).reduced(4, 0)
                                     .withTrimmedTop(kCaptionHeight));

    area.removeFromTop(kTopRowGap);

    const int masterWidth = 190;
    const int sideWidth = (area.getWidth() - masterWidth) / 2;

    auto sourceArea = area.removeFromLeft(sideWidth).reduced(6, 0);
    if (showingPaSources())
    {
        // PA sources: the two speaker channels sit in one row below the shared panel.
        auto paArea = sourceArea.removeFromBottom(kPaChannelHeight);
        const int half = paArea.getWidth() / 2;
        layoutPaChannel(paSourceLeft, paArea.removeFromLeft(half).reduced(2, 0));
        layoutPaChannel(paSourceRight, paArea.reduced(2, 0));
    }
    layoutObjectPanel(sourcePanel, sourceArea);

    auto masterArea = area.removeFromLeft(masterWidth).reduced(6, 0);
    roomGroupLabel.setBounds(masterArea.removeFromTop(kCaptionHeight));
    constexpr int masterControlsHeight = 132;
    masterArea.removeFromTop(std::max(0, (masterArea.getHeight() - masterControlsHeight) / 2));
    const int masterCellWidth = masterArea.getWidth() / 3;
    auto inputCell = masterArea.removeFromLeft(masterCellWidth).reduced(1, 0);
    inputGainLabel.setBounds(inputCell.removeFromTop(kCaptionHeight));
    inputGainEditor.setBounds(inputCell.removeFromBottom(kMinClickTarget));
    inputGainSlider.setBounds(inputCell);

    auto dryWetCell = masterArea.removeFromLeft(masterCellWidth).reduced(1, 0);
    dryWetLabel.setBounds(dryWetCell.removeFromTop(kCaptionHeight));
    dryWetSlider.setBounds(dryWetCell);

    auto outputCell = masterArea.reduced(1, 0);
    gainLabel.setBounds(outputCell.removeFromTop(kCaptionHeight));
    outputGainEditor.setBounds(outputCell.removeFromBottom(kMinClickTarget));
    gainSlider.setBounds(outputCell);

    auto listenerArea = area.reduced(6, 0);
    if (showingPaListeners())
    {
        auto paArea = listenerArea.removeFromBottom(kPaChannelHeight);
        const int half = paArea.getWidth() / 2;
        layoutPaChannel(paListenerLeft, paArea.removeFromLeft(half).reduced(2, 0));
        layoutPaChannel(paListenerRight, paArea.reduced(2, 0));
    }
    layoutObjectPanel(listenerPanel, listenerArea);
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
    auto headerArea = bounds.removeFromTop(24);
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
            axisLabels[i]->setBounds(row.removeFromLeft(44));
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
        labels[i]->setBounds(cell.removeFromTop(kCaptionHeight));
        sliders[i]->setBounds(cell);
    }
}
} // namespace nowave