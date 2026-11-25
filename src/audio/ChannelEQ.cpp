#include "audio/ChannelEQ.h"

#ifdef _MSC_VER
#  pragma warning(push)
#  pragma warning(disable : 4244 4305) // double -> float narrowing in JUCE IIR API
#endif

namespace nowave
{
void ChannelEQ::prepare(const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;

    lowShelf.prepare(spec);
    midPeak.prepare(spec);
    highShelf.prepare(spec);

    // Set to unity by default
    setLowShelf(0.0f, 100.0f);
    setMidPeak(0.0f, 1000.0f, 1.0f);
    setHighShelf(0.0f, 8000.0f);
}

void ChannelEQ::reset()
{
    lowShelf.reset();
    midPeak.reset();
    highShelf.reset();
}

void ChannelEQ::setLowShelf(float gainDb, float freqHz)
{
    *lowShelf.state = *juce::dsp::IIR::Coefficients<float>::makeLowShelf(
        sampleRate,
        static_cast<double>(freqHz),
        0.707f,
        static_cast<float>(juce::Decibels::decibelsToGain(gainDb)));
}

void ChannelEQ::setMidPeak(float gainDb, float freqHz, float q)
{
    *midPeak.state = *juce::dsp::IIR::Coefficients<float>::makePeakFilter(
        sampleRate,
        static_cast<double>(freqHz),
        static_cast<double>(q),
        static_cast<float>(juce::Decibels::decibelsToGain(gainDb)));
}

void ChannelEQ::setHighShelf(float gainDb, float freqHz)
{
    *highShelf.state = *juce::dsp::IIR::Coefficients<float>::makeHighShelf(
        sampleRate,
        static_cast<double>(freqHz),
        0.707f,
        static_cast<float>(juce::Decibels::decibelsToGain(gainDb)));
}
} // namespace nowave

#ifdef _MSC_VER
#  pragma warning(pop)
#endif
