#include "audio/CrossfadeConvolver.h"

#include <algorithm>
#include <cmath>

namespace nowave
{
void CrossfadeConvolver::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    blockSize = static_cast<int>(spec.maximumBlockSize);

    crossfadeSamplesTotal = static_cast<int>(
        std::round(static_cast<double>(kCrossfadeMs) * 0.001 * sampleRate));
    crossfadeSamplesTotal = std::max(crossfadeSamplesTotal, 1);

    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    const auto prepareEngine = [&](ConvEngine& engine)
    {
        for (auto& convolution : engine.convolutions)
        {
            if (convolution == nullptr)
                convolution = std::make_unique<juce::dsp::Convolution>(
                    convolutionMessageQueue);
            convolution->prepare(monoSpec);
            convolution->reset();
        }
        for (auto& splitter : engine.bandSplitters)
        {
            splitter.lowCrossover.prepare(monoSpec);
            splitter.lowCrossover.setCutoffFrequency(
                static_cast<float>(std::min(250.0, spec.sampleRate * 0.15)));
            splitter.highCrossover.prepare(monoSpec);
            splitter.highCrossover.setCutoffFrequency(
                static_cast<float>(std::min(4000.0, spec.sampleRate * 0.40)));
            splitter.lowCrossover.reset();
            splitter.highCrossover.reset();
        }
        for (auto& scratch : engine.bandScratch)
            scratch.setSize(1, blockSize);
        engine.monoScratch.setSize(1, blockSize);
        engine.accumulator.setSize(2, blockSize);
        engine.sourceCount = 0;
        engine.receiverCount = 0;
        engine.loaded = false;
    };

    prepareEngine(engineA);
    prepareEngine(engineB);
    engineALoaded = false;
    engineBLoaded = false;
    isCrossfading = false;
    crossfadeSamplesRemaining = 0;
    useEngineA = true;
}

void CrossfadeConvolver::reset()
{
    for (auto& convolution : engineA.convolutions)
        if (convolution != nullptr)
            convolution->reset();
    for (auto& convolution : engineB.convolutions)
        if (convolution != nullptr)
            convolution->reset();
    for (auto& splitter : engineA.bandSplitters)
    {
        splitter.lowCrossover.reset();
        splitter.highCrossover.reset();
    }
    for (auto& splitter : engineB.bandSplitters)
    {
        splitter.lowCrossover.reset();
        splitter.highCrossover.reset();
    }
    isCrossfading = false;
    crossfadeSamplesRemaining = 0;
}

bool CrossfadeConvolver::loadIntoEngine(ConvEngine& engine, IRBuffer& ir) noexcept
{
    jassert(ir.hasPreparedConvolutionBuffers());
    if (!ir.hasPreparedConvolutionBuffers())
        return false;

    for (size_t route = 0; route < ir.channels.size(); ++route)
    {
        if (engine.convolutions[route] == nullptr)
            return false;

        engine.convolutions[route]->loadImpulseResponse(
            std::move(ir.convolutionBuffers[route]),
            sampleRate,
            juce::dsp::Convolution::Stereo::no,
            juce::dsp::Convolution::Trim::no,
            juce::dsp::Convolution::Normalise::no);
    }
    engine.sourceCount = std::clamp(ir.sourceCount, 1, 2);
    engine.receiverCount = std::clamp(ir.receiverCount, 1, 2);
    engine.loaded = true;
    return true;
}

void CrossfadeConvolver::loadIR(IRBuffer* irBuffer) noexcept
{
    if (irBuffer == nullptr || irBuffer->isEmpty())
    {
        isCrossfading = true;
        crossfadeSamplesRemaining = crossfadeSamplesTotal;
        engineALoaded = false;
        engineBLoaded = false;
        engineA.loaded = false;
        engineB.loaded = false;
        engineA.sourceCount = engineA.receiverCount = 0;
        engineB.sourceCount = engineB.receiverCount = 0;
        return;
    }

    if (useEngineA)
    {
        if (!loadIntoEngine(engineB, *irBuffer))
            return;
        engineBLoaded = true;
    }
    else
    {
        if (!loadIntoEngine(engineA, *irBuffer))
            return;
        engineALoaded = true;
    }

    isCrossfading = true;
    crossfadeSamplesRemaining = crossfadeSamplesTotal;
    useEngineA = !useEngineA;
}

void CrossfadeConvolver::renderEngine(ConvEngine& engine,
                                      const juce::AudioBuffer<float>& input,
                                      int numSamples) noexcept
{
    engine.accumulator.clear(0, 0, numSamples);
    engine.accumulator.clear(1, 0, numSamples);
    if (!engine.loaded)
        return;

    const int inputChannels = std::max(input.getNumChannels(), 1);
    for (int source = 0; source < 2; ++source)
    {
        const bool sourceActive = source < engine.sourceCount;
        if (sourceActive && engine.sourceCount == 1 && inputChannels > 1)
        {
            engine.monoScratch.copyFrom(0, 0, input, 0, 0, numSamples);
            engine.monoScratch.addFrom(0, 0, input, 1, 0, numSamples);
            engine.monoScratch.applyGain(0, numSamples, 0.5f);
        }
        else if (sourceActive)
        {
            const int inputChannel = engine.sourceCount == 1 ? 0
                                     : std::min(source, inputChannels - 1);
            engine.monoScratch.copyFrom(0, 0, input, inputChannel, 0, numSamples);
        }
        else
        {
            engine.monoScratch.clear();
        }

        const auto* sourceSamples = engine.monoScratch.getReadPointer(0);
        auto* lowSamples = engine.bandScratch[0].getWritePointer(0);
        auto* midSamples = engine.bandScratch[1].getWritePointer(0);
        auto* highSamples = engine.bandScratch[2].getWritePointer(0);
        auto& splitter = engine.bandSplitters[static_cast<size_t>(source)];
        for (int sample = 0; sample < numSamples; ++sample)
        {
            float low = 0.0f;
            float upper = 0.0f;
            float mid = 0.0f;
            float high = 0.0f;
            splitter.lowCrossover.processSample(0, sourceSamples[sample], low, upper);
            splitter.highCrossover.processSample(0, upper, mid, high);
            lowSamples[sample] = low;
            midSamples[sample] = mid;
            highSamples[sample] = high;
        }

        if (!sourceActive)
            continue;

        for (size_t band = 0; band < IRBuffer::kBandCount; ++band)
        {
            engine.monoScratch.copyFrom(0, 0, engine.bandScratch[band], 0, 0, numSamples);
            for (int receiver = 0; receiver < engine.receiverCount; ++receiver)
            {
                const size_t route = IRBuffer::channelIndex(
                    band, static_cast<size_t>(receiver), static_cast<size_t>(source));
                juce::dsp::AudioBlock<float> block(engine.monoScratch);
                juce::dsp::ProcessContextReplacing<float> context(block);
                engine.convolutions[route]->process(context);
                engine.accumulator.addFrom(receiver, 0, engine.monoScratch, 0, 0, numSamples);
            }
        }
    }

    if (engine.receiverCount == 1)
        engine.accumulator.copyFrom(1, 0, engine.accumulator, 0, 0, numSamples);
}

void CrossfadeConvolver::copyEngineOutput(const ConvEngine& engine,
                                          juce::AudioBuffer<float>& output,
                                          int numChannels,
                                          int numSamples) noexcept
{
    if (numChannels == 1)
    {
        const auto* left = engine.accumulator.getReadPointer(0);
        const auto* right = engine.accumulator.getReadPointer(1);
        auto* mono = output.getWritePointer(0);
        for (int i = 0; i < numSamples; ++i)
            mono[i] = 0.5f * (left[i] + right[i]);
        return;
    }

    for (int channel = 0; channel < 2; ++channel)
        output.copyFrom(channel, 0, engine.accumulator, channel, 0, numSamples);
}

void CrossfadeConvolver::process(juce::AudioBuffer<float>& buffer) noexcept
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = std::min(buffer.getNumChannels(), 2);
    if (numChannels == 0 || (!engineALoaded && !engineBLoaded))
        return;

    if (!isCrossfading)
    {
        ConvEngine& active = useEngineA ? engineA : engineB;
        if (!active.loaded)
            return;
        renderEngine(active, buffer, numSamples);
        copyEngineOutput(active, buffer, numChannels, numSamples);
        return;
    }

    renderEngine(engineA, buffer, numSamples);
    renderEngine(engineB, buffer, numSamples);

    const ConvEngine& incoming = useEngineA ? engineA : engineB;
    const ConvEngine& outgoing = useEngineA ? engineB : engineA;
    const float fadeStep = 1.0f / static_cast<float>(crossfadeSamplesTotal);
    float gainIncoming = 1.0f -
        (static_cast<float>(crossfadeSamplesRemaining) / static_cast<float>(crossfadeSamplesTotal));
    auto* outLeft = buffer.getWritePointer(0);
    auto* outRight = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    const auto* inLeft = incoming.accumulator.getReadPointer(0);
    const auto* outLeftSource = outgoing.accumulator.getReadPointer(0);
    const auto* inRight = incoming.accumulator.getReadPointer(1);
    const auto* outRightSource = outgoing.accumulator.getReadPointer(1);
    for (int sample = 0; sample < numSamples; ++sample)
    {
        const float amount = std::clamp(gainIncoming, 0.0f, 1.0f);
        const float gainIn = std::sin(amount * juce::MathConstants<float>::halfPi);
        const float gainOut = std::sin((1.0f - amount) * juce::MathConstants<float>::halfPi);
        if (numChannels == 1)
        {
            const float inMono = 0.5f * (inLeft[sample] + inRight[sample]);
            const float outMono = 0.5f * (outLeftSource[sample] + outRightSource[sample]);
            outLeft[sample] = inMono * gainIn + outMono * gainOut;
        }
        else
        {
            outLeft[sample] = inLeft[sample] * gainIn + outLeftSource[sample] * gainOut;
            outRight[sample] = inRight[sample] * gainIn + outRightSource[sample] * gainOut;
        }
        gainIncoming += fadeStep;
    }

    crossfadeSamplesRemaining -= numSamples;
    if (crossfadeSamplesRemaining <= 0)
    {
        isCrossfading = false;
        crossfadeSamplesRemaining = 0;
        if (useEngineA)
        {
            engineBLoaded = false;
            engineB.loaded = false;
        }
        else
        {
            engineALoaded = false;
            engineA.loaded = false;
        }
    }
}
} // namespace nowave
