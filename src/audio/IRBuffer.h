#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>

namespace nowave
{
/**
 * A three-band, pre-allocated 2x2 source-to-receiver IR matrix.
 * Written exclusively by the raytracing worker thread;
 * read exclusively by the audio thread after ownership transfer
 * via IRUpdateQueue. Never allocates on the audio thread.
 */
struct IRBuffer
{
    static constexpr size_t kBandCount = 3;
    static constexpr size_t kRoutesPerBand = 4;
    static constexpr size_t kLeftFromLeft = 0;
    static constexpr size_t kLeftFromRight = 1;
    static constexpr size_t kRightFromLeft = 2;
    static constexpr size_t kRightFromRight = 3;

    static constexpr size_t channelIndex(size_t band, size_t receiver, size_t source) noexcept
    {
        return band * kRoutesPerBand + receiver * 2 + source;
    }

    std::array<std::vector<float>, kBandCount * kRoutesPerBand> channels;
    std::array<juce::AudioBuffer<float>, kBandCount * kRoutesPerBand> convolutionBuffers;
    int sampleLength { 0 };   // valid sample count in each channel
    int sourceCount { 0 };
    int receiverCount { 0 };
    bool isEmpty() const noexcept { return sampleLength <= 0; }

    void allocate(int maxLength)
    {
        for (auto& channel : channels)
            channel.assign(static_cast<size_t>(maxLength), 0.0f);
        sampleLength = 0;
        sourceCount = 0;
        receiverCount = 0;
    }

    void clear()
    {
        for (auto& channel : channels)
            std::fill(channel.begin(), channel.end(), 0.0f);
        sampleLength = 0;
        sourceCount = 0;
        receiverCount = 0;
    }

    bool prepareConvolutionBuffers()
    {
        if (isEmpty())
            return false;

        for (const auto& channel : channels)
            if (channel.size() < static_cast<size_t>(sampleLength))
                return false;

        for (size_t route = 0; route < channels.size(); ++route)
        {
            auto& buffer = convolutionBuffers[route];
            buffer.setSize(1, sampleLength, false, false, true);
            std::copy_n(channels[route].begin(), sampleLength, buffer.getWritePointer(0));
        }
        return true;
    }

    bool hasPreparedConvolutionBuffers() const noexcept
    {
        if (isEmpty())
            return false;
        for (const auto& buffer : convolutionBuffers)
            if (buffer.getNumChannels() != 1 || buffer.getNumSamples() != sampleLength)
                return false;
        return true;
    }
};
} // namespace nowave

