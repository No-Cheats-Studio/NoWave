#pragma once

#include "parameters/ParameterLayout.h"
#include "raytracing/BVHNode.h"
#include "scene/RoomMesh.h"

#include <atomic>
#include <array>
#include <functional>
#include <glm/vec3.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <memory>
#include <mutex>
#include <vector>

namespace nowave
{
class Viewport3DComponent final : public juce::Component,
                                  public juce::OpenGLRenderer,
                                  private juce::Timer
{
public:
    using RayPathProvider =
        std::function<std::shared_ptr<const std::vector<RaySegment>>()>;

    Viewport3DComponent(const ParameterPointers& paramPointers,
                        RayPathProvider rayPathProvider,
                        std::function<void()> onChangeRoomRequested);
    ~Viewport3DComponent() override;

    void setRoomMesh(std::shared_ptr<const RoomMesh> mesh);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseDown(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void timerCallback() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

private:
    void uploadPendingMeshIfNeeded();
    void uploadRayPathsIfNeeded();
    void createMarkerGeometry();
    void createDirectionGeometry();
    void createRayGeometry();
    void destroyBuffers();

    const ParameterPointers& params;
    RayPathProvider rayPathProvider;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    GLint uMvpLoc { -1 };
    GLint uModelLoc { -1 };
    GLint uColorLoc { -1 };
    GLint uAlphaLoc { -1 };
    GLint uUseLightingLoc { -1 };
    GLint uUseMaterialTextureLoc { -1 };
    GLint uRayModeLoc { -1 };

    GLuint roomVao { 0 };
    GLuint roomVbo { 0 };
    GLsizei roomVertexCount { 0 };

    GLuint markerVao { 0 };
    GLuint markerVbo { 0 };
    GLsizei markerVertexCount { 0 };
    GLuint directionVao { 0 };
    GLuint directionVbo { 0 };
    GLsizei directionVertexCount { 0 };
    GLuint rayPathVao { 0 };
    GLuint rayPathVbo { 0 };
    GLsizei rayPathVertexCount { 0 };
    std::array<GLint, 3> rayPathFirst {};
    std::array<GLsizei, 3> rayPathCounts {};
    std::shared_ptr<const std::vector<RaySegment>> rayPathSnapshot;

    std::mutex meshMutex;
    std::shared_ptr<const RoomMesh> pendingMesh;
    bool meshDirty { false };

    std::atomic<float> cameraX { 0.0f };
    std::atomic<float> cameraY { 1.5f };
    std::atomic<float> cameraZ { 0.0f };
    std::atomic<float> yawDeg { 0.0f };
    std::atomic<float> pitchDeg { 0.0f };
    juce::Point<int> lastDragPos;
    bool cameraNavigationHeld { false };

    juce::Label roomNameLabel;
    juce::Label cameraHintLabel;
    juce::TextButton changeRoomButton { "Change Room" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Viewport3DComponent)
};
} // namespace nowave
