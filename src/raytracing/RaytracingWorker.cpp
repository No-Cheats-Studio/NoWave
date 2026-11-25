#include "raytracing/RaytracingWorker.h"

#include "raytracing/VulkanRaytracer.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <juce_core/juce_core.h>

namespace nowave
{
namespace
{
float parameterValue(const std::atomic<float>* parameter, float fallback) noexcept
{
    return parameter != nullptr ? parameter->load(std::memory_order_relaxed) : fallback;
}

int rayDensityValue(const ParameterPointers& parameters) noexcept
{
    const int value = static_cast<int>(std::round(
        parameterValue(parameters.rayDensity, static_cast<float>(kDefaultRayCount))));
    return std::clamp(value, kMinRayCount, kMaxRayCount);
}

int rayBouncesValue(const ParameterPointers& parameters) noexcept
{
    const int value = static_cast<int>(std::round(
        parameterValue(parameters.rayBounces,
                       static_cast<float>(kDefaultRayBounces))));
    return std::clamp(value, 0, kMaxRayBounces);
}

bool rayPathsEnabled(const ParameterPointers& parameters) noexcept
{
    return parameterValue(parameters.showRayPaths, 0.0f) >= 0.5f;
}

int choiceValue(const std::atomic<float>* parameter, int fallback, int maximum) noexcept
{
    const int value = static_cast<int>(std::round(parameterValue(parameter,
                                                                 static_cast<float>(fallback))));
    return std::clamp(value, 0, maximum);
}

void setVector(float (&destination)[4], const glm::vec3& value, float w) noexcept
{
    destination[0] = value.x;
    destination[1] = value.y;
    destination[2] = value.z;
    destination[3] = w;
}
} // namespace

RaytracingWorker::RaytracingWorker(const ParameterPointers& parameterPointers,
                                   IRUpdateQueue& updateQueue,
                                   MeshProvider roomProvider)
    : params(parameterPointers),
      irQueue(updateQueue),
      meshProvider(std::move(roomProvider))
{
}

RaytracingWorker::~RaytracingWorker()
{
    stop();
}

void RaytracingWorker::start(double rate)
{
    stop();
    sampleRate = rate > 0.0 ? rate : 44100.0;
    lastMesh.reset();
    publishRayPaths({});
    vulkanAvailable.store(false, std::memory_order_release);
    status.store(SimulationStatus::NoGpu, std::memory_order_release);
    running.store(true, std::memory_order_release);
    thread = std::thread(&RaytracingWorker::threadFunc, this);
}

void RaytracingWorker::stop()
{
    running.store(false, std::memory_order_release);
    if (thread.joinable())
        thread.join();
}

SimulationStatus RaytracingWorker::getStatus() const noexcept
{
    return status.load(std::memory_order_acquire);
}

bool RaytracingWorker::isVulkanAvailable() const noexcept
{
    return vulkanAvailable.load(std::memory_order_acquire);
}

std::shared_ptr<const std::vector<RaySegment>>
RaytracingWorker::getRayPathsSnapshot() const
{
    const std::lock_guard<std::mutex> lock(rayPathsMutex);
    return rayPathsSnapshot;
}

void RaytracingWorker::publishRayPaths(std::vector<RaySegment> paths)
{
    auto snapshot =
        std::make_shared<const std::vector<RaySegment>>(std::move(paths));
    const std::lock_guard<std::mutex> lock(rayPathsMutex);
    rayPathsSnapshot = std::move(snapshot);
}

bool RaytracingWorker::simulationParametersChanged() noexcept
{
    const float srcX = parameterValue(params.srcX, -2.0f);
    const float srcY = parameterValue(params.srcY, 1.5f);
    const float srcZ = parameterValue(params.srcZ, 0.0f);
    const float dstX = parameterValue(params.dstX, 2.0f);
    const float dstY = parameterValue(params.dstY, 1.5f);
    const float dstZ = parameterValue(params.dstZ, 0.0f);
    const float srcYaw = parameterValue(params.srcYawDeg, 90.0f);
    const float srcPitch = parameterValue(params.srcPitchDeg, 0.0f);
    const float dstYaw = parameterValue(params.dstYawDeg, -90.0f);
    const float dstPitch = parameterValue(params.dstPitchDeg, 0.0f);

    return std::abs(srcX - lastSrcX) >= kPositionChangeDeltaM ||
           std::abs(srcY - lastSrcY) >= kPositionChangeDeltaM ||
           std::abs(srcZ - lastSrcZ) >= kPositionChangeDeltaM ||
           std::abs(dstX - lastDstX) >= kPositionChangeDeltaM ||
           std::abs(dstY - lastDstY) >= kPositionChangeDeltaM ||
           std::abs(dstZ - lastDstZ) >= kPositionChangeDeltaM ||
           std::abs(srcYaw - lastSrcYaw) >= kRotationChangeDeltaDeg ||
           std::abs(srcPitch - lastSrcPitch) >= kRotationChangeDeltaDeg ||
           std::abs(dstYaw - lastDstYaw) >= kRotationChangeDeltaDeg ||
           std::abs(dstPitch - lastDstPitch) >= kRotationChangeDeltaDeg ||
           choiceValue(params.outputType, static_cast<int>(OutputType::Vocal), 2) != lastOutputType ||
           choiceValue(params.inputType, static_cast<int>(InputType::Microphone), 2) != lastInputType ||
           choiceValue(params.microphonePattern,
                       static_cast<int>(MicrophonePattern::Cardioid), 3) != lastMicPattern ||
           choiceValue(params.leftMicrophonePattern,
                       static_cast<int>(MicrophonePattern::Cardioid), 3) != lastLeftMicPattern ||
           choiceValue(params.rightMicrophonePattern,
                       static_cast<int>(MicrophonePattern::Cardioid), 3) != lastRightMicPattern ||
           rayDensityValue(params) != lastRayDensity ||
           rayBouncesValue(params) != lastRayBounces ||
           rayPathsEnabled(params) != lastShowRayPaths ||
           paChannelsChanged();
}

bool RaytracingWorker::paChannelsChanged() const noexcept
{
    const auto current = paChannelValues();
    for (size_t i = 0; i < current.size(); ++i)
    {
        const float threshold = i % 5 < 3 ? kPositionChangeDeltaM : kRotationChangeDeltaDeg;
        if (std::abs(current[i] - lastPaChannelValues[i]) >= threshold)
            return true;
    }
    return false;
}

std::array<float, 20> RaytracingWorker::paChannelValues() const noexcept
{
    std::array<float, 20> values {};
    for (int channel = 0; channel < 2; ++channel)
    {
        const size_t base = static_cast<size_t>(channel) * 5;
        values[base + 0] = parameterValue(params.paSrcX[channel], 0.0f);
        values[base + 1] = parameterValue(params.paSrcY[channel], 1.5f);
        values[base + 2] = parameterValue(params.paSrcZ[channel], 0.0f);
        values[base + 3] = parameterValue(params.paSrcYawDeg[channel], 90.0f);
        values[base + 4] = parameterValue(params.paSrcPitchDeg[channel], 0.0f);

        const size_t dst = 10 + base;
        values[dst + 0] = parameterValue(params.paDstX[channel], 2.0f);
        values[dst + 1] = parameterValue(params.paDstY[channel], 1.5f);
        values[dst + 2] = parameterValue(params.paDstZ[channel], 0.0f);
        values[dst + 3] = parameterValue(params.paDstYawDeg[channel], -90.0f);
        values[dst + 4] = parameterValue(params.paDstPitchDeg[channel], 0.0f);
    }
    return values;
}

void RaytracingWorker::recordSimulationParameters() noexcept
{
    lastSrcX = parameterValue(params.srcX, -2.0f);
    lastSrcY = parameterValue(params.srcY, 1.5f);
    lastSrcZ = parameterValue(params.srcZ, 0.0f);
    lastDstX = parameterValue(params.dstX, 2.0f);
    lastDstY = parameterValue(params.dstY, 1.5f);
    lastDstZ = parameterValue(params.dstZ, 0.0f);
    lastSrcYaw = parameterValue(params.srcYawDeg, 90.0f);
    lastSrcPitch = parameterValue(params.srcPitchDeg, 0.0f);
    lastDstYaw = parameterValue(params.dstYawDeg, -90.0f);
    lastDstPitch = parameterValue(params.dstPitchDeg, 0.0f);
    lastOutputType = choiceValue(params.outputType, static_cast<int>(OutputType::Vocal), 2);
    lastInputType = choiceValue(params.inputType, static_cast<int>(InputType::Microphone), 2);
    lastMicPattern = choiceValue(params.microphonePattern,
                                 static_cast<int>(MicrophonePattern::Cardioid), 3);
    lastLeftMicPattern = choiceValue(params.leftMicrophonePattern,
                                     static_cast<int>(MicrophonePattern::Cardioid), 3);
    lastRightMicPattern = choiceValue(params.rightMicrophonePattern,
                                      static_cast<int>(MicrophonePattern::Cardioid), 3);
    lastRayDensity = rayDensityValue(params);
    lastRayBounces = rayBouncesValue(params);
    lastShowRayPaths = rayPathsEnabled(params);
    lastPaChannelValues = paChannelValues();
}

RaytraceParameters RaytracingWorker::makeRaytraceParameters(int maxIRSamples) const noexcept
{
    RaytraceParameters result {};
    const auto outputType = static_cast<OutputType>(
        choiceValue(params.outputType, static_cast<int>(OutputType::Vocal), 2));
    const auto inputType = static_cast<InputType>(
        choiceValue(params.inputType, static_cast<int>(InputType::Microphone), 2));
    const auto microphonePattern = static_cast<MicrophonePattern>(
        choiceValue(params.microphonePattern,
                    static_cast<int>(MicrophonePattern::Cardioid), 3));
    const auto leftPattern = static_cast<MicrophonePattern>(
        choiceValue(params.leftMicrophonePattern,
                    static_cast<int>(MicrophonePattern::Cardioid), 3));
    const auto rightPattern = static_cast<MicrophonePattern>(
        choiceValue(params.rightMicrophonePattern,
                    static_cast<int>(MicrophonePattern::Cardioid), 3));

    const glm::vec3 sourceCenter(parameterValue(params.srcX, -2.0f),
                                 parameterValue(params.srcY, 1.5f),
                                 parameterValue(params.srcZ, 0.0f));
    const glm::vec3 receiverCenter(parameterValue(params.dstX, 2.0f),
                                   parameterValue(params.dstY, 1.5f),
                                   parameterValue(params.dstZ, 0.0f));
    const float sourceYaw = parameterValue(params.srcYawDeg, 90.0f);
    const float sourcePitch = parameterValue(params.srcPitchDeg, 0.0f);
    const float receiverYaw = parameterValue(params.dstYawDeg, -90.0f);
    const float receiverPitch = parameterValue(params.dstPitchDeg, 0.0f);
    const glm::vec3 sourceRight = rightFromYaw(sourceYaw);
    const glm::vec3 receiverRight = rightFromYaw(receiverYaw);

    const int sourceCount = outputType == OutputType::Vocal ? 1 : 2;
    const int receiverCount = inputType == InputType::Microphone ? 1 : 2;
    result.configuration[0] = sourceCount;
    result.configuration[1] = receiverCount;
    result.configuration[2] = static_cast<int>(outputType);
    result.configuration[3] = static_cast<int>(inputType);
    result.patterns[0] = static_cast<int>(MicrophonePattern::Cardioid);

    for (int source = 0; source < sourceCount; ++source)
    {
        const bool left = source == 0;
        const float side = left ? -1.0f : 1.0f;
        if (outputType == OutputType::PA)
        {
            // PA: each speaker has its own placement and orientation.
            const glm::vec3 position(parameterValue(params.paSrcX[source], 0.0f),
                                     parameterValue(params.paSrcY[source], 1.5f),
                                     parameterValue(params.paSrcZ[source], 0.0f));
            const float yaw = parameterValue(params.paSrcYawDeg[source], 90.0f);
            const float pitch = parameterValue(params.paSrcPitchDeg[source], 0.0f);
            setVector(result.sourcePositions[source], position, 1.0f);
            setVector(result.sourceDirections[source], directionFromYawPitch(yaw, pitch), 0.0f);
            continue;
        }

        float offset = 0.0f;
        float yawOffset = 0.0f;
        if (outputType == OutputType::SpeakerBox)
        {
            offset = 0.06f;
            yawOffset = side * 30.0f;
        }
        setVector(result.sourcePositions[source],
                  sourceCenter + sourceRight * (side * offset), 1.0f);
        setVector(result.sourceDirections[source],
                  directionFromYawPitch(sourceYaw + yawOffset, sourcePitch), 0.0f);
    }

    for (int receiver = 0; receiver < receiverCount; ++receiver)
    {
        const bool left = receiver == 0;
        const float side = left ? -1.0f : 1.0f;
        if (inputType == InputType::PA)
        {
            // PA: each microphone has its own placement and orientation.
            const glm::vec3 position(parameterValue(params.paDstX[receiver], 2.0f),
                                     parameterValue(params.paDstY[receiver], 1.5f),
                                     parameterValue(params.paDstZ[receiver], 0.0f));
            const float yaw = parameterValue(params.paDstYawDeg[receiver], -90.0f);
            const float pitch = parameterValue(params.paDstPitchDeg[receiver], 0.0f);
            setVector(result.receiverPositions[receiver], position, 1.0f);
            setVector(result.receiverDirections[receiver], directionFromYawPitch(yaw, pitch), 0.0f);
            result.patterns[receiver + 1] = static_cast<int>(left ? leftPattern : rightPattern);
            continue;
        }

        float offset = 0.0f;
        float yawOffset = 0.0f;
        if (inputType == InputType::Ears)
        {
            offset = 0.09f;
            yawOffset = side * 10.0f;
        }
        setVector(result.receiverPositions[receiver],
                  receiverCenter + receiverRight * (side * offset), 1.0f);
        setVector(result.receiverDirections[receiver],
                  directionFromYawPitch(receiverYaw + yawOffset, receiverPitch), 0.0f);
        result.patterns[receiver + 1] = static_cast<int>(
            inputType == InputType::Microphone ? microphonePattern
                                               : MicrophonePattern::Cardioid);
    }

    result.physics[0] = kListenerRadius;
    result.physics[1] = kSpeedOfSound;
    result.physics[2] = static_cast<float>(sampleRate);
    result.physics[3] = rayPathsEnabled(params) ? 1.0f : 0.0f;
    result.limits[0] = maxIRSamples;
    result.limits[1] = kMaxRayTaps;
    result.limits[2] = rayDensityValue(params);
    result.limits[3] = rayBouncesValue(params);
    return result;
}

void RaytracingWorker::threadFunc()
{
    raytracer = std::make_unique<VulkanRaytracer>();
    const bool gpuReady = raytracer->initialize();
    vulkanAvailable.store(gpuReady, std::memory_order_release);
    if (!gpuReady)
    {
        status.store(SimulationStatus::NoGpu, std::memory_order_release);
        juce::Logger::writeToLog("NoWave: Vulkan raytracing unavailable; no impulse response will be produced. " +
                                 juce::String(raytracer->getLastError()));
    }

    bool hasPublishedIR = false;
    bool sceneReady = false;
    while (running.load(std::memory_order_acquire))
    {
        const auto mesh = meshProvider ? meshProvider() : nullptr;
        const bool meshChanged = mesh != lastMesh;
        const bool parametersChanged = simulationParametersChanged();
        if (meshChanged || parametersChanged)
        {
            publishRayPaths({});
            const bool meshValid = mesh != nullptr && mesh->isValid();
            auto newStatus = !gpuReady ? SimulationStatus::NoGpu
                                       : (meshValid ? SimulationStatus::Computing
                                                    : SimulationStatus::NoRoom);
            status.store(newStatus, std::memory_order_release);

            if (meshChanged)
            {
                sceneReady = meshValid && gpuReady && raytracer->setScene(*mesh);
                if (meshValid && gpuReady && !sceneReady)
                {
                    newStatus = SimulationStatus::Failed;
                    juce::Logger::writeToLog("NoWave: Failed to upload room BVH. " +
                                             juce::String(raytracer->getLastError()));
                }
            }

            std::vector<RaySegment> rayPaths;
            bool publish = false;
            if (newStatus == SimulationStatus::Computing && sceneReady)
            {
                IRBuffer* ir = irQueue.getWriteBuffer();
                ir->clear();
                const RaytraceParameters traceParameters =
                    makeRaytraceParameters(static_cast<int>(ir->channels[0].size()));
                const bool traced = raytracer->trace(
                    traceParameters,
                    traceParameters.limits[2],
                    traceParameters.limits[3],
                    *ir,
                    rayPaths);
                if (!traced)
                {
                    rayPaths.clear();
                    ir->clear();
                    newStatus = SimulationStatus::Failed;
                    juce::Logger::writeToLog("NoWave: GPU raytrace failed. " +
                                             juce::String(raytracer->getLastError()));
                }
                else if (!ir->isEmpty() && !ir->prepareConvolutionBuffers())
                {
                    ir->clear();
                    newStatus = SimulationStatus::Failed;
                    juce::Logger::writeToLog(
                        "NoWave: Failed to prepare convolution buffers for the generated IR.");
                }
                else
                {
                    newStatus = SimulationStatus::Ready;
                }
                publish = true;
            }
            else if (hasPublishedIR)
            {
                // Ohne GPU-Ergebnis wird eine leere IR veröffentlicht, das Signal bleibt trocken.
                irQueue.getWriteBuffer()->clear();
                publish = true;
            }

            if (publish)
            {
                irQueue.publishWriteBuffer();
                hasPublishedIR = newStatus == SimulationStatus::Ready;
            }

            status.store(newStatus, std::memory_order_release);
            publishRayPaths(std::move(rayPaths));
            lastMesh = mesh;
            recordSimulationParameters();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kDebounceMs));
    }

    vulkanAvailable.store(false, std::memory_order_release);
    status.store(SimulationStatus::NoGpu, std::memory_order_release);
    raytracer.reset();
}
} // namespace nowave