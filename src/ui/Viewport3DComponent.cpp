#include "ui/Viewport3DComponent.h"
#include "ui/Palette.h"

#include <algorithm>
#include <cmath>
#include <glm/geometric.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/mat4x4.hpp>
#include <vector>

namespace nowave
{
namespace
{
constexpr float kCameraSpeed = 2.5f;     // metres per second
constexpr float kFastCameraSpeed = 7.5f; // metres per second while Shift is held

constexpr const char* kVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in float aSurfaceType;
layout (location = 3) in float aRayEnergy;

uniform mat4 uMvp;
uniform mat4 uModel;

out vec3 vNormal;
out vec3 vWorldPos;
flat out int vSurfaceType;
out float vRayEnergy;

void main()
{
    vec4 worldPos = uModel * vec4(aPosition, 1.0);
    vWorldPos = worldPos.xyz;
    vNormal = mat3(uModel) * aNormal;
    vSurfaceType = int(aSurfaceType + 0.5);
    vRayEnergy = aRayEnergy;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)";

constexpr const char* kFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec3 vWorldPos;
flat in int vSurfaceType;
in float vRayEnergy;

uniform vec3 uColor;
uniform float uAlpha;
uniform int uUseLighting;
uniform int uUseMaterialTexture;
uniform int uRayMode;

out vec4 FragColor;

float hash21(vec2 p)
{
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

float valueNoise(vec2 p)
{
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * (3.0 - 2.0 * f);
    float a = hash21(i);
    float b = hash21(i + vec2(1.0, 0.0));
    float c = hash21(i + vec2(0.0, 1.0));
    float d = hash21(i + vec2(1.0, 1.0));
    return mix(mix(a, b, u.x), mix(c, d, u.x), u.y);
}

float fbm(vec2 p)
{
    return 0.6 * valueNoise(p) + 0.4 * valueNoise(p * 2.7 + vec2(3.1, 1.7));
}

vec2 surfaceUV(vec3 position, vec3 normal)
{
    vec3 n = abs(normalize(normal));
    if (n.y > 0.6)
        return position.xz;
    if (n.x > n.z)
        return position.zy;
    return position.xy;
}

// uv is in metres (see surfaceUV), so all scales below are real-world sizes.
vec3 proceduralMaterial(int type, vec2 uv)
{
    if (type == 1)
    {
        // Ashlar masonry: 0.60 x 0.30 m blocks, staggered courses, 12 mm joints
        vec2 blockSize = vec2(0.60, 0.30);
        vec2 blockUv = uv / blockSize;
        blockUv.x += mod(floor(blockUv.y), 2.0) * 0.5;
        vec2 cell = floor(blockUv);
        vec2 local = fract(blockUv);
        float edge = min(min(local.x, 1.0 - local.x) * blockSize.x,
                         min(local.y, 1.0 - local.y) * blockSize.y);
        float joint = 1.0 - smoothstep(0.004, 0.009, edge);
        float tone = hash21(cell + 17.0);
        float grain = fbm(uv * 8.0);
        vec3 stone = mix(vec3(0.33, 0.34, 0.36), vec3(0.62, 0.62, 0.60),
                         tone * 0.5 + grain * 0.35);
        return mix(stone, vec3(0.50, 0.49, 0.47), joint * 0.85);
    }
    if (type == 2)
    {
        // Curtain-wall panes 1.35 x 1.80 m with thin aluminium frames
        vec2 paneSize = vec2(1.35, 1.80);
        vec2 pane = fract(uv / paneSize);
        vec2 frameDist = min(pane, 1.0 - pane) * paneSize;
        float frame = 1.0 - smoothstep(0.02, 0.035, min(frameDist.x, frameDist.y));
        float sky = 0.5 + 0.5 * sin(uv.x * 0.9 + uv.y * 0.35);
        float haze = fbm(uv * 0.7);
        vec3 glass = mix(vec3(0.08, 0.25, 0.33), vec3(0.32, 0.55, 0.63),
                         sky * 0.4 + haze * 0.3);
        return mix(glass, vec3(0.07, 0.09, 0.10), frame);
    }
    if (type == 3)
    {
        // Running bond: 215 x 65 mm bricks with 10 mm mortar joints
        vec2 brickSize = vec2(0.215, 0.065);
        vec2 brickUv = uv / brickSize;
        brickUv.x += mod(floor(brickUv.y), 2.0) * 0.5;
        vec2 cell = floor(brickUv);
        vec2 local = fract(brickUv);
        vec2 edgeDist = min(local, 1.0 - local) * brickSize;
        float mortar = 1.0 - smoothstep(0.004, 0.008, min(edgeDist.x, edgeDist.y));
        float tone = hash21(cell + 3.7);
        float fleck = valueNoise(uv * 160.0);
        vec3 fired = mix(vec3(0.42, 0.17, 0.09), vec3(0.64, 0.31, 0.19),
                         tone * 0.7 + fleck * 0.3);
        return mix(fired, vec3(0.60, 0.58, 0.54), mortar);
    }
    if (type == 4)
    {
        // Floor-style planks, 0.14 m wide with fine seams and lengthwise grain
        float plankWidth = 0.14;
        float across = fract(uv.x / plankWidth);
        float seam = 1.0 - smoothstep(0.0, 0.0025, min(across, 1.0 - across) * plankWidth);
        float plankTone = hash21(vec2(floor(uv.x / plankWidth), 5.0));
        float streak = valueNoise(vec2(uv.x * 140.0, uv.y * 2.5));
        float figure = fbm(vec2(uv.x * 20.0, uv.y * 0.8));
        vec3 wood = mix(vec3(0.30, 0.16, 0.08), vec3(0.58, 0.36, 0.19),
                        plankTone * 0.5 + streak * 0.3 + figure * 0.2);
        return mix(wood, vec3(0.14, 0.07, 0.035), seam * 0.9);
    }
    if (type == 5)
    {
        // Velour: vertical folds about 0.12 m apart with a fine pile texture
        float fold = 0.5 + 0.5 * cos(uv.x * 52.0);
        float shade = 0.55 + 0.45 * fold;
        float pile = valueNoise(uv * vec2(110.0, 35.0));
        return vec3(0.36, 0.08, 0.13) * shade * (0.88 + 0.18 * pile);
    }
    if (type == 6)
    {
        // Mineral wool: irregular fibres at the centimetre scale, subtle tonal drift
        float tone = fbm(uv * 3.0);
        float fibres = valueNoise(uv * vec2(70.0, 45.0));
        float strands = valueNoise(uv * vec2(150.0, 100.0));
        vec3 base = mix(vec3(0.55, 0.46, 0.28), vec3(0.78, 0.68, 0.44), tone);
        return base * (0.80 + 0.14 * fibres + 0.08 * strands);
    }
    if (type == 7)
    {
        // Poured concrete: 2.4 x 1.2 m formwork panels with tie holes and surface mottling
        vec2 panel = fract(uv / vec2(2.4, 1.2));
        vec2 panelEdge = min(panel, 1.0 - panel) * vec2(2.4, 1.2);
        float seam = 1.0 - smoothstep(0.003, 0.008, min(panelEdge.x, panelEdge.y));
        float mottling = fbm(uv * 2.0);
        float pores = step(0.93, valueNoise(uv * 60.0));
        vec3 concrete = mix(vec3(0.38, 0.39, 0.40), vec3(0.58, 0.58, 0.57), mottling);
        concrete *= 1.0 - 0.18 * pores;
        return mix(concrete, vec3(0.24, 0.25, 0.26), seam * 0.8);
    }
    if (type == 8)
    {
        // Carpet: dense pile at the millimetre scale, subtle large-scale tonal drift
        float pile = valueNoise(uv * 220.0);
        float weave = 0.5 + 0.5 * sin(uv.x * 900.0) * sin(uv.y * 900.0);
        float drift = fbm(uv * 1.5);
        vec3 base = mix(vec3(0.20, 0.23, 0.30), vec3(0.28, 0.32, 0.40), drift);
        return base * (0.80 + 0.14 * pile + 0.08 * weave);
    }
    if (type == 9)
    {
        // Painted plaster: near-flat off-white with faint trowel marks
        float trowel = fbm(uv * 1.1);
        float grain = valueNoise(uv * 90.0);
        vec3 paint = mix(vec3(0.78, 0.76, 0.71), vec3(0.86, 0.84, 0.79), trowel);
        return paint * (0.97 + 0.04 * grain);
    }
    if (type == 10)
    {
        // Suspended ceiling: 0.60 x 0.60 m tiles in a metal grid, fine perforation
        vec2 tile = fract(uv / 0.6);
        vec2 gridDist = min(tile, 1.0 - tile) * 0.6;
        float grid = 1.0 - smoothstep(0.006, 0.012, min(gridDist.x, gridDist.y));
        vec2 hole = fract(uv * 55.0) - 0.5;
        float perforation = 1.0 - smoothstep(0.10, 0.16, length(hole));
        float tone = hash21(floor(uv / 0.6) + 11.0);
        vec3 board = mix(vec3(0.80, 0.80, 0.77), vec3(0.88, 0.88, 0.85), tone);
        board *= 1.0 - 0.22 * perforation;
        return mix(board, vec3(0.62, 0.64, 0.66), grid);
    }
    return vec3(0.42, 0.46, 0.52);
}

void main()
{
    if (uRayMode != 0)
    {
        FragColor = vec4(uColor, uAlpha * clamp(vRayEnergy, 0.0, 1.0));
        return;
    }

    if (uUseLighting == 0)
    {
        FragColor = vec4(uColor, uAlpha);
        return;
    }

    vec3 n = normalize(vNormal);
    vec3 lightDir = normalize(vec3(0.4, 0.9, 0.3));
    float diff = abs(dot(n, lightDir));
    float shade = 0.35 + 0.65 * diff;
    vec3 baseColor = uUseMaterialTexture != 0
                         ? proceduralMaterial(vSurfaceType, surfaceUV(vWorldPos, n))
                         : vec3(1.0);
    float alpha = uAlpha * (vSurfaceType == 2 && uUseMaterialTexture != 0 ? 0.55 : 1.0);
    FragColor = vec4(baseColor * uColor * shade, alpha);
}
)";

struct GLVertex
{
    float px, py, pz;
    float nx, ny, nz;
    float surfaceType { 0.0f };
    float rayEnergy { 1.0f };
};

float paValue(std::atomic<float>* const* pointers, int channel, float fallback)
{
    const auto* value = pointers[channel];
    return value != nullptr ? value->load() : fallback;
}
} // namespace

Viewport3DComponent::Viewport3DComponent(const ParameterPointers& paramPointers,
                                         RayPathProvider pathProvider,
                                         std::function<void()> onChangeRoomRequested)
    : params(paramPointers),
      rayPathProvider(std::move(pathProvider))
{
    roomNameLabel.setText("No Room Loaded", juce::dontSendNotification);
    roomNameLabel.setFont(juce::FontOptions(14.0f, juce::Font::bold));
    roomNameLabel.setColour(juce::Label::textColourId, palette::text);
    roomNameLabel.setColour(juce::Label::backgroundColourId, palette::overlay);
    addAndMakeVisible(roomNameLabel);

    cameraHintLabel.setText("Hold LMB: WASD move, drag look, Space up, Ctrl down",
                            juce::dontSendNotification);
    cameraHintLabel.setFont(juce::FontOptions(11.5f));
    cameraHintLabel.setColour(juce::Label::textColourId, palette::textLabel);
    cameraHintLabel.setColour(juce::Label::backgroundColourId, palette::overlay);
    cameraHintLabel.setJustificationType(juce::Justification::centred);
    cameraHintLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(cameraHintLabel);

    changeRoomButton.setColour(juce::TextButton::buttonColourId, palette::viewportButton);
    changeRoomButton.setColour(juce::TextButton::textColourOffId, palette::text);
    changeRoomButton.onClick = std::move(onChangeRoomRequested);
    addAndMakeVisible(changeRoomButton);

    setWantsKeyboardFocus(true);
    setMouseClickGrabsKeyboardFocus(true);
    openGLContext.setOpenGLVersionRequired(juce::OpenGLContext::openGL3_2);
    openGLContext.setRenderer(this);
    openGLContext.setContinuousRepainting(true);
    openGLContext.attachTo(*this);
}

Viewport3DComponent::~Viewport3DComponent()
{
    openGLContext.detach();
}

void Viewport3DComponent::setRoomMesh(std::shared_ptr<const RoomMesh> mesh)
{
    {
        const std::lock_guard<std::mutex> lock(meshMutex);
        pendingMesh = std::move(mesh);
        meshDirty = true;
    }

    if (pendingMesh != nullptr)
    {
        roomNameLabel.setText("Room: " + juce::String(pendingMesh->name),
                              juce::dontSendNotification);
    }
    else
    {
        roomNameLabel.setText("No Room Loaded", juce::dontSendNotification);
    }
}

void Viewport3DComponent::paint(juce::Graphics& g)
{
    juce::ignoreUnused(g);
}

void Viewport3DComponent::resized()
{
    auto topBar = getLocalBounds().reduced(12).removeFromTop(28);
    changeRoomButton.setBounds(topBar.removeFromRight(120));
    topBar.removeFromRight(8);
    roomNameLabel.setBounds(topBar.removeFromLeft(280));
    cameraHintLabel.setBounds(getLocalBounds().reduced(12).removeFromBottom(24).removeFromLeft(400));
}

void Viewport3DComponent::mouseDown(const juce::MouseEvent& event)
{
    lastDragPos = event.getPosition();
    if (event.mods.isLeftButtonDown())
    {
        cameraNavigationHeld = true;
        grabKeyboardFocus();
        startTimerHz(60);
    }
}

void Viewport3DComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    cameraNavigationHeld = false;
    stopTimer();
}

void Viewport3DComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!event.mods.isLeftButtonDown())
        return;

    const auto pos = event.getPosition();
    const float dx = static_cast<float>(pos.x - lastDragPos.x);
    const float dy = static_cast<float>(pos.y - lastDragPos.y);
    lastDragPos = pos;

    constexpr float sensitivity = 0.25f;
    yawDeg.store(yawDeg.load() + dx * sensitivity);
    const float nextPitch = std::clamp(pitchDeg.load() - dy * sensitivity, -89.0f, 89.0f);
    pitchDeg.store(nextPitch);
}

void Viewport3DComponent::timerCallback()
{
    if (!cameraNavigationHeld)
    {
        stopTimer();
        return;
    }

    const float yaw = glm::radians(yawDeg.load());
    const glm::vec3 forward(std::sin(yaw), 0.0f, -std::cos(yaw));
    const glm::vec3 right(std::cos(yaw), 0.0f, std::sin(yaw));
    glm::vec3 movement(0.0f);

    if (juce::KeyPress::isKeyCurrentlyDown('w'))
        movement += forward;
    if (juce::KeyPress::isKeyCurrentlyDown('s'))
        movement -= forward;
    if (juce::KeyPress::isKeyCurrentlyDown('d'))
        movement += right;
    if (juce::KeyPress::isKeyCurrentlyDown('a'))
        movement -= right;
    if (juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
        movement.y += 1.0f;
    if (juce::ModifierKeys::getCurrentModifiers().isCtrlDown())
        movement.y -= 1.0f;

    const float length = glm::length(movement);
    if (length > 0.0f)
    {
        const bool fast = juce::ModifierKeys::getCurrentModifiers().isShiftDown();
        const float speedMetresPerSecond = fast ? kFastCameraSpeed : kCameraSpeed;
        movement *= (speedMetresPerSecond / 60.0f) / length;
        cameraX.store(cameraX.load() + movement.x);
        cameraY.store(cameraY.load() + movement.y);
        cameraZ.store(cameraZ.load() + movement.z);
    }
}

void Viewport3DComponent::newOpenGLContextCreated()
{
    using namespace juce::gl;

    shader = std::make_unique<juce::OpenGLShaderProgram>(openGLContext);
    shader->addVertexShader(kVertexShader);
    shader->addFragmentShader(kFragmentShader);
    shader->link();

    uMvpLoc = glGetUniformLocation(shader->getProgramID(), "uMvp");
    uModelLoc = glGetUniformLocation(shader->getProgramID(), "uModel");
    uColorLoc = glGetUniformLocation(shader->getProgramID(), "uColor");
    uAlphaLoc = glGetUniformLocation(shader->getProgramID(), "uAlpha");
    uUseLightingLoc = glGetUniformLocation(shader->getProgramID(), "uUseLighting");
    uUseMaterialTextureLoc =
        glGetUniformLocation(shader->getProgramID(), "uUseMaterialTexture");
    uRayModeLoc = glGetUniformLocation(shader->getProgramID(), "uRayMode");

    createMarkerGeometry();
    createDirectionGeometry();
    createRayGeometry();
}

void Viewport3DComponent::openGLContextClosing()
{
    destroyBuffers();
    shader.reset();
}

void Viewport3DComponent::createMarkerGeometry()
{
    using namespace juce::gl;

    // Octahedron marker (8 triangles, radius 0.25m)
    constexpr float r = 0.25f;
    const glm::vec3 top(0.0f, r, 0.0f);
    const glm::vec3 bot(0.0f, -r, 0.0f);
    const glm::vec3 px(r, 0.0f, 0.0f);
    const glm::vec3 nx(-r, 0.0f, 0.0f);
    const glm::vec3 pz(0.0f, 0.0f, r);
    const glm::vec3 nz(0.0f, 0.0f, -r);

    std::vector<GLVertex> verts;
    auto pushTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        const glm::vec3 n = glm::normalize(glm::cross(b - a, c - a));
        verts.push_back({ a.x, a.y, a.z, n.x, n.y, n.z });
        verts.push_back({ b.x, b.y, b.z, n.x, n.y, n.z });
        verts.push_back({ c.x, c.y, c.z, n.x, n.y, n.z });
    };

    pushTri(top, pz, px);
    pushTri(top, px, nz);
    pushTri(top, nz, nx);
    pushTri(top, nx, pz);

    pushTri(bot, px, pz);
    pushTri(bot, nz, px);
    pushTri(bot, nx, nz);
    pushTri(bot, pz, nx);

    markerVertexCount = static_cast<GLsizei>(verts.size());

    glGenVertexArrays(1, &markerVao);
    glGenBuffers(1, &markerVbo);
    glBindVertexArray(markerVao);
    glBindBuffer(GL_ARRAY_BUFFER, markerVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(GLVertex)),
                 verts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);
}

void Viewport3DComponent::createDirectionGeometry()
{
    using namespace juce::gl;

    std::vector<GLVertex> verts;
    auto pushTri = [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c)
    {
        const glm::vec3 normal = glm::normalize(glm::cross(b - a, c - a));
        verts.push_back({ a.x, a.y, a.z, normal.x, normal.y, normal.z });
        verts.push_back({ b.x, b.y, b.z, normal.x, normal.y, normal.z });
        verts.push_back({ c.x, c.y, c.z, normal.x, normal.y, normal.z });
    };

    constexpr int segments = 12;
    constexpr float shaftRadius = 0.035f;
    constexpr float shaftStart = -0.12f;
    constexpr float shaftEnd = -0.62f;
    constexpr float headRadius = 0.14f;
    constexpr float tipZ = -1.05f;
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = juce::MathConstants<float>::twoPi *
                         static_cast<float>(i) / static_cast<float>(segments);
        const float a1 = juce::MathConstants<float>::twoPi *
                         static_cast<float>(i + 1) / static_cast<float>(segments);
        const glm::vec3 upper0(shaftRadius * std::cos(a0), shaftRadius * std::sin(a0), shaftStart);
        const glm::vec3 upper1(shaftRadius * std::cos(a1), shaftRadius * std::sin(a1), shaftStart);
        const glm::vec3 lower0(shaftRadius * std::cos(a0), shaftRadius * std::sin(a0), shaftEnd);
        const glm::vec3 lower1(shaftRadius * std::cos(a1), shaftRadius * std::sin(a1), shaftEnd);
        const glm::vec3 base0(headRadius * std::cos(a0), headRadius * std::sin(a0), shaftEnd);
        const glm::vec3 base1(headRadius * std::cos(a1), headRadius * std::sin(a1), shaftEnd);
        const glm::vec3 tip(0.0f, 0.0f, tipZ);

        pushTri(upper0, lower0, upper1);
        pushTri(upper1, lower0, lower1);
        pushTri(base0, tip, base1);
    }

    directionVertexCount = static_cast<GLsizei>(verts.size());
    glGenVertexArrays(1, &directionVao);
    glGenBuffers(1, &directionVbo);
    glBindVertexArray(directionVao);
    glBindBuffer(GL_ARRAY_BUFFER, directionVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(GLVertex)),
                 verts.data(),
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);
}

void Viewport3DComponent::createRayGeometry()
{
    using namespace juce::gl;

    glGenVertexArrays(1, &rayPathVao);
    glGenBuffers(1, &rayPathVbo);
    glBindVertexArray(rayPathVao);
    glBindBuffer(GL_ARRAY_BUFFER, rayPathVbo);
    glBufferData(GL_ARRAY_BUFFER, 0, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(7 * sizeof(float)));
    glBindVertexArray(0);
}

void Viewport3DComponent::uploadPendingMeshIfNeeded()
{
    using namespace juce::gl;

    std::shared_ptr<const RoomMesh> meshCopy;
    {
        const std::lock_guard<std::mutex> lock(meshMutex);
        if (!meshDirty)
            return;
        meshCopy = pendingMesh;
        meshDirty = false;
    }

    if (roomVbo != 0)
    {
        glDeleteBuffers(1, &roomVbo);
        roomVbo = 0;
    }
    if (roomVao != 0)
    {
        glDeleteVertexArrays(1, &roomVao);
        roomVao = 0;
    }
    roomVertexCount = 0;

    if (meshCopy == nullptr || !meshCopy->isValid())
        return;

    const glm::vec3 center = meshCopy->bounds.center();
    cameraX.store(center.x);
    cameraY.store(center.y);
    cameraZ.store(center.z);

    std::vector<GLVertex> gpuVerts;
    gpuVerts.reserve(meshCopy->triangles.size() * 3);

    for (const auto& tri : meshCopy->triangles)
    {
        const Vertex& v0 = meshCopy->vertices[tri.i0];
        const Vertex& v1 = meshCopy->vertices[tri.i1];
        const Vertex& v2 = meshCopy->vertices[tri.i2];
        const float surfaceType = tri.materialId < meshCopy->materials.size()
                                      ? static_cast<float>(meshCopy->materials[tri.materialId].texture)
                                      : static_cast<float>(SurfaceTexture::Neutral);

        gpuVerts.push_back({ v0.position.x, v0.position.y, v0.position.z,
                             v0.normal.x, v0.normal.y, v0.normal.z, surfaceType });
        gpuVerts.push_back({ v1.position.x, v1.position.y, v1.position.z,
                             v1.normal.x, v1.normal.y, v1.normal.z, surfaceType });
        gpuVerts.push_back({ v2.position.x, v2.position.y, v2.position.z,
                             v2.normal.x, v2.normal.y, v2.normal.z, surfaceType });
    }

    roomVertexCount = static_cast<GLsizei>(gpuVerts.size());

    glGenVertexArrays(1, &roomVao);
    glGenBuffers(1, &roomVbo);
    glBindVertexArray(roomVao);
    glBindBuffer(GL_ARRAY_BUFFER, roomVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(gpuVerts.size() * sizeof(GLVertex)),
                 gpuVerts.data(),
                 GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex), nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<const void*>(6 * sizeof(float)));
    glBindVertexArray(0);
}

void Viewport3DComponent::uploadRayPathsIfNeeded()
{
    using namespace juce::gl;

    const auto newSnapshot = rayPathProvider ? rayPathProvider() : nullptr;
    if (newSnapshot == rayPathSnapshot)
        return;

    rayPathSnapshot = newSnapshot;
    rayPathVertexCount = 0;
    rayPathFirst.fill(0);
    rayPathCounts.fill(0);
    std::vector<GLVertex> vertices;
    if (rayPathSnapshot != nullptr)
    {
        std::array<std::vector<GLVertex>, 3> energyBins;
        for (const auto& segment : *rayPathSnapshot)
        {
            const float energy = std::clamp(
                (segment.start[3] + segment.end[3]) * 0.5f, 0.0f, 1.0f);
            const size_t bin = energy < 0.15f ? 0u : (energy < 0.5f ? 1u : 2u);
            energyBins[bin].push_back({ segment.start[0], segment.start[1], segment.start[2],
                                        0.0f, 1.0f, 0.0f, 0.0f, energy });
            energyBins[bin].push_back({ segment.end[0], segment.end[1], segment.end[2],
                                        0.0f, 1.0f, 0.0f, 0.0f, energy });
        }

        vertices.reserve(rayPathSnapshot->size() * 2);
        for (size_t bin = 0; bin < energyBins.size(); ++bin)
        {
            rayPathFirst[bin] = static_cast<GLint>(vertices.size());
            rayPathCounts[bin] = static_cast<GLsizei>(energyBins[bin].size());
            vertices.insert(vertices.end(), energyBins[bin].begin(), energyBins[bin].end());
        }
    }

    rayPathVertexCount = static_cast<GLsizei>(vertices.size());
    glBindBuffer(GL_ARRAY_BUFFER, rayPathVbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertices.size() * sizeof(GLVertex)),
                 vertices.empty() ? nullptr : vertices.data(),
                 GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Viewport3DComponent::destroyBuffers()
{
    using namespace juce::gl;

    if (roomVbo != 0)
    {
        glDeleteBuffers(1, &roomVbo);
        roomVbo = 0;
    }
    if (roomVao != 0)
    {
        glDeleteVertexArrays(1, &roomVao);
        roomVao = 0;
    }
    if (markerVbo != 0)
    {
        glDeleteBuffers(1, &markerVbo);
        markerVbo = 0;
    }
    if (markerVao != 0)
    {
        glDeleteVertexArrays(1, &markerVao);
        markerVao = 0;
    }
    if (directionVbo != 0)
    {
        glDeleteBuffers(1, &directionVbo);
        directionVbo = 0;
    }
    if (directionVao != 0)
    {
        glDeleteVertexArrays(1, &directionVao);
        directionVao = 0;
    }
    if (rayPathVbo != 0)
    {
        glDeleteBuffers(1, &rayPathVbo);
        rayPathVbo = 0;
    }
    if (rayPathVao != 0)
    {
        glDeleteVertexArrays(1, &rayPathVao);
        rayPathVao = 0;
    }
}

void Viewport3DComponent::renderOpenGL()
{
    using namespace juce::gl;

    if (shader == nullptr)
        return;

    uploadPendingMeshIfNeeded();

    const float scale = static_cast<float>(openGLContext.getRenderingScale());
    const int width = std::max(1, static_cast<int>(static_cast<float>(getWidth()) * scale));
    const int height = std::max(1, static_cast<int>(static_cast<float>(getHeight()) * scale));

    glViewport(0, 0, width, height);
    const glm::vec3 clear = palette::rgb(palette::argb::background);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    const float aspect = static_cast<float>(width) / static_cast<float>(height);
    const glm::mat4 proj = glm::perspective(glm::radians(65.0f), aspect, 0.05f, 200.0f);

    const float yawRad = glm::radians(yawDeg.load());
    const float pitchRad = glm::radians(pitchDeg.load());
    const glm::vec3 forward(
        std::cos(pitchRad) * std::sin(yawRad),
        std::sin(pitchRad),
        -std::cos(pitchRad) * std::cos(yawRad));

    const glm::vec3 cameraPos(cameraX.load(), cameraY.load(), cameraZ.load());
    const glm::mat4 view = glm::lookAt(cameraPos, cameraPos + forward, glm::vec3(0.0f, 1.0f, 0.0f));
    const glm::mat4 vp = proj * view;

    shader->use();

    if (roomVao != 0 && roomVertexCount > 0)
    {
        const glm::mat4 identity(1.0f);
        glUniformMatrix4fv(uMvpLoc, 1, GL_FALSE, glm::value_ptr(vp));
        glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(identity));
        glUniform1i(uRayModeLoc, 0);

        // Solid shaded room interior
        glBindVertexArray(roomVao);
        glUniform3f(uColorLoc, 1.0f, 1.0f, 1.0f);
        glUniform1f(uAlphaLoc, 0.65f);
        glUniform1i(uUseLightingLoc, 1);
        glUniform1i(uUseMaterialTextureLoc, 1);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glDrawArrays(GL_TRIANGLES, 0, roomVertexCount);

        // Subtle wireframe overlay for spatial depth
        const glm::vec3 wire = palette::rgb(palette::argb::roomWireframe);
        glUniform1f(uAlphaLoc, 0.45f);
        glUniform1i(uUseLightingLoc, 0);
        glUniform1i(uUseMaterialTextureLoc, 0);
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        glDrawArrays(GL_TRIANGLES, 0, roomVertexCount);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindVertexArray(0);
    }

    const bool showRayPaths = params.showRayPaths != nullptr &&
                              params.showRayPaths->load(std::memory_order_relaxed) >= 0.5f;
    if (showRayPaths)
    {
        uploadRayPathsIfNeeded();
        if (rayPathVao != 0 && rayPathVertexCount > 0)
        {
            const glm::mat4 identity(1.0f);
            glUniformMatrix4fv(uMvpLoc, 1, GL_FALSE, glm::value_ptr(vp));
            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(identity));
            const glm::vec3 rayColour = palette::rgb(palette::argb::rayPath);
            glUniform1f(uAlphaLoc, 0.9f);
            glUniform1i(uUseLightingLoc, 0);
            glUniform1i(uUseMaterialTextureLoc, 0);
            glUniform1i(uRayModeLoc, 1);
            glDisable(GL_DEPTH_TEST);
            glBindVertexArray(rayPathVao);
            const GLfloat widths[] { 1.0f, 1.5f, 2.0f };
            GLfloat supportedWidths[2] { 1.0f, 1.0f };
            glGetFloatv(GL_ALIASED_LINE_WIDTH_RANGE, supportedWidths);
            for (size_t bin = 0; bin < rayPathCounts.size(); ++bin)
            {
                if (rayPathCounts[bin] == 0)
                    continue;
                glLineWidth(std::clamp(widths[bin],
                                       supportedWidths[0],
                                       supportedWidths[1]));
                glDrawArrays(GL_LINES, rayPathFirst[bin], rayPathCounts[bin]);
            }
            glBindVertexArray(0);
            glLineWidth(1.0f);
            glEnable(GL_DEPTH_TEST);
        }
    }

    if (markerVao != 0 && markerVertexCount > 0)
    {
        glUniform1i(uRayModeLoc, 0);
        glUniform1i(uUseLightingLoc, 1);
        glUniform1i(uUseMaterialTextureLoc, 0);
        glUniform1f(uAlphaLoc, 1.0f);

        const auto drawObject = [&](const glm::vec3& position,
                                    float yaw,
                                    float pitch,
                                    const glm::vec3& markerColor,
                                    const glm::vec3& directionColor,
                                    float markerScale)
        {
            const glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
            const glm::mat4 rotation =
                glm::rotate(glm::mat4(1.0f), glm::radians(-yaw), glm::vec3(0.0f, 1.0f, 0.0f)) *
                glm::rotate(glm::mat4(1.0f), glm::radians(pitch), glm::vec3(1.0f, 0.0f, 0.0f));
            const glm::mat4 arrowModel = translation * rotation;
            const glm::mat4 markerModel = arrowModel *
                glm::scale(glm::mat4(1.0f), glm::vec3(markerScale));

            glBindVertexArray(markerVao);
            const glm::mat4 markerMvp = vp * markerModel;
            glUniformMatrix4fv(uMvpLoc, 1, GL_FALSE, glm::value_ptr(markerMvp));
            glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(markerModel));
            glUniform3f(uColorLoc, markerColor.r, markerColor.g, markerColor.b);
            glDrawArrays(GL_TRIANGLES, 0, markerVertexCount);

            if (directionVao != 0 && directionVertexCount > 0)
            {
                glBindVertexArray(directionVao);
                const glm::mat4 arrowMvp = vp * arrowModel;
                glUniformMatrix4fv(uMvpLoc, 1, GL_FALSE, glm::value_ptr(arrowMvp));
                glUniformMatrix4fv(uModelLoc, 1, GL_FALSE, glm::value_ptr(arrowModel));
                glUniform3f(uColorLoc, directionColor.r, directionColor.g, directionColor.b);
                glDrawArrays(GL_TRIANGLES, 0, directionVertexCount);
            }
        };

        const glm::vec3 sourceCenter(params.srcX != nullptr ? params.srcX->load() : -2.0f,
                                     params.srcY != nullptr ? params.srcY->load() : 1.5f,
                                     params.srcZ != nullptr ? params.srcZ->load() : 0.0f);
        const glm::vec3 receiverCenter(params.dstX != nullptr ? params.dstX->load() : 2.0f,
                                       params.dstY != nullptr ? params.dstY->load() : 1.5f,
                                       params.dstZ != nullptr ? params.dstZ->load() : 0.0f);
        const float sourceYaw = params.srcYawDeg != nullptr ? params.srcYawDeg->load() : 90.0f;
        const float sourcePitch = params.srcPitchDeg != nullptr ? params.srcPitchDeg->load() : 0.0f;
        const float receiverYaw = params.dstYawDeg != nullptr ? params.dstYawDeg->load() : -90.0f;
        const float receiverPitch = params.dstPitchDeg != nullptr ? params.dstPitchDeg->load() : 0.0f;
        const int outputType = params.outputType != nullptr
                                   ? static_cast<int>(std::round(params.outputType->load()))
                                   : static_cast<int>(OutputType::Vocal);
        const int inputType = params.inputType != nullptr
                                  ? static_cast<int>(std::round(params.inputType->load()))
                                  : static_cast<int>(InputType::Microphone);
        const glm::vec3 sourceRight = rightFromYaw(sourceYaw);
        const glm::vec3 receiverRight = rightFromYaw(receiverYaw);

        const int sourceCount = outputType == static_cast<int>(OutputType::Vocal) ? 1 : 2;
        for (int source = 0; source < sourceCount; ++source)
        {
            const float side = source == 0 ? -1.0f : 1.0f;
            const bool paChannel = outputType == static_cast<int>(OutputType::PA);
            const float offset = sourceCount == 2 && !paChannel ? 0.06f : 0.0f;
            const float yawOffset = outputType == static_cast<int>(OutputType::SpeakerBox)
                                        ? side * 30.0f : 0.0f;
            const glm::vec3 markerColor = source == 0
                                              ? palette::rgb(palette::argb::source)
                                              : palette::rgb(palette::argb::sourceAlt);
            const glm::vec3 directionColor = source == 0
                                                 ? palette::rgb(palette::argb::sourceDirection)
                                                 : palette::rgb(palette::argb::source);
            glm::vec3 position = sourceCenter + sourceRight * (side * offset);
            float yaw = sourceYaw + yawOffset;
            float pitch = sourcePitch;
            if (paChannel)
            {
                position = glm::vec3(paValue(params.paSrcX, source, 0.0f),
                                     paValue(params.paSrcY, source, 1.5f),
                                     paValue(params.paSrcZ, source, 0.0f));
                yaw = paValue(params.paSrcYawDeg, source, 90.0f);
                pitch = paValue(params.paSrcPitchDeg, source, 0.0f);
            }
            drawObject(position, yaw, pitch, markerColor,
                       directionColor, sourceCount == 1 ? 1.0f : 0.55f);
        }

        const int receiverCount = inputType == static_cast<int>(InputType::Microphone) ? 1 : 2;
        for (int receiver = 0; receiver < receiverCount; ++receiver)
        {
            const float side = receiver == 0 ? -1.0f : 1.0f;
            const bool paChannel = inputType == static_cast<int>(InputType::PA);
            const float offset = inputType == static_cast<int>(InputType::Ears) ? 0.09f
                               : receiverCount == 2 && !paChannel ? 0.15f : 0.0f;
            const float yawOffset = inputType == static_cast<int>(InputType::Ears)
                                        ? side * 10.0f : 0.0f;
            const glm::vec3 markerColor = receiver == 0
                                              ? palette::rgb(palette::argb::receiver)
                                              : palette::rgb(palette::argb::receiverAlt);
            const glm::vec3 directionColor = receiver == 0
                                                 ? palette::rgb(palette::argb::receiverDirection)
                                                 : palette::rgb(palette::argb::receiverDirectionAlt);
            glm::vec3 position = receiverCenter + receiverRight * (side * offset);
            float yaw = receiverYaw + yawOffset;
            float pitch = receiverPitch;
            if (paChannel)
            {
                position = glm::vec3(paValue(params.paDstX, receiver, 2.0f),
                                     paValue(params.paDstY, receiver, 1.5f),
                                     paValue(params.paDstZ, receiver, 0.0f));
                yaw = paValue(params.paDstYawDeg, receiver, -90.0f);
                pitch = paValue(params.paDstPitchDeg, receiver, 0.0f);
            }
            drawObject(position, yaw, pitch, markerColor,
                       directionColor, receiverCount == 1 ? 1.0f : 0.55f);
        }

        glBindVertexArray(0);
    }
}
} // namespace nowave
