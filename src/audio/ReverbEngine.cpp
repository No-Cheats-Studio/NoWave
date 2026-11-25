#include "audio/ReverbEngine.h"

#include <algorithm>
#include <cmath>

namespace nowave
{
void ReverbEngine::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    sourceEQ.prepare(spec);
    listenerEQ.prepare(spec);
    convolver.prepare(spec);

    wetBuffer.setSize(static_cast<int>(spec.numChannels),
                      static_cast<int>(spec.maximumBlockSize));
}

void ReverbEngine::reset()
{
    sourceEQ.reset();
    listenerEQ.reset();
    convolver.reset();
}

void ReverbEngine::updateEQFromParams(const ParameterPointers& params) noexcept
{
    // Source EQ
    const float srcLow  = params.srcEqLowGainDb  ? params.srcEqLowGainDb->load()  : 0.0f;
    const float srcMid  = params.srcEqMidGainDb  ? params.srcEqMidGainDb->load()  : 0.0f;
    const float srcMidF = params.srcEqMidFreqHz  ? params.srcEqMidFreqHz->load()  : 1000.0f;
    const float srcHigh = params.srcEqHighGainDb ? params.srcEqHighGainDb->load() : 0.0f;

    if (srcLow != lastSrcLow || srcMidF != lastSrcMidF)
    {
        sourceEQ.setLowShelf(srcLow, 200.0f);
        lastSrcLow = srcLow;
    }
    if (srcMid != lastSrcMid || srcMidF != lastSrcMidF)
    {
        sourceEQ.setMidPeak(srcMid, srcMidF);
        lastSrcMid  = srcMid;
        lastSrcMidF = srcMidF;
    }
    if (srcHigh != lastSrcHigh)
    {
        sourceEQ.setHighShelf(srcHigh, 8000.0f);
        lastSrcHigh = srcHigh;
    }

    // Listener EQ
    const float dstLow  = params.dstEqLowGainDb  ? params.dstEqLowGainDb->load()  : 0.0f;
    const float dstMid  = params.dstEqMidGainDb  ? params.dstEqMidGainDb->load()  : 0.0f;
    const float dstMidF = params.dstEqMidFreqHz  ? params.dstEqMidFreqHz->load()  : 1000.0f;
    const float dstHigh = params.dstEqHighGainDb ? params.dstEqHighGainDb->load() : 0.0f;

    if (dstLow != lastDstLow || dstMidF != lastDstMidF)
    {
        listenerEQ.setLowShelf(dstLow, 200.0f);
        lastDstLow = dstLow;
    }
    if (dstMid != lastDstMid || dstMidF != lastDstMidF)
    {
        listenerEQ.setMidPeak(dstMid, dstMidF);
        lastDstMid  = dstMid;
        lastDstMidF = dstMidF;
    }
    if (dstHigh != lastDstHigh)
    {
        listenerEQ.setHighShelf(dstHigh, 8000.0f);
        lastDstHigh = dstHigh;
    }
}

void ReverbEngine::processBlock(juce::AudioBuffer<float>& buffer,
                                const ParameterPointers& params) noexcept
{
    // 1. Poll for a freshly-computed IR from the raytracing worker
    if (IRBuffer* newIR = irQueue.consumeLatestBuffer())
        convolver.loadIR(newIR);

    // 2. Update EQ coefficients (only when params changed)
    updateEQFromParams(params);

    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // 3. Input trim before the source EQ and convolution.
    const float inputGainDb = params.inputGainDb
                                  ? std::clamp(params.inputGainDb->load(), -120.0f, 120.0f)
                                  : 0.0f;
    const float inputGain = juce::Decibels::decibelsToGain(inputGainDb, -120.0f);
    buffer.applyGain(inputGain);

    // 4. Source EQ on the input
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        sourceEQ.process(ctx);
    }

    // 5. If convolver is loaded, produce wet signal in scratch buffer
    const float dryWetNorm = params.dryWetPercent ? (params.dryWetPercent->load() * 0.01f) : 0.5f;
    const float wetGain    = dryWetNorm;
    const float dryGain    = 1.0f - dryWetNorm;

    if (convolver.isLoaded())
    {
        // Copy dry signal to wet buffer and convolve
        wetBuffer.setSize(numChannels, numSamples, false, false, true);
        for (int ch = 0; ch < numChannels; ++ch)
            wetBuffer.copyFrom(ch, 0, buffer, ch, 0, numSamples);

        convolver.process(wetBuffer);

        // 6. Listener EQ on wet signal
        {
            juce::dsp::AudioBlock<float> block(wetBuffer);
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            listenerEQ.process(ctx);
        }

        // 7. Dry/wet blend back into main buffer
        for (int ch = 0; ch < numChannels; ++ch)
        {
            buffer.applyGain(ch, 0, numSamples, dryGain);
            buffer.addFrom(ch, 0, wetBuffer, ch, 0, numSamples, wetGain);
        }
    }

    // 8. Output trim is applied last, after dry/wet blending.
    const float outputGainDb = params.gainDb
                                   ? std::clamp(params.gainDb->load(), -120.0f, 120.0f)
                                   : 0.0f;
    const float outputGain = juce::Decibels::decibelsToGain(outputGainDb, -120.0f);
    buffer.applyGain(outputGain);
}
} // namespace nowave
