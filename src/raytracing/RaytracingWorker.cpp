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
    running.store(true, std::memory_order_release);
    thread = std::thread(&RaytracingWorker::threadFunc, this);
}

void RaytracingWorker::stop()
{
    running.store(false, std::memory_order_release);
    if (thread.joinable())
        thread.join();
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
           rayPathsEnabled(params) != lastShowRayPaths;
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
        float offset = 0.0f;
        float yawOffset = 0.0f;
        if (outputType == OutputType::SpeakerBox)
        {
            offset = 0.06f;
            yawOffset = side * 30.0f;
        }
        else if (outputType == OutputType::PA)
        {
            offset = 0.15f;
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
        float offset = 0.0f;
        float yawOffset = 0.0f;
        if (inputType == InputType::Ears)
        {
            offset = 0.09f;
            yawOffset = side * 10.0f;
        }
        else if (inputType == InputType::PA)
        {
            offset = 0.15f;
        }
        setVector(result.receiverPositions[receiver],
                  receiverCenter + receiverRight * (side * offset), 1.0f);
        setVector(result.receiverDirections[receiver],
                  directionFromYawPitch(receiverYaw + yawOffset, receiverPitch), 0.0f);
        result.patterns[receiver + 1] = static_cast<int>(
            inputType == InputType::PA
                ? (left ? leftPattern : rightPattern)
                : (inputType == InputType::Microphone
                       ? microphonePattern
                       : MicrophonePattern::Cardioid));
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

void RaytracingWorker::synthesizeDirectPath(const RaytraceParameters& parameters,
                                             IRBuffer& ir) const noexcept
{
    ir.clear();
    ir.sourceCount = parameters.configuration[0];
    ir.receiverCount = parameters.configuration[1];
    int sampleLength = 0;

    for (int source = 0; source < ir.sourceCount; ++source)
    {
        const glm::vec3 sourcePosition(parameters.sourcePositions[source][0],
                                       parameters.sourcePositions[source][1],
                                       parameters.sourcePositions[source][2]);
        const glm::vec3 sourceDirection(parameters.sourceDirections[source][0],
                                        parameters.sourceDirections[source][1],
                                        parameters.sourceDirections[source][2]);
        for (int receiver = 0; receiver < ir.receiverCount; ++receiver)
        {
            const glm::vec3 receiverPosition(parameters.receiverPositions[receiver][0],
                                             parameters.receiverPositions[receiver][1],
                                             parameters.receiverPositions[receiver][2]);
            const glm::vec3 receiverDirection(parameters.receiverDirections[receiver][0],
                                              parameters.receiverDirections[receiver][1],
                                              parameters.receiverDirections[receiver][2]);
            const glm::vec3 displacement = receiverPosition - sourcePosition;
            const float distance = glm::length(displacement);
            if (distance <= 1.0e-6f)
                continue;

            const int sampleIndex = static_cast<int>(
                distance / kSpeedOfSound * static_cast<float>(sampleRate));
            if (sampleIndex < 0 ||
                sampleIndex >= std::min(parameters.limits[0],
                                        static_cast<int>(ir.channels[0].size())))
                continue;

            const glm::vec3 direction = displacement / distance;
            const float sourceGain = patternGain(
                static_cast<MicrophonePattern>(parameters.patterns[0]),
                glm::dot(sourceDirection, direction));
            const float receiverGain = patternGain(
                static_cast<MicrophonePattern>(parameters.patterns[receiver + 1]),
                glm::dot(receiverDirection, -direction));
            const float gain = sourceGain * receiverGain /
                               std::max(distance, 0.1f);
            for (size_t band = 0; band < IRBuffer::kBandCount; ++band)
            {
                const size_t route = IRBuffer::channelIndex(
                    band, static_cast<size_t>(receiver), static_cast<size_t>(source));
                ir.channels[route][static_cast<size_t>(sampleIndex)] += gain;
            }
            sampleLength = std::max(sampleLength, sampleIndex + 1);
        }
    }

    if (ir.receiverCount == 1)
    {
        for (size_t band = 0; band < IRBuffer::kBandCount; ++band)
        {
            const size_t bandOffset = band * IRBuffer::kRoutesPerBand;
            std::copy(ir.channels[bandOffset].begin(),
                      ir.channels[bandOffset].begin() + sampleLength,
                      ir.channels[bandOffset + IRBuffer::kRightFromLeft].begin());
            std::copy(ir.channels[bandOffset + IRBuffer::kLeftFromRight].begin(),
                      ir.channels[bandOffset + IRBuffer::kLeftFromRight].begin() + sampleLength,
                      ir.channels[bandOffset + IRBuffer::kRightFromRight].begin());
        }
    }
    ir.sampleLength = sampleLength;
}

void RaytracingWorker::threadFunc()
{
    raytracer = std::make_unique<VulkanRaytracer>();
    const bool gpuReady = raytracer->initialize();
    vulkanAvailable.store(gpuReady, std::memory_order_release);
    if (!gpuReady)
        juce::Logger::writeToLog("NoWave: Vulkan raytracing unavailable; using direct-path fallback. " +
                                 juce::String(raytracer->getLastError()));

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
            if (meshChanged)
            {
                sceneReady = mesh != nullptr && mesh->isValid() && gpuReady &&
                             raytracer->setScene(*mesh);
                if (mesh != nullptr && mesh->isValid() && gpuReady && !sceneReady)
                    juce::Logger::writeToLog("NoWave: Failed to upload room BVH; using direct-path fallback. " +
                                             juce::String(raytracer->getLastError()));
            }

            IRBuffer* ir = irQueue.getWriteBuffer();
            ir->clear();
            std::vector<RaySegment> rayPaths;
            if (mesh != nullptr && mesh->isValid())
            {
                const RaytraceParameters traceParameters =
                    makeRaytraceParameters(static_cast<int>(ir->channels[0].size()));
                const bool traced = sceneReady && raytracer->trace(
                    traceParameters,
                    traceParameters.limits[2],
                    traceParameters.limits[3],
                    *ir,
                    rayPaths);
                if (!traced)
                {
                    rayPaths.clear();
                    if (sceneReady)
                        juce::Logger::writeToLog("NoWave: GPU raytrace failed; using direct-path fallback. " +
                                                 juce::String(raytracer->getLastError()));
                    synthesizeDirectPath(traceParameters, *ir);
                }
                if (!ir->isEmpty() && !ir->prepareConvolutionBuffers())
                {
                    juce::Logger::writeToLog(
                        "NoWave: Failed to prepare convolution buffers for the generated IR.");
                    ir->clear();
                }
                irQueue.publishWriteBuffer();
                hasPublishedIR = true;
            }
            else if (hasPublishedIR)
            {
                irQueue.publishWriteBuffer();
                hasPublishedIR = false;
            }

            publishRayPaths(std::move(rayPaths));
            lastMesh = mesh;
            recordSimulationParameters();
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(kDebounceMs));
    }

    vulkanAvailable.store(false, std::memory_order_release);
    raytracer.reset();
}
} // namespace nowave
