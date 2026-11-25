#pragma once

#include "audio/IRBuffer.h"

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <memory>

namespace nowave
{
/**
 * Uniform Partitioned Convolution with equal-power crossfade between IRs.
 *
 * Two independent convolution engines (A and B) run in parallel.
 * When a new IR is loaded, the inactive engine fades in over kCrossfadeSamples
 * while the active engine fades out, then the roles are swapped.
 *
 * The worker thread prepares IR buffers before publication; audio processing
 * does not allocate while handing those buffers to JUCE's convolution engines.
 */
class CrossfadeConvolver
{
public:
    static constexpr int kCrossfadeMs = 25; // crossfade duration in milliseconds

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    /**
     * Load a worker-prepared IR. Safe to call from the audio thread.
     * If irBuffer is nullptr or empty, the convolver is bypassed (dry pass-through).
     */
    void loadIR(IRBuffer* irBuffer) noexcept;

    /**
     * Process one stereo block in-place.
     * Input channel 0 = left, channel 1 = right.
     */
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    bool isLoaded() const noexcept { return engineALoaded || engineBLoaded; }

private:
    struct ConvEngine
    {
        struct BandSplitter
        {
            juce::dsp::LinkwitzRileyFilter<float> lowCrossover;
            juce::dsp::LinkwitzRileyFilter<float> highCrossover;
        };

        std::array<std::unique_ptr<juce::dsp::Convolution>,
                   IRBuffer::kBandCount * IRBuffer::kRoutesPerBand>
            convolutions;
        std::array<BandSplitter, 2> bandSplitters;
        std::array<juce::AudioBuffer<float>, IRBuffer::kBandCount> bandScratch;
        juce::AudioBuffer<float> monoScratch;
        juce::AudioBuffer<float> accumulator;
        int sourceCount { 0 };
        int receiverCount { 0 };
        bool loaded { false };
    };

    bool loadIntoEngine(ConvEngine& engine, IRBuffer& ir) noexcept;
    void renderEngine(ConvEngine& engine,
                      const juce::AudioBuffer<float>& input,
                      int numSamples) noexcept;
    void copyEngineOutput(const ConvEngine& engine,
                          juce::AudioBuffer<float>& output,
                          int numChannels,
                          int numSamples) noexcept;

    juce::dsp::ConvolutionMessageQueue convolutionMessageQueue { 64 };
    ConvEngine engineA;
    ConvEngine engineB;

    // which engine is currently the dominant/fading-in one
    bool useEngineA { true };

    bool engineALoaded { false };
    bool engineBLoaded { false };

    int crossfadeSamplesTotal { 0 };
    int crossfadeSamplesRemaining { 0 };
    bool isCrossfading { false };

    double sampleRate { 44100.0 };
    int blockSize { 512 };

};
} // namespace nowave
