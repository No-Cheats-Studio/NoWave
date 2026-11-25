#pragma once

#include "audio/IRBuffer.h"
#include "audio/IRUpdateQueue.h"
#include "parameters/ParameterLayout.h"
#include "scene/RoomMesh.h"

#include <atomic>
#include <chrono>
#include <cfloat>
#include <functional>
#include <memory>
#include <thread>

namespace nowave
{
class VulkanRaytracer;

/**
 * Runs in a dedicated background thread. Monitors source/listener position
 * parameters and triggers a GPU raytrace pass whenever they change
 * significantly. Publishes new IRs to IRUpdateQueue lock-free.
 */
class RaytracingWorker
{
public:
    using MeshProvider = std::function<std::shared_ptr<const RoomMesh>()>;

    RaytracingWorker(const ParameterPointers& params,
                     IRUpdateQueue&           irQueue,
                     MeshProvider             meshProvider);

    ~RaytracingWorker();

    RaytracingWorker(const RaytracingWorker&)            = delete;
    RaytracingWorker& operator=(const RaytracingWorker&) = delete;

    void start(double sampleRate);
    void stop();

    bool isVulkanAvailable() const noexcept;

private:
    void threadFunc();
    bool simulationParametersChanged() noexcept;
    void recordSimulationParameters() noexcept;
    RaytraceParameters makeRaytraceParameters(int maxIRSamples) const noexcept;
    void synthesizeDirectPath(const RaytraceParameters& parameters,
                              IRBuffer& ir) const noexcept;

    const ParameterPointers& params;
    IRUpdateQueue&           irQueue;
    MeshProvider             meshProvider;
    double                   sampleRate { 44100.0 };

    std::thread       thread;
    std::atomic<bool> running { false };
    std::atomic<bool> vulkanAvailable { false };

    // Cached simulation inputs to detect changes
    float lastSrcX { FLT_MAX }, lastSrcY { FLT_MAX }, lastSrcZ { FLT_MAX };
    float lastDstX { FLT_MAX }, lastDstY { FLT_MAX }, lastDstZ { FLT_MAX };
    float lastSrcYaw { FLT_MAX }, lastSrcPitch { FLT_MAX };
    float lastDstYaw { FLT_MAX }, lastDstPitch { FLT_MAX };
    int lastOutputType { -1 }, lastInputType { -1 };
    int lastMicPattern { -1 }, lastLeftMicPattern { -1 }, lastRightMicPattern { -1 };
    std::shared_ptr<const RoomMesh> lastMesh;

    static constexpr float kPositionChangeDeltaM   = 0.01f;  // 1 cm threshold
    static constexpr int   kDebounceMs             = 40;     // ~25 Hz max update rate
    static constexpr float kSpeedOfSound           = 343.0f; // m/s
    static constexpr float kListenerRadius         = 0.2f;   // metres
    static constexpr float kRotationChangeDeltaDeg = 1.0f;
    static constexpr int   kNumRays                = 1024;
    static constexpr int   kMaxBounces             = 10;

    std::unique_ptr<VulkanRaytracer> raytracer;
};
} // namespace nowave

