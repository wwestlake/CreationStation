#pragma once

#include <JuceHeader.h>

#include <functional>
#include <memory>

namespace cs
{
// The File > Import window: pick files, or a whole folder, see the list, then import them all in one batch. It only
// builds the list; MainComponent does the actual importing (the same path drag and drop uses).
class ImportPanel final : public juce::Component, private juce::ListBoxModel
{
public:
    static bool isAudioExtension(const juce::String& extension)
    {
        return extension == ".wav" || extension == ".aif" || extension == ".aiff"
            || extension == ".flac" || extension == ".mp3" || extension == ".ogg";
    }

    static bool isVideoExtension(const juce::String& extension)
    {
        return extension == ".mp4" || extension == ".mov" || extension == ".mkv"
            || extension == ".avi" || extension == ".webm" || extension == ".m4v";
    }

    static bool isImportable(const juce::File& file)
    {
        const auto extension = file.getFileExtension().toLowerCase();
        return isAudioExtension(extension) || isVideoExtension(extension);
    }

    std::function<void(const juce::StringArray&)> onImport;
    std::function<void()> onCancel;

    ImportPanel()
    {
        intro.setText("Pick the files to bring into this project, or a folder to bring in everything inside it (including its subfolders). "
                      "Originals are kept as they are.", juce::dontSendNotification);
        intro.setJustificationType(juce::Justification::topLeft);
        intro.setMinimumHorizontalScale(1.0f);
        addAndMakeVisible(intro);

        addFilesButton.onClick = [this] { chooseFiles(); };
        addFolderButton.onClick = [this] { chooseFolder(); };
        removeButton.onClick = [this] { removeSelected(); };
        clearButton.onClick = [this] { files.clear(); refresh(); };
        importButton.onClick = [this] { if (onImport) onImport(pathList()); };
        cancelButton.onClick = [this] { if (onCancel) onCancel(); };
        for (auto* button : { &addFilesButton, &addFolderButton, &removeButton, &clearButton, &importButton, &cancelButton })
            addAndMakeVisible(button);

        list.setModel(this);
        list.setRowHeight(24);
        list.setMultipleSelectionEnabled(true);
        addAndMakeVisible(list);

        summary.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(summary);

        refresh();
        setSize(640, 470);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(14);
        intro.setBounds(area.removeFromTop(44));
        area.removeFromTop(6);

        auto top = area.removeFromTop(28);
        addFilesButton.setBounds(top.removeFromLeft(120));
        top.removeFromLeft(8);
        addFolderButton.setBounds(top.removeFromLeft(120));
        clearButton.setBounds(top.removeFromRight(80));
        top.removeFromRight(8);
        removeButton.setBounds(top.removeFromRight(120));
        area.removeFromTop(8);

        auto bottom = area.removeFromBottom(30);
        importButton.setBounds(bottom.removeFromRight(120));
        bottom.removeFromRight(8);
        cancelButton.setBounds(bottom.removeFromRight(100));
        summary.setBounds(bottom);
        area.removeFromBottom(8);

        list.setBounds(area);
    }

    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff141a24)); }

private:
    void chooseFiles()
    {
        chooser = std::make_unique<juce::FileChooser>("Choose files to import", juce::File(),
                                                      "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.mp4;*.mov;*.mkv;*.avi;*.webm;*.m4v");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                                 | juce::FileBrowserComponent::canSelectMultipleItems,
                             [this](const juce::FileChooser& result)
                             {
                                 for (const auto& file : result.getResults())
                                     addFile(file);
                                 chooser.reset();
                                 refresh();
                             });
    }

    void chooseFolder()
    {
        chooser = std::make_unique<juce::FileChooser>("Choose a folder to import", juce::File(), "*", true);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                             [this](const juce::FileChooser& result)
                             {
                                 const auto folder = result.getResult();
                                 if (folder.isDirectory())
                                 {
                                     juce::Array<juce::File> found;
                                     folder.findChildFiles(found, juce::File::findFiles, true);
                                     found.sort();
                                     for (const auto& file : found)
                                         addFile(file);
                                 }
                                 chooser.reset();
                                 refresh();
                             });
    }

    void addFile(const juce::File& file)
    {
        if (! file.existsAsFile() || ! isImportable(file) || files.contains(file))
            return;
        files.add(file);
    }

    void removeSelected()
    {
        const auto selected = list.getSelectedRows();
        for (int i = selected.size() - 1; i >= 0; --i)
            files.remove(selected[i]);
        list.deselectAllRows();
        refresh();
    }

    juce::StringArray pathList() const
    {
        juce::StringArray paths;
        for (const auto& file : files)
            paths.add(file.getFullPathName());
        return paths;
    }

    void refresh()
    {
        int audio = 0, video = 0;
        juce::int64 bytes = 0;
        for (const auto& file : files)
        {
            if (isVideoExtension(file.getFileExtension().toLowerCase()))
                ++video;
            else
                ++audio;
            bytes += file.getSize();
        }

        summary.setText(files.isEmpty() ? juce::String("Nothing chosen yet.")
                                        : juce::String(audio) + " audio, " + juce::String(video) + " video  (" + juce::File::descriptionOfSizeInBytes(bytes) + ")",
                        juce::dontSendNotification);
        importButton.setEnabled(! files.isEmpty());
        removeButton.setEnabled(! files.isEmpty());
        clearButton.setEnabled(! files.isEmpty());
        list.updateContent();
        list.repaint();
    }

    int getNumRows() override { return files.size(); }

    void paintListBoxItem(int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow(row, files.size()))
            return;

        if (selected)
            g.fillAll(juce::Colour(0xff26405f));

        const auto& file = files.getReference(row);
        const auto kind = isVideoExtension(file.getFileExtension().toLowerCase()) ? "video" : "audio";
        g.setColour(juce::Colours::white);
        g.setFont(juce::Font(juce::FontOptions(14.0f)));
        g.drawText(file.getFileName(), 8, 0, width / 2 - 8, height, juce::Justification::centredLeft, true);
        g.setColour(juce::Colour(0xff8ea0b7));
        g.setFont(juce::Font(juce::FontOptions(12.0f)));
        g.drawText(juce::String(kind) + "   " + file.getParentDirectory().getFullPathName(), width / 2, 0, width / 2 - 8, height,
                   juce::Justification::centredLeft, true);
    }

    void deleteKeyPressed(int) override { removeSelected(); }

    juce::Label intro, summary;
    juce::TextButton addFilesButton { "Add Files..." }, addFolderButton { "Add Folder..." }, removeButton { "Remove Selected" },
                     clearButton { "Clear" }, importButton { "Import" }, cancelButton { "Cancel" };
    juce::ListBox list;
    juce::Array<juce::File> files;
    std::unique_ptr<juce::FileChooser> chooser;
};
}
