#pragma once

#include <JuceHeader.h>
#include "../Branding.h"
#include "RouterCableModel.h"
#include "RouterAudioEngine.h"
#include "RouterEndpointRegistry.h"

class RouterMainComponent final : public juce::Component
{
public:
    RouterMainComponent();

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct RouteSource
    {
        juce::String name;
        juce::String detail;
        bool selected = false;
    };

    struct RouteSink
    {
        juce::String name;
        juce::String detail;
        bool enabled = false;
    };

    class HeaderBar final : public juce::Component
    {
    public:
        HeaderBar();

        void setStatus(const juce::String& text);
        void paint(juce::Graphics&) override;
        void resized() override;

        juce::Label titleLabel;
        juce::Label subtitleLabel;
        juce::Label statusLabel;
        juce::Image logoImage;
        juce::TextButton savePresetButton { "Save Preset" };
        juce::TextButton loadPresetButton { "Load Preset" };
        juce::TextButton helpButton { "How it works" };

    private:
        juce::String statusText;
    };

    class SourcePanel final : public juce::Component
    {
    public:
        SourcePanel();

        void setSources(const juce::StringArray& names, const juce::StringArray& details);
        void selectSource(int index);
        int getSelectedSource() const noexcept { return selectedSource; }

        std::function<void(int)> onSourceSelected;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        class SourceRow final : public juce::Component
        {
        public:
            SourceRow();
            void setText(const juce::String& title, const juce::String& detail);
            void setSelected(bool shouldBeSelected);
            void paint(juce::Graphics&) override;
            void resized() override;

            std::function<void()> onSelected;
            juce::TextButton selectButton { "Use" };
            juce::Label titleLabel;
            juce::Label detailLabel;

        private:
            bool selected = false;
        };

        juce::Label headingLabel;
        juce::OwnedArray<SourceRow> rows;
        int selectedSource = 0;
    };

    class SinkPanel final : public juce::Component
    {
    public:
        SinkPanel();

        void setSinks(const juce::StringArray& names, const juce::StringArray& details);
        void setSinkEnabled(int index, bool shouldEnable);
        bool isSinkEnabled(int index) const;

        std::function<void(int, bool)> onSinkToggled;

        void paint(juce::Graphics&) override;
        void resized() override;

    private:
        class SinkCard final : public juce::Component
        {
        public:
            SinkCard();
            void setText(const juce::String& title, const juce::String& detail);
            void setEnabledState(bool shouldEnable);
            void paint(juce::Graphics&) override;
            void resized() override;

            std::function<void(bool)> onToggled;
            juce::ToggleButton enableButton { "Route" };
            juce::Label titleLabel;
            juce::Label detailLabel;

        private:
            bool enabled = false;
        };

        juce::Label headingLabel;
        juce::OwnedArray<SinkCard> cards;
    };

    class ByokPanel final : public juce::Component
    {
    public:
        ByokPanel();

        void setSummary(const juce::String& text);
        void paint(juce::Graphics&) override;
        void resized() override;

        juce::Label headingLabel;
        juce::Label subtitleLabel;
        juce::TextEditor promptEditor;
        juce::TextEditor responseEditor;
        juce::TextButton draftButton { "Draft route" };
        juce::TextButton applyButton { "Apply patch" };

    private:
        juce::String summaryText;
    };

    HeaderBar headerBar;
    juce::ComboBox inputDeviceBox;
    juce::ComboBox outputDeviceBox;
    juce::Label driverContractLabel;
    SourcePanel sourcePanel;
    SinkPanel sinkPanel;
    ByokPanel byokPanel;
    juce::Label patchSummaryLabel;
    juce::Rectangle<int> contentBounds;
    juce::String currentSummary;
    juce::String currentSourceName;
    juce::StringArray sourceNames;
    juce::StringArray sourceDetails;
    juce::StringArray sinkNames;
    juce::StringArray sinkDetails;
    int selectedSourceIndex = 0;
    RouterCableModel cableModel;
    RouterAudioEngine audioEngine;
    RouterEndpointRegistry endpointRegistry;
    juce::TextButton practicePresetButton { "Practice" };
    juce::TextButton recordPresetButton { "Record" };
    juce::TextButton streamPresetButton { "Stream" };
    juce::TextButton clearPresetButton { "Clear Route" };
    juce::Label presetStatusLabel;
    juce::String presetName;
    bool isApplyingPreset = false;

    void refreshDeviceMenus();
    void refreshSummary();
    juce::String makeSummary() const;
    void applyPreset(const juce::String& preset);
    juce::File getPresetFile() const;
    void savePreset();
    void loadPreset();
};
