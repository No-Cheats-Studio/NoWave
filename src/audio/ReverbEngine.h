#pragma once

#include "audio/ChannelEQ.h"
#include "audio/CrossfadeConvolver.h"
#include "audio/IRUpdateQueue.h"
#include "parameters/ParameterLayout.h"

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

namespace nowave
{
/**
 * Full signal chain for one audio block:
 *
 *   Input → Source EQ → [Convolution/Raytracing IR] → Listener EQ → Dry/Wet mix → Output
 *
 * Instantiated once inside NoWaveAudioProcessor and driven from processBlock().
 * All operations on the audio thread are lock-free and allocation-free.
 */
class ReverbEngine
{
public:
    ReverbEngine() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /**
     * Called from the audio thread each block.
     * Reads all parameters via their atomic pointers.
     */
    void processBlock(juce::AudioBuffer<float>& buffer,
                      const ParameterPointers& params) noexcept;

    /**
     * Returns the write-side of the IR queue.
     * The raytracing worker calls getWriteBuffer() / publishWriteBuffer() on this.
     */
    IRUpdateQueue& getIRQueue() noexcept { return irQueue; }

private:
    void updateEQFromParams(const ParameterPointers& params) noexcept;

    ChannelEQ sourceEQ;
    ChannelEQ listenerEQ;
    CrossfadeConvolver convolver;
    IRUpdateQueue irQueue;

    juce::AudioBuffer<float> wetBuffer;
    double sampleRate { 44100.0 };

    // Coefficient update throttle: only recalculate EQ coefficients when
    // a parameter has changed, to avoid unnecessary work per block.
    float lastSrcLow { 0.0f }, lastSrcMid { 0.0f }, lastSrcMidF { 1000.0f }, lastSrcHigh { 0.0f };
    float lastDstLow { 0.0f }, lastDstMid { 0.0f }, lastDstMidF { 1000.0f }, lastDstHigh { 0.0f };
};
} // namespace nowave
