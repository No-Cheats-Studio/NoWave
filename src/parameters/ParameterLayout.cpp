#include "ParameterLayout.h"

#include <memory>
#include <vector>

namespace nowave
{
namespace
{
void addPositionParams(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                       const char* idX,
                       const char* idY,
                       const char* idZ,
                       const juce::String& prefix,
                       float defaultX,
                       float defaultY,
                       float defaultZ)
{
    const juce::NormalisableRange<float> posRange(-25.0f, 25.0f, 0.01f);
    const auto attrs = juce::AudioParameterFloatAttributes().withLabel("m");

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idX, 1}, prefix + " X", posRange, defaultX, attrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idY, 1}, prefix + " Y", posRange, defaultY, attrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idZ, 1}, prefix + " Z", posRange, defaultZ, attrs));
}

void addEqParams(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                 const char* idLow,
                 const char* idMidGain,
                 const char* idMidFreq,
                 const char* idHigh,
                 const juce::String& prefix)
{
    const juce::NormalisableRange<float> eqGainRange(-18.0f, 18.0f, 0.1f);
    const auto dbAttrs = juce::AudioParameterFloatAttributes().withLabel("dB");

    juce::NormalisableRange<float> midFreqRange(200.0f, 5000.0f, 1.0f);
    midFreqRange.setSkewForCentre(1000.0f);
    const auto hzAttrs = juce::AudioParameterFloatAttributes().withLabel("Hz");

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idLow, 1}, prefix + " EQ Low", eqGainRange, 0.0f, dbAttrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idMidGain, 1}, prefix + " EQ Mid", eqGainRange, 0.0f, dbAttrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idMidFreq, 1}, prefix + " EQ Mid Freq", midFreqRange, 1000.0f, hzAttrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idHigh, 1}, prefix + " EQ High", eqGainRange, 0.0f, dbAttrs));
}

void addRotationParams(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                       const char* idYaw,
                       const char* idPitch,
                       const juce::String& prefix,
                       float defaultYaw)
{
    const auto angleAttrs = juce::AudioParameterFloatAttributes().withLabel("deg");
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idYaw, 1}, prefix + " Yaw",
        juce::NormalisableRange<float>(-180.0f, 180.0f, 0.1f), defaultYaw, angleAttrs));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{idPitch, 1}, prefix + " Pitch",
        juce::NormalisableRange<float>(-89.0f, 89.0f, 0.1f), 0.0f, angleAttrs));
}

void addChoiceParam(std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params,
                    const char* id,
                    const juce::String& name,
                    const juce::StringArray& choices,
                    int defaultIndex)
{
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID{id, 1}, name, choices, defaultIndex));
}
} // namespace

void ParameterPointers::bind(juce::AudioProcessorValueTreeState& apvts) noexcept
{
    gainDb = apvts.getRawParameterValue(ParamIDs::gain);
    inputGainDb = apvts.getRawParameterValue(ParamIDs::inputGain);
    dryWetPercent = apvts.getRawParameterValue(ParamIDs::dryWet);
    rayDensity = apvts.getRawParameterValue(ParamIDs::rayDensity);
    rayBounces = apvts.getRawParameterValue(ParamIDs::rayBounces);
    showRayPaths = apvts.getRawParameterValue(ParamIDs::showRayPaths);
    outputType = apvts.getRawParameterValue(ParamIDs::outputType);
    inputType = apvts.getRawParameterValue(ParamIDs::inputType);
    microphonePattern = apvts.getRawParameterValue(ParamIDs::microphonePattern);
    leftMicrophonePattern = apvts.getRawParameterValue(ParamIDs::leftMicrophonePattern);
    rightMicrophonePattern = apvts.getRawParameterValue(ParamIDs::rightMicrophonePattern);

    srcX = apvts.getRawParameterValue(ParamIDs::srcX);
    srcY = apvts.getRawParameterValue(ParamIDs::srcY);
    srcZ = apvts.getRawParameterValue(ParamIDs::srcZ);
    srcYawDeg = apvts.getRawParameterValue(ParamIDs::srcYaw);
    srcPitchDeg = apvts.getRawParameterValue(ParamIDs::srcPitch);
    srcEqLowGainDb = apvts.getRawParameterValue(ParamIDs::srcEqLowGain);
    srcEqMidGainDb = apvts.getRawParameterValue(ParamIDs::srcEqMidGain);
    srcEqMidFreqHz = apvts.getRawParameterValue(ParamIDs::srcEqMidFreq);
    srcEqHighGainDb = apvts.getRawParameterValue(ParamIDs::srcEqHighGain);

    dstX = apvts.getRawParameterValue(ParamIDs::dstX);
    dstY = apvts.getRawParameterValue(ParamIDs::dstY);
    dstZ = apvts.getRawParameterValue(ParamIDs::dstZ);
    dstYawDeg = apvts.getRawParameterValue(ParamIDs::dstYaw);
    dstPitchDeg = apvts.getRawParameterValue(ParamIDs::dstPitch);
    dstEqLowGainDb = apvts.getRawParameterValue(ParamIDs::dstEqLowGain);
    dstEqMidGainDb = apvts.getRawParameterValue(ParamIDs::dstEqMidGain);
    dstEqMidFreqHz = apvts.getRawParameterValue(ParamIDs::dstEqMidFreq);
    dstEqHighGainDb = apvts.getRawParameterValue(ParamIDs::dstEqHighGain);

    paSrcX[0] = apvts.getRawParameterValue(ParamIDs::paSrcLeftX);
    paSrcY[0] = apvts.getRawParameterValue(ParamIDs::paSrcLeftY);
    paSrcZ[0] = apvts.getRawParameterValue(ParamIDs::paSrcLeftZ);
    paSrcYawDeg[0] = apvts.getRawParameterValue(ParamIDs::paSrcLeftYaw);
    paSrcPitchDeg[0] = apvts.getRawParameterValue(ParamIDs::paSrcLeftPitch);
    paSrcX[1] = apvts.getRawParameterValue(ParamIDs::paSrcRightX);
    paSrcY[1] = apvts.getRawParameterValue(ParamIDs::paSrcRightY);
    paSrcZ[1] = apvts.getRawParameterValue(ParamIDs::paSrcRightZ);
    paSrcYawDeg[1] = apvts.getRawParameterValue(ParamIDs::paSrcRightYaw);
    paSrcPitchDeg[1] = apvts.getRawParameterValue(ParamIDs::paSrcRightPitch);

    paDstX[0] = apvts.getRawParameterValue(ParamIDs::paDstLeftX);
    paDstY[0] = apvts.getRawParameterValue(ParamIDs::paDstLeftY);
    paDstZ[0] = apvts.getRawParameterValue(ParamIDs::paDstLeftZ);
    paDstYawDeg[0] = apvts.getRawParameterValue(ParamIDs::paDstLeftYaw);
    paDstPitchDeg[0] = apvts.getRawParameterValue(ParamIDs::paDstLeftPitch);
    paDstX[1] = apvts.getRawParameterValue(ParamIDs::paDstRightX);
    paDstY[1] = apvts.getRawParameterValue(ParamIDs::paDstRightY);
    paDstZ[1] = apvts.getRawParameterValue(ParamIDs::paDstRightZ);
    paDstYawDeg[1] = apvts.getRawParameterValue(ParamIDs::paDstRightYaw);
    paDstPitchDeg[1] = apvts.getRawParameterValue(ParamIDs::paDstRightPitch);
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::gain, 1},
        "Output Gain",
        juce::NormalisableRange<float>(-120.0f, 120.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::inputGain, 1},
        "Input Gain",
        juce::NormalisableRange<float>(-120.0f, 120.0f, 0.1f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel("dB")));

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{ParamIDs::dryWet, 1},
        "Dry/Wet",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f),
        50.0f,
        juce::AudioParameterFloatAttributes().withLabel("%")));

    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::rayDensity, 1},
        "Ray Density",
        kMinRayCount,
        kMaxRayCount,
        kDefaultRayCount));
    params.push_back(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{ParamIDs::rayBounces, 1},
        "Max Ray Bounces",
        0,
        kMaxRayBounces,
        kDefaultRayBounces));
    params.push_back(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{ParamIDs::showRayPaths, 1},
        "Visualize Ray Paths",
        false));

    addChoiceParam(params, ParamIDs::outputType, "Output Type",
                   { "Vocal", "Speaker Box", "PA" },
                   static_cast<int>(OutputType::Vocal));
    addChoiceParam(params, ParamIDs::inputType, "Input Type",
                   { "Microphone", "Ears", "PA" },
                   static_cast<int>(InputType::Microphone));
    const juce::StringArray microphonePatterns {
        "Omnidirectional", "Cardioid", "Supercardioid", "Figure-Eight"
    };
    addChoiceParam(params, ParamIDs::microphonePattern, "Microphone Pattern",
                   microphonePatterns, static_cast<int>(MicrophonePattern::Cardioid));
    addChoiceParam(params, ParamIDs::leftMicrophonePattern, "Left PA Microphone Pattern",
                   microphonePatterns, static_cast<int>(MicrophonePattern::Cardioid));
    addChoiceParam(params, ParamIDs::rightMicrophonePattern, "Right PA Microphone Pattern",
                   microphonePatterns, static_cast<int>(MicrophonePattern::Cardioid));

    addPositionParams(params,
                      ParamIDs::srcX,
                      ParamIDs::srcY,
                      ParamIDs::srcZ,
                      "Source",
                      -2.0f,
                      1.5f,
                      0.0f);
    addRotationParams(params, ParamIDs::srcYaw, ParamIDs::srcPitch, "Source", 90.0f);
    addEqParams(params,
                ParamIDs::srcEqLowGain,
                ParamIDs::srcEqMidGain,
                ParamIDs::srcEqMidFreq,
                ParamIDs::srcEqHighGain,
                "Source");

    addPositionParams(params,
                      ParamIDs::dstX,
                      ParamIDs::dstY,
                      ParamIDs::dstZ,
                      "Listener",
                      2.0f,
                      1.5f,
                      0.0f);
    addRotationParams(params, ParamIDs::dstYaw, ParamIDs::dstPitch, "Listener", -90.0f);
    addEqParams(params,
                ParamIDs::dstEqLowGain,
                ParamIDs::dstEqMidGain,
                ParamIDs::dstEqMidFreq,
                ParamIDs::dstEqHighGain,
                "Listener");

    // PA channel placement. Defaults mirror the centred speaker/mic pair at ±0.15 m.
    addPositionParams(params, ParamIDs::paSrcLeftX, ParamIDs::paSrcLeftY, ParamIDs::paSrcLeftZ,
                      "PA Source Left", -2.15f, 1.5f, 0.0f);
    addRotationParams(params, ParamIDs::paSrcLeftYaw, ParamIDs::paSrcLeftPitch,
                      "PA Source Left", 90.0f);
    addPositionParams(params, ParamIDs::paSrcRightX, ParamIDs::paSrcRightY, ParamIDs::paSrcRightZ,
                      "PA Source Right", -1.85f, 1.5f, 0.0f);
    addRotationParams(params, ParamIDs::paSrcRightYaw, ParamIDs::paSrcRightPitch,
                      "PA Source Right", 90.0f);

    addPositionParams(params, ParamIDs::paDstLeftX, ParamIDs::paDstLeftY, ParamIDs::paDstLeftZ,
                      "PA Listener Left", 1.85f, 1.5f, 0.0f);
    addRotationParams(params, ParamIDs::paDstLeftYaw, ParamIDs::paDstLeftPitch,
                      "PA Listener Left", -90.0f);
    addPositionParams(params, ParamIDs::paDstRightX, ParamIDs::paDstRightY, ParamIDs::paDstRightZ,
                      "PA Listener Right", 2.15f, 1.5f, 0.0f);
    addRotationParams(params, ParamIDs::paDstRightYaw, ParamIDs::paDstRightPitch,
                      "PA Listener Right", -90.0f);

    return {params.begin(), params.end()};
}
} // namespace nowave

