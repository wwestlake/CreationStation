#pragma once

#include <JuceHeader.h>
#include "../Suite/SuiteSettings.h"

class SuiteSettingsPanel final : public juce::Component
{
public:
    SuiteSettingsPanel();

    std::function<void(const juce::String& fieldId)> onBrowseRequested;
    std::function<void(const SuiteSettings& settings)> onApplyRequested;

    void setSettings(const SuiteSettings& settings);
    SuiteSettings getSettings() const;
    void setStatusText(const juce::String& text);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    struct PathRow
    {
        juce::String id;
        juce::Label label;
        juce::TextEditor editor;
        juce::TextButton browseButton { "Browse" };
    };

    void configureRow(PathRow& row, const juce::String& id, const juce::String& labelText);
    void layoutRow(PathRow& row, juce::Rectangle<int>& area);

    juce::Label titleLabel;
    juce::Label subTitleLabel;
    PathRow suiteVfsRow;
    PathRow sharedResourcesRow;
    PathRow stationProjectsRow;
    PathRow engineProjectsRow;
    PathRow movieProjectsRow;
    PathRow liveProjectsRow;
    juce::TextButton applyButton { "Apply Suite Settings" };
    juce::Label statusLabel;
};
