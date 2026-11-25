#pragma once

#include "audio/ReverbEngine.h"
#include "parameters/ParameterLayout.h"
#include "raytracing/RaytracingWorker.h"
#include "scene/RoomMesh.h"

#include <atomic>
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
#include <mutex>

class NoWaveAudioProcessor final : public juce::AudioProcessor
{
public:
    NoWaveAudioProcessor();
    ~NoWaveAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& getAPVTS() noexcept { return apvts; }
    const nowave::ParameterPointers& getParameterPointers() const noexcept { return params; }

    nowave::IRUpdateQueue& getIRQueue() noexcept { return reverbEngine.getIRQueue(); }

    void setRoomMesh(std::shared_ptr<const nowave::RoomMesh> mesh,
                     const juce::String& sourceType,
                     const juce::String& identifier);
    std::shared_ptr<const nowave::RoomMesh> getRoomMesh() const;

private:
    void restoreRoomFromState();
    void saveCurrentRoomAudioSettings();
    void restoreRoomAudioSettings(const juce::String& sourceType,
                                  const juce::String& identifier);

    juce::AudioProcessorValueTreeState apvts;
    nowave::ParameterPointers params;

    nowave::ReverbEngine reverbEngine;

    mutable std::mutex roomMeshMutex;
    std::shared_ptr<const nowave::RoomMesh> activeRoomMesh;
    std::unique_ptr<nowave::RaytracingWorker> raytracingWorker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoWaveAudioProcessor)
};
