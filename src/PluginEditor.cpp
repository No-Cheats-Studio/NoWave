#include "PluginEditor.h"
#include "ui/Palette.h"
#include "scene/RoomPresets.h"
#include "scene/SceneFileLoader.h"

NoWaveAudioProcessorEditor::NoWaveAudioProcessorEditor(NoWaveAudioProcessor& processor)
    : AudioProcessorEditor(&processor),
      audioProcessor(processor),
      startScreen(
          [this](const juce::File& file) { loadRoomFromFile(file); },
          [this](nowave::RoomPresetId presetId) { loadRoomPreset(presetId); }),
      viewport3D(
          processor.getParameterPointers(),
          [this]() { return audioProcessor.getRayPathsSnapshot(); },
          [this]() { showStartScreen(); }),
      bottomControlBar(processor.getAPVTS(), [this] { resized(); },
                       processor.getPreferredGpu(),
                       [this](const juce::String& gpu) { audioProcessor.setPreferredGpu(gpu); })
{
    addAndMakeVisible(startScreen);
    addChildComponent(viewport3D);
    addChildComponent(bottomControlBar);

    setResizable(true, true);
    setResizeLimits(880, 560, 1920, 1200);
    setSize(1080, 680);

    if (const auto existingMesh = audioProcessor.getRoomMesh())
        showRoomView(existingMesh);
}

void NoWaveAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(nowave::palette::background);
}

void NoWaveAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    startScreen.setBounds(area);

    const int bottomBarHeight = bottomControlBar.getPreferredHeight();
    bottomControlBar.setBounds(area.removeFromBottom(bottomBarHeight));
    viewport3D.setBounds(area);
}

void NoWaveAudioProcessorEditor::loadRoomFromFile(const juce::File& file)
{
    const auto result = nowave::SceneFileLoader::loadFromFile(file.getFullPathName().toStdString());
    if (!result.success || result.mesh == nullptr)
    {
        startScreen.setStatusMessage(
            "Failed to load room: " + juce::String(result.errorMessage), true);
        return;
    }

    audioProcessor.setRoomMesh(result.mesh, "file", file.getFullPathName());
    startScreen.setStatusMessage("", false);
    showRoomView(result.mesh);
}

void NoWaveAudioProcessorEditor::loadRoomPreset(nowave::RoomPresetId presetId)
{
    auto mesh = nowave::RoomPresets::createMesh(presetId);
    audioProcessor.setRoomMesh(mesh, "preset", nowave::RoomPresets::toKey(presetId));
    startScreen.setStatusMessage("", false);
    showRoomView(std::move(mesh));
}

void NoWaveAudioProcessorEditor::showRoomView(std::shared_ptr<const nowave::RoomMesh> mesh)
{
    isRoomLoaded = true;
    viewport3D.setRoomMesh(std::move(mesh));
    startScreen.setVisible(false);
    viewport3D.setVisible(true);
    bottomControlBar.setVisible(true);
    resized();
}

void NoWaveAudioProcessorEditor::showStartScreen()
{
    isRoomLoaded = false;
    viewport3D.setVisible(false);
    bottomControlBar.setVisible(false);
    startScreen.setVisible(true);
    resized();
}
