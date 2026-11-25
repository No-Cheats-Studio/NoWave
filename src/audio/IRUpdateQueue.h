#pragma once

#include "audio/IRBuffer.h"

#include <array>
#include <atomic>
#include <memory>

namespace nowave
{
/**
 * Lock-free triple-buffer for IR hand-off from the raytracing/worker thread
 * to the audio thread.
 *
 * Protocol:
 *   Writer (raytracing thread):
 *     auto* buf = queue.getWriteBuffer();   // acquire a free slot
 *     // fill the 3-band source/receiver matrix and sampleLength
 *     buf->prepareConvolutionBuffers();
 *     queue.publishWriteBuffer();           // make it available to the reader
 *
 *   Reader (audio thread):
 *     if (auto* buf = queue.consumeLatestBuffer())
 *         // new IR available, start crossfade
 *
 * The middle slot's index and dirty flag are exchanged atomically, so a
 * publish racing with a read cannot lose the notification for the new IR.
 */
class IRUpdateQueue
{
public:
    static constexpr int kMaxIRSamples = 192000; // 4 s @ 48 kHz

    IRUpdateQueue()
    {
        for (auto& buf : buffers)
            buf.allocate(kMaxIRSamples);
    }

    // ---- Writer side (worker/raytracing thread) ----

    IRBuffer* getWriteBuffer() noexcept
    {
        return &buffers[static_cast<size_t>(writeSlot)];
    }

    void publishWriteBuffer() noexcept
    {
        const int oldMiddle = middleSlot.exchange(writeSlot | kDirtyFlag,
                                                  std::memory_order_acq_rel);
        writeSlot = oldMiddle & kSlotMask;
    }

    // ---- Reader side (audio thread) ----

    /**
     * Returns a pointer to the latest IR buffer if a new one was published,
     * nullptr otherwise. The returned pointer remains valid until the next
     * call to consumeLatestBuffer() or until the queue is destroyed.
     */
    IRBuffer* consumeLatestBuffer() noexcept
    {
        if ((middleSlot.load(std::memory_order_acquire) & kDirtyFlag) == 0)
            return nullptr;

        const int oldMiddle = middleSlot.exchange(readSlot, std::memory_order_acq_rel);
        readSlot = oldMiddle & kSlotMask;
        return &buffers[static_cast<size_t>(readSlot)];
    }

private:
    static constexpr int kDirtyFlag = 0x4;
    static constexpr int kSlotMask = 0x3;

    // Three slots: 0=read, 1=middle, 2=write. The middle slot has a dirty bit.
    std::array<IRBuffer, 3> buffers;

    int writeSlot { 2 };
    std::atomic<int> middleSlot { 1 };
    int readSlot { 0 };
};
} // namespace nowave

