#pragma once

#include <juce_dsp/juce_dsp.h>

namespace nowave
{
/**
 * A 3-band EQ (Low Shelf, Peak Mid, High Shelf) supporting stereo channels.
 * Coefficients are updated from the message thread; actual filtering runs
 * on the audio thread and is therefore always lock-free (SmoothedValue
 * handles internal interpolation between coefficient updates atomically
 * per IIR filter cascade step).
 *
 * Usage:
 *   eq.prepare(spec);
 *   eq.setLowShelf(gainDb, freq);  // from message/timer thread
 *   eq.setMidPeak(gainDb, freq, q);
 *   eq.setHighShelf(gainDb, freq);
 *   eq.process(context);            // from audio thread
 */
class ChannelEQ
{
public:
    ChannelEQ() = default;

    void prepare(const juce::dsp::ProcessSpec& spec);
    void reset();

    void setLowShelf(float gainDb, float freqHz);
    void setMidPeak(float gainDb, float freqHz, float q = 1.0f);
    void setHighShelf(float gainDb, float freqHz);

    template <typename ProcessContext>
    void process(ProcessContext& context)
    {
        lowShelf.process(context);
        midPeak.process(context);
        highShelf.process(context);
    }

private:
    using IIRFilter = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>,
                                                      juce::dsp::IIR::Coefficients<float>>;

    IIRFilter lowShelf;
    IIRFilter midPeak;
    IIRFilter highShelf;

    double sampleRate { 44100.0 };
};
} // namespace nowave
