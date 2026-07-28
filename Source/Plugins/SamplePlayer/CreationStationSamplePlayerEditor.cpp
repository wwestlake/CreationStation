#include "CreationStationSamplePlayerEditor.h"
#include "SamplePlayerLibrarySettings.h"
#include "../../PluginKit/CreationStationPluginPalette.h"

namespace cs::plugins
{
CreationStationSamplePlayerEditor::CreationStationSamplePlayerEditor(CreationStationSamplePlayerProcessor& processorToEdit)
    : AudioProcessorEditor(&processorToEdit), processorRef(processorToEdit)
{
    setLookAndFeel(&lookAndFeel);

    addAndMakeVisible(header);

    libraryFolderButton.onClick = [this] { chooseLibraryFolder(); };
    addAndMakeVisible(libraryFolderButton);

    libraryPathLabel.setJustificationType(juce::Justification::centredLeft);
    libraryPathLabel.setColour(juce::Label::textColourId, palette::textSecondary);
    libraryPathLabel.setFont(juce::Font(12.0f));
    addAndMakeVisible(libraryPathLabel);

    catalog.setLibraryPath(SamplePlayerLibrarySettings::getLibraryPath());

    for (int layer = 0; layer < CreationStationSamplePlayerProcessor::numLayers; ++layer)
    {
        auto& row = layerRows[(size_t) layer];

        row.label.setText("Layer " + juce::String(layer + 1), juce::dontSendNotification);
        row.label.setColour(juce::Label::textColourId, palette::textSecondary);
        row.label.setFont(juce::Font(12.0f).boldened());
        addAndMakeVisible(row.label);

        addAndMakeVisible(row.packCombo);
        row.packCombo.onChange = [this, layer]
        {
            auto& thisRow = layerRows[(size_t) layer];
            auto selectedId = thisRow.packCombo.getSelectedId();
            if (selectedId <= 1)
                return; // "(None)" - leaves whatever was already loaded in this layer alone

            auto entryIndex = selectedId - 2;
            if (! juce::isPositiveAndBelow(entryIndex, catalog.getEntries().size()))
                return;

            juce::String errorMessage;
            if (! processorRef.loadLayerPack(layer, catalog.getEntries()[entryIndex].folder, errorMessage))
                thisRow.packCombo.setText("Load failed: " + errorMessage, juce::dontSendNotification);
        };

        row.gainSlider.setRange(0.0, 1.0, 0.001);
        addAndMakeVisible(row.gainSlider);
        row.gainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.apvts, CreationStationSamplePlayerProcessor::layerGainParamId(layer), row.gainSlider);
    }

    configureKnob(attackKnob, "attack", "Attack");
    configureKnob(decayKnob, "decay", "Decay");
    configureKnob(sustainKnob, "sustain", "Sustain");
    configureKnob(releaseKnob, "release", "Release");

    outputMeter.setLevelSource(&processorRef.outputLevelDb, -60.0f, 0.0f);
    addAndMakeVisible(outputMeter);

    refreshCatalogAndCombos();

    setSize(560, 420);
}

CreationStationSamplePlayerEditor::~CreationStationSamplePlayerEditor()
{
    setLookAndFeel(nullptr);
}

void CreationStationSamplePlayerEditor::configureKnob(KnobControl& knob, const juce::String& parameterId, const juce::String& labelText)
{
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 18);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, palette::textPrimary);
    addAndMakeVisible(knob.slider);

    knob.label.setText(labelText, juce::dontSendNotification);
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, palette::textSecondary);
    knob.label.setFont(juce::Font(12.0f).boldened());
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.apvts, parameterId, knob.slider);
}

void CreationStationSamplePlayerEditor::chooseLibraryFolder()
{
    activeChooser = std::make_unique<juce::FileChooser>("Choose your sample pack library folder",
                                                        catalog.getLibraryPath().exists()
                                                            ? catalog.getLibraryPath()
                                                            : juce::File::getSpecialLocation(juce::File::userMusicDirectory));

    activeChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [this](const juce::FileChooser& chooser)
                               {
                                   auto folder = chooser.getResult();
                                   activeChooser.reset();

                                   if (! folder.isDirectory())
                                       return;

                                   SamplePlayerLibrarySettings::setLibraryPath(folder);
                                   catalog.setLibraryPath(folder);
                                   refreshCatalogAndCombos();
                               });
}

void CreationStationSamplePlayerEditor::refreshCatalogAndCombos()
{
    catalog.rescan();

    libraryPathLabel.setText(catalog.getLibraryPath().exists()
                                 ? catalog.getLibraryPath().getFullPathName() + " (" + juce::String(catalog.getEntries().size()) + " packs)"
                                 : "No library folder set",
                             juce::dontSendNotification);

    for (int layer = 0; layer < CreationStationSamplePlayerProcessor::numLayers; ++layer)
        populateLayerCombo(layer);
}

void CreationStationSamplePlayerEditor::populateLayerCombo(int layerIndex)
{
    auto& row = layerRows[(size_t) layerIndex];
    row.packCombo.clear(juce::dontSendNotification);
    row.packCombo.addItem("(None)", 1);

    auto currentFolder = processorRef.getLayerPackFolder(layerIndex);
    auto selectedId = 1;

    const auto& entries = catalog.getEntries();
    for (int i = 0; i < entries.size(); ++i)
    {
        row.packCombo.addItem(entries[i].name + " (" + juce::String(entries[i].noteCount) + " notes)", i + 2);
        if (entries[i].folder == currentFolder)
            selectedId = i + 2;
    }

    row.packCombo.setSelectedId(selectedId, juce::dontSendNotification);
}

void CreationStationSamplePlayerEditor::paint(juce::Graphics& g)
{
    g.fillAll(palette::background);
}

void CreationStationSamplePlayerEditor::resized()
{
    auto area = getLocalBounds().reduced(12);
    header.setBounds(area.removeFromTop(48));
    area.removeFromTop(10);

    auto libraryRow = area.removeFromTop(28);
    libraryFolderButton.setBounds(libraryRow.removeFromLeft(150));
    libraryRow.removeFromLeft(10);
    libraryPathLabel.setBounds(libraryRow);
    area.removeFromTop(12);

    for (auto& row : layerRows)
    {
        auto rowArea = area.removeFromTop(30);
        row.label.setBounds(rowArea.removeFromLeft(60));
        rowArea.removeFromLeft(6);
        row.packCombo.setBounds(rowArea.removeFromLeft(240));
        rowArea.removeFromLeft(10);
        row.gainSlider.setBounds(rowArea);
        area.removeFromTop(6);
    }

    area.removeFromTop(10);

    auto lowerArea = area.removeFromTop(110);
    auto meterArea = lowerArea.removeFromRight(40);
    outputMeter.setBounds(meterArea);
    lowerArea.removeFromRight(12);

    auto knobWidth = lowerArea.getWidth() / 4;
    auto layoutKnob = [&](KnobControl& knob)
    {
        auto knobArea = lowerArea.removeFromLeft(knobWidth);
        knob.label.setBounds(knobArea.removeFromTop(18));
        knob.slider.setBounds(knobArea);
    };
    layoutKnob(attackKnob);
    layoutKnob(decayKnob);
    layoutKnob(sustainKnob);
    layoutKnob(releaseKnob);
}
}
