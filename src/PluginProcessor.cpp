#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "scene/RoomPresets.h"
#include "scene/SceneFileLoader.h"

#include <array>

namespace
{
constexpr std::array<const char*, 26> kRoomAudioParameterIds {
    nowave::ParamIDs::inputGain,
    nowave::ParamIDs::inputType,
    nowave::ParamIDs::microphonePattern,
    nowave::ParamIDs::leftMicrophonePattern,
    nowave::ParamIDs::rightMicrophonePattern,
    nowave::ParamIDs::dstX,
    nowave::ParamIDs::dstY,
    nowave::ParamIDs::dstZ,
    nowave::ParamIDs::dstYaw,
    nowave::ParamIDs::dstPitch,
    nowave::ParamIDs::dstEqLowGain,
    nowave::ParamIDs::dstEqMidGain,
    nowave::ParamIDs::dstEqMidFreq,
    nowave::ParamIDs::dstEqHighGain,
    nowave::ParamIDs::gain,
    nowave::ParamIDs::outputType,
    nowave::ParamIDs::srcX,
    nowave::ParamIDs::srcY,
    nowave::ParamIDs::srcZ,
    nowave::ParamIDs::srcYaw,
    nowave::ParamIDs::srcPitch,
    nowave::ParamIDs::srcEqLowGain,
    nowave::ParamIDs::srcEqMidGain,
    nowave::ParamIDs::srcEqMidFreq,
    nowave::ParamIDs::srcEqHighGain,
    nowave::ParamIDs::dryWet
};

const juce::Identifier kRoomSettingsRoot { "RoomAudioSettings" };
const juce::Identifier kRoomSettingsChild { "Room" };
const juce::Identifier kRoomSettingsKey { "roomKey" };

juce::String makeRoomSettingsKey(const juce::String& sourceType,
                                 const juce::String& identifier)
{
    return sourceType + "|" + identifier;
}
} // namespace

NoWaveAudioProcessor::NoWaveAudioProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "Parameters", nowave::createParameterLayout())
{
    params.bind(apvts);
    raytracingWorker = std::make_unique<nowave::RaytracingWorker>(
        params, reverbEngine.getIRQueue(), [this] { return getRoomMesh(); });
    apvts.state.setProperty(nowave::StateProps::roomSourceType, "none", nullptr);
    apvts.state.setProperty(nowave::StateProps::roomIdentifier, "", nullptr);
}

const juce::String NoWaveAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool NoWaveAudioProcessor::acceptsMidi() const { return false; }
bool NoWaveAudioProcessor::producesMidi() const { return false; }
bool NoWaveAudioProcessor::isMidiEffect() const { return false; }

double NoWaveAudioProcessor::getTailLengthSeconds() const
{
    // Report tail length equal to the max IR length so the DAW doesn't cut off reverb tail.
    return static_cast<double>(nowave::IRUpdateQueue::kMaxIRSamples) / 48000.0;
}

int NoWaveAudioProcessor::getNumPrograms() { return 1; }
int NoWaveAudioProcessor::getCurrentProgram() { return 0; }
void NoWaveAudioProcessor::setCurrentProgram(int index) { juce::ignoreUnused(index); }
const juce::String NoWaveAudioProcessor::getProgramName(int index) { juce::ignoreUnused(index); return {}; }
void NoWaveAudioProcessor::changeProgramName(int index, const juce::String& newName) { juce::ignoreUnused(index, newName); }

void NoWaveAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(samplesPerBlock);
    spec.numChannels      = static_cast<juce::uint32>(getTotalNumOutputChannels());

    reverbEngine.prepare(spec);
    if (raytracingWorker != nullptr)
        raytracingWorker->start(sampleRate);
}

void NoWaveAudioProcessor::releaseResources()
{
    if (raytracingWorker != nullptr)
        raytracingWorker->stop();
    reverbEngine.reset();
}

bool NoWaveAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainOutput = layouts.getMainOutputChannelSet();
    if (mainOutput != juce::AudioChannelSet::mono() &&
        mainOutput != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet() == mainOutput;
}

void NoWaveAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    juce::ScopedNoDenormals noDenormals;

    const auto totalNumInputChannels  = getTotalNumInputChannels();
    const auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    reverbEngine.processBlock(buffer, params);
}

bool NoWaveAudioProcessor::hasEditor() const { return true; }

juce::AudioProcessorEditor* NoWaveAudioProcessor::createEditor()
{
    return new NoWaveAudioProcessorEditor(*this);
}

void NoWaveAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    saveCurrentRoomAudioSettings();
    const auto state = apvts.copyState();
    if (const auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void NoWaveAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (const auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        if (xml->hasTagName(apvts.state.getType()))
        {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            restoreRoomFromState();
        }
    }
}

void NoWaveAudioProcessor::setRoomMesh(std::shared_ptr<const nowave::RoomMesh> mesh,
                                       const juce::String& sourceType,
                                       const juce::String& identifier)
{
    saveCurrentRoomAudioSettings();
    restoreRoomAudioSettings(sourceType, identifier);

    {
        const std::lock_guard<std::mutex> lock(roomMeshMutex);
        activeRoomMesh = std::move(mesh);
    }

    apvts.state.setProperty(nowave::StateProps::roomSourceType, sourceType, nullptr);
    apvts.state.setProperty(nowave::StateProps::roomIdentifier, identifier, nullptr);
}

void NoWaveAudioProcessor::saveCurrentRoomAudioSettings()
{
    const auto sourceType = apvts.state.getProperty(nowave::StateProps::roomSourceType).toString();
    const auto identifier = apvts.state.getProperty(nowave::StateProps::roomIdentifier).toString();
    if (sourceType.isEmpty() || sourceType == "none" || identifier.isEmpty())
        return;

    auto settingsRoot = apvts.state.getChildWithName(kRoomSettingsRoot);
    if (!settingsRoot.isValid())
    {
        settingsRoot = juce::ValueTree(kRoomSettingsRoot);
        apvts.state.addChild(settingsRoot, -1, nullptr);
    }

    const auto roomKey = makeRoomSettingsKey(sourceType, identifier);
    juce::ValueTree roomSettings;
    for (int i = 0; i < settingsRoot.getNumChildren(); ++i)
    {
        const auto candidate = settingsRoot.getChild(i);
        if (candidate.getType() == kRoomSettingsChild &&
            candidate.getProperty(kRoomSettingsKey).toString() == roomKey)
        {
            roomSettings = candidate;
            break;
        }
    }

    if (!roomSettings.isValid())
    {
        roomSettings = juce::ValueTree(kRoomSettingsChild);
        roomSettings.setProperty(kRoomSettingsKey, roomKey, nullptr);
        settingsRoot.addChild(roomSettings, -1, nullptr);
    }

    for (const auto* parameterId : kRoomAudioParameterIds)
    {
        if (const auto* value = apvts.getRawParameterValue(parameterId))
            roomSettings.setProperty(juce::Identifier(parameterId),
                                    value->load(std::memory_order_relaxed),
                                    nullptr);
    }
}

void NoWaveAudioProcessor::restoreRoomAudioSettings(
    const juce::String& sourceType,
    const juce::String& identifier)
{
    const auto settingsRoot = apvts.state.getChildWithName(kRoomSettingsRoot);
    if (!settingsRoot.isValid())
        return;

    const auto roomKey = makeRoomSettingsKey(sourceType, identifier);
    for (int i = 0; i < settingsRoot.getNumChildren(); ++i)
    {
        const auto roomSettings = settingsRoot.getChild(i);
        if (roomSettings.getType() != kRoomSettingsChild ||
            roomSettings.getProperty(kRoomSettingsKey).toString() != roomKey)
            continue;

        for (const auto* parameterId : kRoomAudioParameterIds)
        {
            const juce::Identifier property(parameterId);
            auto* parameter = apvts.getParameter(parameterId);
            if (parameter != nullptr && roomSettings.hasProperty(property))
            {
                const float value = static_cast<float>(roomSettings.getProperty(property));
                parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
            }
        }
        return;
    }
}

std::shared_ptr<const nowave::RoomMesh> NoWaveAudioProcessor::getRoomMesh() const
{
    const std::lock_guard<std::mutex> lock(roomMeshMutex);
    return activeRoomMesh;
}

void NoWaveAudioProcessor::restoreRoomFromState()
{
    const auto sourceType = apvts.state.getProperty(nowave::StateProps::roomSourceType).toString();
    const auto identifier = apvts.state.getProperty(nowave::StateProps::roomIdentifier).toString();

    std::shared_ptr<const nowave::RoomMesh> restored;
    if (sourceType == "preset")
    {
        if (const auto presetId = nowave::RoomPresets::fromKey(identifier.toStdString()))
            restored = nowave::RoomPresets::createMesh(*presetId);
    }
    else if (sourceType == "file" && identifier.isNotEmpty())
    {
        const auto result = nowave::SceneFileLoader::loadFromFile(identifier.toStdString());
        if (result.success)
            restored = result.mesh;
    }

    restoreRoomAudioSettings(sourceType, identifier);
    const std::lock_guard<std::mutex> lock(roomMeshMutex);
    activeRoomMesh = std::move(restored);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NoWaveAudioProcessor();
}
