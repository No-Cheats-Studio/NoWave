#include "ui/StartScreenComponent.h"

namespace nowave
{
StartScreenComponent::StartScreenComponent(FileSelectedCallback onFileSelected,
                                           PresetSelectedCallback onPresetSelected)
    : fileCallback(std::move(onFileSelected)),
      presetCallback(std::move(onPresetSelected))
{
    titleLabel.setText("NoWave", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(28.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, juce::Colour(0xfff1f5f9));
    titleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("Real-Time GPU Raytraced Acoustic Reverb", juce::dontSendNotification);
    subtitleLabel.setFont(juce::FontOptions(14.0f));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff94a3b8));
    subtitleLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(subtitleLabel);

    presetHeaderLabel.setText("Or choose a built-in room preset:", juce::dontSendNotification);
    presetHeaderLabel.setFont(juce::FontOptions(13.5f, juce::Font::bold));
    presetHeaderLabel.setColour(juce::Label::textColourId, juce::Colour(0xffcbd5e1));
    presetHeaderLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(presetHeaderLabel);

    statusLabel.setText("", juce::dontSendNotification);
    statusLabel.setFont(juce::FontOptions(13.0f));
    statusLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(statusLabel);

    for (const auto& info : RoomPresets::getAll())
    {
        auto btn = std::make_unique<juce::TextButton>(
            juce::String(info.displayName) + "\n" + juce::String(info.description));
        btn->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1e2430));
        btn->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2b3547));
        btn->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffe2e8f0));
        const RoomPresetId presetId = info.id;
        btn->onClick = [this, presetId]()
        {
            if (presetCallback)
                presetCallback(presetId);
        };
        addAndMakeVisible(*btn);
        presetButtons.push_back(std::move(btn));
    }
}

void StartScreenComponent::setStatusMessage(const juce::String& message, bool isError)
{
    statusLabel.setColour(juce::Label::textColourId,
                          isError ? juce::Colour(0xfff87171) : juce::Colour(0xff94a3b8));
    statusLabel.setText(message, juce::dontSendNotification);
}

void StartScreenComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff11141a));

    const auto dz = dropZoneBounds.toFloat();
    const juce::Colour fillCol = isDragOver ? juce::Colour(0xff1d2b3f) : juce::Colour(0xff171c24);
    const juce::Colour borderCol = isDragOver ? juce::Colour(0xff38bdf8) : juce::Colour(0xff334155);

    g.setColour(fillCol);
    g.fillRoundedRectangle(dz, 12.0f);

    g.setColour(borderCol);
    const float dashLengths[] = { 8.0f, 6.0f };
    juce::Path borderPath;
    borderPath.addRoundedRectangle(dz.reduced(1.5f), 12.0f);
    juce::Path dashedPath;
    juce::PathStrokeType(2.0f).createDashedStroke(dashedPath, borderPath, dashLengths, 2);
    g.fillPath(dashedPath);

    g.setColour(juce::Colour(0xffe2e8f0));
    g.setFont(juce::FontOptions(17.0f, juce::Font::bold));
    g.drawText("Drop .OBJ or .FBX 3D Room File Here",
               dropZoneBounds.withTrimmedBottom(28),
               juce::Justification::centred);

    g.setColour(juce::Colour(0xff94a3b8));
    g.setFont(juce::FontOptions(13.0f));
    g.drawText("or click anywhere in this box to browse",
               dropZoneBounds.withTrimmedTop(28),
               juce::Justification::centred);
}

void StartScreenComponent::resized()
{
    auto area = getLocalBounds().reduced(40, 28);

    titleLabel.setBounds(area.removeFromTop(38));
    subtitleLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(24);

    const int dropWidth = juce::jmin(580, area.getWidth());
    const int dropHeight = juce::jmin(200, juce::jmax(130, area.getHeight() - 180));
    dropZoneBounds = area.removeFromTop(dropHeight).withSizeKeepingCentre(dropWidth, dropHeight);

    area.removeFromTop(18);
    presetHeaderLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(10);

    const int numPresets = static_cast<int>(presetButtons.size());
    if (numPresets > 0)
    {
        auto row = area.removeFromTop(68).withSizeKeepingCentre(juce::jmin(660, area.getWidth()), 68);
        const int gap = 12;
        const int btnWidth = (row.getWidth() - gap * (numPresets - 1)) / numPresets;
        for (int i = 0; i < numPresets; ++i)
        {
            presetButtons[static_cast<size_t>(i)]->setBounds(row.removeFromLeft(btnWidth));
            if (i < numPresets - 1)
                row.removeFromLeft(gap);
        }
    }

    area.removeFromTop(14);
    statusLabel.setBounds(area.removeFromTop(24));
}

void StartScreenComponent::mouseUp(const juce::MouseEvent& event)
{
    if (dropZoneBounds.contains(event.getPosition()))
        openFileBrowser();
}

bool StartScreenComponent::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        if (isSupportedMeshFile(path))
            return true;
    }
    return false;
}

void StartScreenComponent::fileDragEnter(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(files, x, y);
    isDragOver = true;
    repaint();
}

void StartScreenComponent::fileDragExit(const juce::StringArray& files)
{
    juce::ignoreUnused(files);
    isDragOver = false;
    repaint();
}

void StartScreenComponent::filesDropped(const juce::StringArray& files, int x, int y)
{
    juce::ignoreUnused(x, y);
    isDragOver = false;
    repaint();

    for (const auto& path : files)
    {
        if (isSupportedMeshFile(path))
        {
            if (fileCallback)
                fileCallback(juce::File(path));
            return;
        }
    }

    setStatusMessage("Unsupported file format. Please drop a .obj or .fbx file.", true);
}

void StartScreenComponent::openFileBrowser()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Select 3D Room (.obj, .fbx)",
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory),
        "*.obj;*.fbx");

    constexpr int flags = juce::FileBrowserComponent::openMode
                        | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync(flags, [this](const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (file.existsAsFile() && fileCallback)
            fileCallback(file);
    });
}

bool StartScreenComponent::isSupportedMeshFile(const juce::String& path)
{
    return path.endsWithIgnoreCase(".obj") || path.endsWithIgnoreCase(".fbx");
}
} // namespace nowave
