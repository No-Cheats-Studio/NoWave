#pragma once

#include "parameters/ParameterLayout.h"
#include "scene/RoomMesh.h"

#include <atomic>
#include <functional>
#include <glm/vec3.hpp>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_opengl/juce_opengl.h>
#include <memory>
#include <mutex>

namespace nowave
{
class Viewport3DComponent final : public juce::Component,
                                  public juce::OpenGLRenderer,
                                  private juce::Timer
{
public:
    Viewport3DComponent(const ParameterPointers& paramPointers,
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
    void createMarkerGeometry();
    void createDirectionGeometry();
    void destroyBuffers();

    const ParameterPointers& params;

    juce::OpenGLContext openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;

    GLint uMvpLoc { -1 };
    GLint uModelLoc { -1 };
    GLint uColorLoc { -1 };
    GLint uAlphaLoc { -1 };
    GLint uUseLightingLoc { -1 };
    GLint uUseMaterialTextureLoc { -1 };

    GLuint roomVao { 0 };
    GLuint roomVbo { 0 };
    GLsizei roomVertexCount { 0 };

    GLuint markerVao { 0 };
    GLuint markerVbo { 0 };
    GLsizei markerVertexCount { 0 };
    GLuint directionVao { 0 };
    GLuint directionVbo { 0 };
    GLsizei directionVertexCount { 0 };

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
