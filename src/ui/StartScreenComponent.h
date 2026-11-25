#pragma once

#include "scene/RoomPresets.h"

#include <functional>
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <vector>

namespace nowave
{
class StartScreenComponent final : public juce::Component,
                                   public juce::FileDragAndDropTarget
{
public:
    using FileSelectedCallback = std::function<void(const juce::File&)>;
    using PresetSelectedCallback = std::function<void(RoomPresetId)>;

    StartScreenComponent(FileSelectedCallback onFileSelected,
                         PresetSelectedCallback onPresetSelected);
    ~StartScreenComponent() override = default;

    void setStatusMessage(const juce::String& message, bool isError = false);

    void paint(juce::Graphics& g) override;
    void resized() override;

    void mouseUp(const juce::MouseEvent& event) override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    void openFileBrowser();
    static bool isSupportedMeshFile(const juce::String& path);

    FileSelectedCallback fileCallback;
    PresetSelectedCallback presetCallback;

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label presetHeaderLabel;
    juce::Label statusLabel;

    juce::Rectangle<int> dropZoneBounds;
    bool isDragOver { false };

    juce::TextButton browseButton;
    std::vector<std::unique_ptr<juce::TextButton>> presetButtons;
    std::unique_ptr<juce::FileChooser> fileChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StartScreenComponent)
};
} // namespace nowave