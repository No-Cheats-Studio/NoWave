// Validates the GPU raytracer against the classical reverberation-time formulas
// (Sabine / Eyring) in a shoebox room. Exit code 77 = skipped (no Vulkan GPU).

#include "audio/IRBuffer.h"
#include "raytracing/AcousticSimulation.h"
#include "raytracing/BVHNode.h"
#include "raytracing/VulkanRaytracer.h"
#include "scene/RoomMesh.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <vector>

using namespace nowave;

// C++ side of the GPU layouts (the GLSL structs in raytracer.comp must match).
static_assert(offsetof(RayTap, gainLow) == 16);
static_assert(offsetof(RaySegment, energyLow) == 32);
static_assert(offsetof(RaySegment, reflectionIndex) == 44);
static_assert(offsetof(GPUTriangle, materialIndex) == 64);
static_assert(offsetof(GPUBVHNode, triIndex) == 32);
static_assert(offsetof(RaytraceParameters, configuration) == 128);
static_assert(offsetof(RaytraceParameters, limits) == 176);

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr float kSizeX = 6.0f, kSizeY = 4.0f, kSizeZ = 5.0f;

RoomMesh makeShoebox(float absorption, float scattering)
{
    RoomMesh mesh;
    mesh.name = "Shoebox";
    AcousticMaterial material;
    material.absorptionLow = material.absorptionMid = material.absorptionHigh = absorption;
    material.scattering = scattering;
    mesh.materials.push_back(material);

    for (int i = 0; i < 8; ++i)
    {
        Vertex v;
        v.position = { (i & 1) ? kSizeX : 0.0f, (i & 2) ? kSizeY : 0.0f, (i & 4) ? kSizeZ : 0.0f };
        mesh.vertices.push_back(v);
    }
    const uint32_t quads[6][4] = { { 0, 1, 3, 2 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 },
                                   { 2, 3, 7, 6 }, { 0, 2, 6, 4 }, { 1, 3, 7, 5 } };
    for (const auto& q : quads)
    {
        mesh.triangles.push_back({ q[0], q[1], q[2], 0 });
        mesh.triangles.push_back({ q[0], q[2], q[3], 0 });
    }
    mesh.recomputeBounds();
    mesh.recomputeNormals();
    return mesh;
}

void setVec(float (&dst)[4], float x, float y, float z, float w)
{
    dst[0] = x; dst[1] = y; dst[2] = z; dst[3] = w;
}

// Schroeder backward integration, then a linear fit of the decay between
// startDb and endDb; returns the extrapolated 60 dB decay time in seconds.
double measureRt60(const std::vector<float>& ir, int length, double startDb, double endDb)
{
    std::vector<double> edc(static_cast<size_t>(length));
    double sum = 0.0;
    for (int i = length - 1; i >= 0; --i)
    {
        sum += static_cast<double>(ir[static_cast<size_t>(i)]) * ir[static_cast<size_t>(i)];
        edc[static_cast<size_t>(i)] = sum;
    }
    if (sum <= 0.0)
        return -1.0;

    double n = 0, sx = 0, sy = 0, sxx = 0, sxy = 0;
    for (int i = 0; i < length; ++i)
    {
        const double db = 10.0 * std::log10(edc[static_cast<size_t>(i)] / edc[0]);
        if (db > startDb || db < endDb)
            continue;
        const double t = i / kSampleRate;
        n += 1; sx += t; sy += db; sxx += t * t; sxy += t * db;
    }
    if (n < 10)
        return -1.0;
    const double slope = (n * sxy - sx * sy) / (n * sxx - sx * sx); // dB per second
    return slope < 0.0 ? -60.0 / slope : -1.0;
}

struct Case { float absorption; float scattering; int bounces; };

bool runCase(VulkanRaytracer& raytracer, const Case& c)
{
    const RoomMesh mesh = makeShoebox(c.absorption, c.scattering);
    if (!raytracer.setScene(mesh))
    {
        std::printf("setScene failed: %s\n", raytracer.getLastError().c_str());
        return false;
    }

    const double volume = double(kSizeX) * kSizeY * kSizeZ;
    const double surface = 2.0 * (double(kSizeX) * kSizeY + double(kSizeX) * kSizeZ +
                                  double(kSizeY) * kSizeZ);
    const double sabine = 0.161 * volume / (surface * c.absorption);
    const double eyring = 0.161 * volume / (-surface * std::log(1.0 - c.absorption));

    const int irLength = static_cast<int>(kSampleRate * 1.5);
    IRBuffer ir;
    ir.allocate(irLength);

    RaytraceParameters p {};
    setVec(p.sourcePositions[0], 1.5f, 1.2f, 1.0f, 1.0f);
    setVec(p.sourceDirections[0], 0.0f, 0.0f, 1.0f, 0.0f);
    setVec(p.receiverPositions[0], 4.2f, 1.5f, 3.6f, 1.0f);
    setVec(p.receiverDirections[0], 0.0f, 0.0f, -1.0f, 0.0f);
    p.configuration[0] = 1;
    p.configuration[1] = 1;
    p.patterns[0] = static_cast<int>(MicrophonePattern::Omnidirectional);
    p.patterns[1] = static_cast<int>(MicrophonePattern::Omnidirectional);
    p.physics[0] = 0.2f;
    p.physics[1] = 343.0f;
    p.physics[2] = static_cast<float>(kSampleRate);
    p.limits[0] = irLength;
    p.limits[1] = kMaxRayTaps;

    std::vector<RaySegment> paths;
    if (!raytracer.trace(p, kMaxRayCount / 2, c.bounces, ir, paths) || ir.isEmpty())
    {
        std::printf("trace failed: %s\n", raytracer.getLastError().c_str());
        return false;
    }

    const double measured = measureRt60(ir.channels[IRBuffer::channelIndex(1, 0, 0)],
                                        ir.sampleLength, -5.0, -25.0);
    const double reference = c.absorption < 0.15f ? sabine : eyring;
    const double error = measured > 0.0 ? (measured - reference) / reference : 1.0;
    std::printf("alpha=%.2f scatter=%.2f: measured T60=%.3f s, Sabine=%.3f s, Eyring=%.3f s, "
                "deviation vs reference=%+.1f %%\n",
                c.absorption, c.scattering, measured, sabine, eyring, error * 100.0);
    return measured > 0.0 && std::abs(error) <= 0.20;
}
} // namespace

int main()
{
    VulkanRaytracer raytracer;
    if (!raytracer.initialize())
    {
        std::printf("SKIP: no Vulkan GPU available (%s)\n", raytracer.getLastError().c_str());
        return 77;
    }
    std::printf("GPU: %s\n", raytracer.getDeviceName().c_str());

    const Case cases[] = { { 0.20f, 0.30f, 64 }, { 0.35f, 0.30f, 48 }, { 0.50f, 0.30f, 32 } };
    bool ok = true;
    for (const auto& c : cases)
        ok = runCase(raytracer, c) && ok;

    std::printf(ok ? "PASS\n" : "FAIL\n");
    return ok ? 0 : 1;
}
