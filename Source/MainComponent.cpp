#include <creation/assets/ProjectManifest.h>
#include <creation/assets/AssetMaterializer.h>
#include <creation/assets/AssetTypes.h>
#include "MainComponent.h"
#include "Views/ImportPanel.h"
#include "Views/VideoClipSettingsPanel.h"
#include "Audio/PatchRuntimePlayer.h"
#include "Branding.h"
#include "Patch/PatchModel.h"
#include "Video/VideoDecodeService.h"
#include <creation/assets/VfsEntryInputStream.h>
#include <creation/assets/ProjectContainerService.h>
#include <creation/assets/ProjectWorkspaceService.h>
#include <creation/suite/SuiteStoragePaths.h>
#include <creation/ui/CreationSuiteLogos.h>
#include "BuiltInFrustData.h"
#include <creation/services/SuiteAiProviderRuntime.h>
#include "Tutorial/TutorialScriptCompiler.h"
#include <creation/services/SuiteAiSettings.h>
#include <creation/services/SuiteVfsJsonStore.h>
#include <creation/services/SuiteVfsServiceClient.h>
#include <creation/ui/ControlSurfaceActionIds.h>
#include <creation/ui/CreationSuiteLogos.h>
#include <atomic>
#include <thread>

#if JUCE_WINDOWS
 #include <windows.h>
#endif

namespace
{
// Pumps the OS message queue in small slices until `done` is set -- keeps
// the window painting/responsive (and the splash's forced repaints actually
// visible) while a background thread does real, potentially slow work,
// instead of the message thread just blocking outright. Ported from
// SuiteJUCEApplication.cpp's identical pumpStartupPaintMessages: this
// build has JUCE_MODAL_LOOPS_PERMITTED off, so
// MessageManager::runDispatchLoopUntil isn't available.
void pumpMessagesWhile(const std::atomic<bool>& done)
{
   #if JUCE_WINDOWS
    while (! done.load(std::memory_order_acquire))
    {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE) != 0)
        {
            TranslateMessage(&message);
            DispatchMessage(&message);
        }
        juce::Thread::sleep(1);
    }
   #else
    while (! done.load(std::memory_order_acquire))
        juce::Thread::sleep(5);
   #endif
}
}

namespace
{
class NonOwningPanelHost final : public juce::Component
{
public:
    explicit NonOwningPanelHost(juce::Component& contentToHost) : content(contentToHost)
    {
        addAndMakeVisible(content);
    }

    void resized() override
    {
        content.setBounds(getLocalBounds());
    }

private:
    juce::Component& content;
};

constexpr int menuIdProjectFirst = 1000;
constexpr int menuIdToolTracker = 2001;
constexpr int menuIdToolSampler = 2002;
constexpr int menuIdToolSignal = 2003;
constexpr int menuIdToolLayers = 2004;
constexpr int menuIdToolPlugins = 2005;
constexpr int menuIdToolScript = 2007;
constexpr int menuIdToolCapture = 2008;
constexpr int menuIdToolScore = 2009;
constexpr int menuIdToolSettings = 2010;
constexpr int menuIdToolFoley = 2011;
constexpr int menuIdToolVideo = 2014;
constexpr int menuIdToolVirtualEngineer = 2012;
constexpr int menuIdToolTrackInsert = 2013;
constexpr int menuIdToolResetLayout = 2099;
constexpr int menuIdFileSave = 1;
constexpr int menuIdFileSaveArrangement = 2;
constexpr int menuIdFileLoadArrangement = 3;
constexpr int menuIdFileRender = 4;
constexpr int menuIdFileExportWav = 5;
constexpr int menuIdFileNewSignal = 6;
constexpr int menuIdFileOpenSignal = 7;
constexpr int menuIdFileSaveSignal = 8;
constexpr int menuIdFileRenderSignal = 9;
constexpr int menuIdFileNewArrangement = 10;
constexpr int menuIdFileImport = 11;
constexpr int menuIdEditUndo = 101;
constexpr int menuIdEditRedo = 102;
constexpr int menuIdEditDuplicate = 103;
constexpr int menuIdEditDelete = 104;
constexpr int menuIdEditRename = 105;
constexpr int menuIdEditSplit = 106;
constexpr int menuIdHelpTour = 3001;
constexpr int menuIdHelpResetLayout = 3002;
constexpr int menuIdHelpFeedback = 3003;
constexpr int menuIdHelpContents = 3004;

const char* trackerPanelId = "tracker";
const char* samplerPanelId = "sampler";
const char* signalPanelId = "signal";
const char* layersPanelId = "layers";
const char* pluginsPanelId = "plugins";
const char* scriptPanelId = "script";
const char* capturePanelId = "capture";
const char* scorePanelId = "score";
const char* settingsPanelId = "settings";
const char* foleyPanelId = "foley";
const char* videoPanelId = "video";
const char* virtualEngineerPanelId = "virtual-engineer";
const char* trackInsertPanelId = "track-insert";

juce::String trimProjectLabelPrefix(const juce::String& label)
{
    constexpr auto prefix = "Project:";
    auto trimmed = label.trim();
    if (trimmed.startsWithIgnoreCase(prefix))
        trimmed = trimmed.fromFirstOccurrenceOf(prefix, false, false).trim();
    return trimmed;
}

juce::String slugForProjectAssetName(const juce::String& name)
{
    auto slug = name.trim().toLowerCase();
    slug = slug.retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    return slug.isNotEmpty() ? slug : "signal-asset";
}

juce::String makeDisplayProjectLabel(const juce::String& rawName)
{
    auto trimmed = trimProjectLabelPrefix(rawName);
    return trimmed.isNotEmpty() ? trimmed : "No project open";
}

void drawHeaderActionIcon(juce::Graphics& g,
                          juce::Rectangle<float> bounds,
                          const juce::String& iconName,
                          juce::Colour colour)
{
    auto centre = bounds.getCentre();
    auto size = juce::jmin(bounds.getWidth(), bounds.getHeight());
    g.setColour(colour);

    if (iconName == "plus-circle" || iconName == "plus")
    {
        auto circleBounds = juce::Rectangle<float>(centre.x - size * 0.42f, centre.y - size * 0.42f, size * 0.84f, size * 0.84f);
        g.setColour(colour.withAlpha(0.20f));
        g.fillEllipse(circleBounds);
        g.setColour(colour);
        g.drawEllipse(circleBounds, 2.0f);

        auto armLength = size * 0.22f;
        g.drawLine(centre.x - armLength, centre.y, centre.x + armLength, centre.y, 2.4f);
        g.drawLine(centre.x, centre.y - armLength, centre.x, centre.y + armLength, 2.4f);
        return;
    }

    if (iconName == "folder")
    {
        juce::Path folder;
        folder.startNewSubPath(bounds.getX() + size * 0.10f, bounds.getY() + size * 0.34f);
        folder.lineTo(bounds.getX() + size * 0.30f, bounds.getY() + size * 0.34f);
        folder.lineTo(bounds.getX() + size * 0.40f, bounds.getY() + size * 0.18f);
        folder.lineTo(bounds.getRight() - size * 0.08f, bounds.getY() + size * 0.18f);
        folder.lineTo(bounds.getRight() - size * 0.08f, bounds.getBottom() - size * 0.14f);
        folder.lineTo(bounds.getX() + size * 0.10f, bounds.getBottom() - size * 0.14f);
        folder.closeSubPath();
        g.strokePath(folder, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }

    if (iconName == "speaker")
    {
        juce::Path speaker;
        speaker.startNewSubPath(bounds.getX() + size * 0.12f, centre.y - size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.28f, centre.y - size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.44f, centre.y - size * 0.32f);
        speaker.lineTo(bounds.getX() + size * 0.44f, centre.y + size * 0.32f);
        speaker.lineTo(bounds.getX() + size * 0.28f, centre.y + size * 0.14f);
        speaker.lineTo(bounds.getX() + size * 0.12f, centre.y + size * 0.14f);
        speaker.closeSubPath();
        g.strokePath(speaker, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        for (auto radiusScale : { 0.20f, 0.34f })
        {
            juce::Path wave;
            auto radius = size * radiusScale;
            wave.addCentredArc(bounds.getX() + size * 0.46f,
                               centre.y,
                               radius,
                               radius,
                               0.0f,
                               -juce::MathConstants<float>::pi * 0.35f,
                               juce::MathConstants<float>::pi * 0.35f,
                               true);
            g.strokePath(wave, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        return;
    }

    if (iconName == "gear")
    {
        const auto radius = size * 0.24f;
        juce::Path gearPath;
        const int numTeeth = 8;
        for (int i = 0; i < numTeeth; ++i)
        {
            const float angle = i * juce::MathConstants<float>::twoPi / numTeeth;
            const float outerR = radius * 1.35f;
            const float innerR = radius * 0.88f;

            juce::Path tooth;
            tooth.addRectangle(-radius * 0.16f, -outerR, radius * 0.32f, outerR - innerR);
            juce::AffineTransform transform = juce::AffineTransform::rotation(angle).translated(centre.x, centre.y);
            gearPath.addPath(tooth, transform);
        }

        gearPath.addEllipse(centre.x - radius, centre.y - radius, radius * 2.0f, radius * 2.0f);
        g.fillPath(gearPath);

        juce::Path holePath;
        holePath.addEllipse(centre.x - radius * 0.40f, centre.y - radius * 0.40f, radius * 0.80f, radius * 0.80f);
        g.setColour(juce::Colour(0xff17222c));
        g.fillPath(holePath);
        return;
    }

    if (iconName == "chevron-down")
    {
        juce::Path chevron;
        chevron.startNewSubPath(centre.x - size * 0.20f, centre.y - size * 0.08f);
        chevron.lineTo(centre.x, centre.y + size * 0.12f);
        chevron.lineTo(centre.x + size * 0.20f, centre.y - size * 0.08f);
        g.strokePath(chevron, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        return;
    }
}

AiProviderSettings makeAiProviderSettings(const creation::services::SuiteAiResolvedRuntimeSettings& runtimeSettings)
{
    AiProviderSettings settings;
    settings.providerDisplayName = runtimeSettings.providerDisplayName.isNotEmpty()
                                       ? runtimeSettings.providerDisplayName
                                       : creation::services::SuiteAiProviderRuntime::resolveProfile(runtimeSettings.providerId).displayName;
    settings.providerId = runtimeSettings.providerId;
    settings.baseUrl = runtimeSettings.baseUrl;
    settings.modelName = runtimeSettings.modelName;
    settings.apiKey = runtimeSettings.apiKey;
    return settings;
}

CreationSuiteHeaderBar::ProfileData makeHeaderProfile(const creation::ui::SuiteDesktopAuthSession::SessionData& session)
{
    CreationSuiteHeaderBar::ProfileData profile;
    profile.displayName = session.user.displayName.isNotEmpty() ? session.user.displayName : session.user.email;

    auto tierId = branding::getBestPatreonTierId(session.user.entitlements);
    auto tierName = branding::getPatreonTierDisplayName(tierId);
    profile.detailText = tierName.isNotEmpty() ? session.user.email + "  |  " + tierName
                                               : session.user.email;
    profile.badgeImage = branding::createPatreonBadgeImage(tierId, 36);
    return profile;
}





namespace {
// ---- What an asset is, in words: used by the Add Clip picker. Details come from the asset's own recorded facts.
juce::String formatAssetDuration(double seconds)
{
    if (seconds <= 0.0)
        return {};

    const auto whole = (int) std::round(seconds);
    const auto hours = whole / 3600;
    const auto minutes = (whole / 60) % 60;
    const auto secs = whole % 60;
    if (hours > 0)
        return juce::String::formatted("%d:%02d:%02d", hours, minutes, secs);
    if (seconds < 10.0)
        return juce::String(seconds, 1) + " s";
    return juce::String::formatted("%d:%02d", minutes, secs);
}

juce::String describeAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::StringArray parts;
    const auto& d = asset.details;
    const auto duration = formatAssetDuration(d["durationSeconds"].getDoubleValue());

    switch (asset.kind)
    {
        case creation::assets::AssetKind::video:
            parts.add("Video");
            if (duration.isNotEmpty()) parts.add(duration);
            if (d["width"].getIntValue() > 0) parts.add(d["width"] + juce::String(juce::CharPointer_UTF8("\xc3\x97")) + d["height"]);
            if (d["frameRate"].getDoubleValue() > 0.0) parts.add(juce::String(d["frameRate"].getDoubleValue(), 0) + " fps");
            if (d["hasAudio"] == "0") parts.add("no sound");
            break;
        case creation::assets::AssetKind::audio:
        case creation::assets::AssetKind::render:
            parts.add(asset.kind == creation::assets::AssetKind::render ? "Render" : "Audio");
            if (duration.isNotEmpty()) parts.add(duration);
            if (d["channels"].getIntValue() > 0) parts.add(d["channels"].getIntValue() == 1 ? "mono" : d["channels"].getIntValue() == 2 ? "stereo" : d["channels"] + " ch");
            if (d["sampleRate"].getDoubleValue() > 0.0) parts.add(juce::String(d["sampleRate"].getDoubleValue() / 1000.0, 1) + " kHz");
            break;
        case creation::assets::AssetKind::patch:
            parts.add("Signal patch");
            if (d["variables"].isNotEmpty())
                parts.add(d["variables"] + (d["variables"].getIntValue() == 1 ? " public variable" : " public variables"));
            if (duration.isNotEmpty()) parts.add(duration);
            break;
        default:
            parts.add(creation::assets::toDisplayName(asset.kind));
            break;
    }

    if (asset.fileSizeBytes > 0)
        parts.add(juce::File::descriptionOfSizeInBytes((juce::int64) asset.fileSizeBytes));

    return parts.joinIntoString("  " + juce::String(juce::CharPointer_UTF8("\xc2\xb7")) + "  ");
}

// A small picture of the first stretch of a video, for the picker.
bool encodeThumbnailJpeg(const juce::Image& image, juce::MemoryBlock& out)
{
    if (! image.isValid())
        return false;

    juce::JPEGImageFormat format;
    format.setQuality(0.8f);
    juce::MemoryOutputStream stream(out, false);
    return format.writeImageToStream(image, stream);
}

struct PickerRow
{
    creation::assets::AssetDescriptor asset;
    juce::Image thumbnail;
    juce::String facts;
};

class AssetPickerDialog : public juce::DocumentWindow
{
public:
    AssetPickerDialog(const juce::String& trackDescription,
                      juce::Array<PickerRow> fittingRows,
                      juce::Array<PickerRow> otherRows,
                      std::function<void(const creation::assets::AssetDescriptor&)> onSelected)
        : juce::DocumentWindow("Add to " + trackDescription, juce::Colour(0xff11151c), juce::DocumentWindow::closeButton),
          fitting_(std::move(fittingRows)), others_(std::move(otherRows)), onSelected_(std::move(onSelected)), trackDescription_(trackDescription)
    {
        setUsingNativeTitleBar(true);
        setResizable(true, false);
        setResizeLimits(460, 360, 1100, 900);

        mainPanel_ = std::make_unique<juce::Component>();
        setContentOwned(mainPanel_.get(), false);

        searchBox_.setTextToShowWhenEmpty("Search by name...", juce::Colours::lightgrey);
        searchBox_.onTextChange = [this] { filterList(); };
        mainPanel_->addAndMakeVisible(searchBox_);

        showAllToggle_.setButtonText("Also show items that do not fit this track");
        showAllToggle_.onClick = [this] { filterList(); };
        showAllToggle_.setVisible(! others_.isEmpty());
        mainPanel_->addAndMakeVisible(showAllToggle_);

        emptyLabel_.setJustificationType(juce::Justification::centred);
        emptyLabel_.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
        emptyLabel_.setMinimumHorizontalScale(1.0f);
        mainPanel_->addChildComponent(emptyLabel_);

        listBox_.setModel(&model_);
        listBox_.setRowHeight(78);
        listBox_.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0e1218));
        mainPanel_->addAndMakeVisible(listBox_);

        addButton_.setButtonText("Add");
        addButton_.setEnabled(false);
        addButton_.onClick = [this] { chooseSelected(); };
        mainPanel_->addAndMakeVisible(addButton_);

        cancelButton_.setButtonText("Cancel");
        cancelButton_.onClick = [this] { closeButtonPressed(); };
        mainPanel_->addAndMakeVisible(cancelButton_);

        filterList();
        centreWithSize(560, 560);
        setVisible(true);
    }

    void closeButtonPressed() override { delete this; }

    void resized() override
    {
        juce::DocumentWindow::resized();
        if (mainPanel_ == nullptr)
            return;

        auto bounds = mainPanel_->getLocalBounds();
        searchBox_.setBounds(bounds.removeFromTop(34).reduced(6, 4));
        if (showAllToggle_.isVisible())
            showAllToggle_.setBounds(bounds.removeFromTop(26).reduced(8, 2));

        auto bottom = bounds.removeFromBottom(44);
        cancelButton_.setBounds(bottom.removeFromRight(100).reduced(4, 6));
        addButton_.setBounds(bottom.removeFromRight(100).reduced(4, 6));

        listBox_.setBounds(bounds);
        emptyLabel_.setBounds(bounds.reduced(24));
    }

private:
    void chooseSelected()
    {
        const auto row = listBox_.getSelectedRow();
        if (juce::isPositiveAndBelow(row, visible_.size()) && onSelected_)
        {
            auto chosen = visible_.getReference(row).asset;
            auto callback = onSelected_;
            closeButtonPressed(); // `this` is gone after this line
            callback(chosen);
        }
    }

    void filterList()
    {
        visible_.clear();
        const auto query = searchBox_.getText().trim().toLowerCase();
        const auto matches = [&query](const PickerRow& row)
        {
            return query.isEmpty() || row.asset.displayName.toLowerCase().contains(query) || row.facts.toLowerCase().contains(query);
        };

        for (const auto& row : fitting_)
            if (matches(row))
                visible_.add(row);

        if (showAllToggle_.getToggleState())
            for (const auto& row : others_)
                if (matches(row))
                    visible_.add(row);

        listBox_.updateContent();
        listBox_.repaint();

        const auto nothing = visible_.isEmpty();
        emptyLabel_.setVisible(nothing);
        if (nothing)
            emptyLabel_.setText(fitting_.isEmpty() && ! showAllToggle_.getToggleState()
                                    ? "Nothing in this project fits a " + trackDescription_ + " yet.\nImport a file, or tick the box above to see everything."
                                    : "Nothing matches your search.",
                                juce::dontSendNotification);
        addButton_.setEnabled(listBox_.getSelectedRow() >= 0 && listBox_.getSelectedRow() < visible_.size());
    }

    static void drawKindGlyph(juce::Graphics& g, juce::Rectangle<float> tile, creation::assets::AssetKind kind)
    {
        g.setColour(juce::Colour(0xff1a2432));
        g.fillRoundedRectangle(tile, 6.0f);

        const auto accent = kind == creation::assets::AssetKind::video ? juce::Colour(0xff5da5ff)
                          : kind == creation::assets::AssetKind::patch ? juce::Colour(0xffb185ff)
                          : kind == creation::assets::AssetKind::render ? juce::Colour(0xffffc857)
                                                                        : juce::Colour(0xff67e8a5);
        g.setColour(accent);
        const auto c = tile.getCentre();
        const auto s = juce::jmin(tile.getWidth(), tile.getHeight()) * 0.5f;

        if (kind == creation::assets::AssetKind::video)
        {
            juce::Path p;
            p.addTriangle(c.x - s * 0.35f, c.y - s * 0.5f, c.x - s * 0.35f, c.y + s * 0.5f, c.x + s * 0.55f, c.y);
            g.fillPath(p);
        }
        else if (kind == creation::assets::AssetKind::patch)
        {
            const juce::Point<float> a(c.x - s * 0.7f, c.y + s * 0.3f), b(c.x, c.y - s * 0.4f), d(c.x + s * 0.7f, c.y + s * 0.3f);
            g.drawLine({ a, b }, 2.0f);
            g.drawLine({ b, d }, 2.0f);
            for (auto pt : { a, b, d })
                g.fillEllipse(pt.x - 5.0f, pt.y - 5.0f, 10.0f, 10.0f);
        }
        else
        {
            static const float heights[] = { 0.35f, 0.8f, 0.55f, 1.0f, 0.45f, 0.7f, 0.3f };
            const auto barW = s * 0.18f;
            for (int i = 0; i < 7; ++i)
            {
                const auto h = s * heights[i];
                g.fillRoundedRectangle(c.x - s * 0.75f + (float) i * (barW + s * 0.06f), c.y - h * 0.5f, barW, h, 1.5f);
            }
        }
    }

    struct Model : public juce::ListBoxModel
    {
        explicit Model(AssetPickerDialog* o) : owner(o) {}
        AssetPickerDialog* owner;

        int getNumRows() override { return owner->visible_.size(); }

        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (! juce::isPositiveAndBelow(rowNumber, owner->visible_.size()))
                return;

            const auto& row = owner->visible_.getReference(rowNumber);
            g.fillAll(rowIsSelected ? juce::Colour(0xff293d5a) : (rowNumber % 2 == 0 ? juce::Colour(0xff10151c) : juce::Colour(0xff0e1218)));

            auto content = juce::Rectangle<int>(0, 0, width, height).reduced(10, 7);
            const auto tile = content.removeFromLeft(112).toFloat();
            content.removeFromLeft(12);

            if (row.thumbnail.isValid())
            {
                g.setColour(juce::Colours::black);
                g.fillRoundedRectangle(tile, 6.0f);
                g.drawImage(row.thumbnail, tile.reduced(1.0f), juce::RectanglePlacement::centred);
            }
            else
            {
                drawKindGlyph(g, tile, row.asset.kind);
            }

            g.setColour(juce::Colours::white);
            g.setFont(juce::Font(juce::FontOptions(15.0f, juce::Font::bold)));
            g.drawText(row.asset.displayName, content.removeFromTop(24), juce::Justification::centredLeft, true);

            g.setColour(juce::Colour(0xff9fb3cc));
            g.setFont(juce::Font(juce::FontOptions(12.5f)));
            g.drawText(row.facts, content.removeFromTop(20), juce::Justification::centredLeft, true);

            if (row.asset.createdAt.toMilliseconds() > 0)
            {
                g.setColour(juce::Colour(0xff657690));
                g.setFont(juce::Font(juce::FontOptions(11.5f)));
                g.drawText("Imported " + row.asset.createdAt.formatted("%Y-%m-%d"), content.removeFromTop(18), juce::Justification::centredLeft, true);
            }
        }

        void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override
        {
            owner->listBox_.selectRow(row);
            owner->chooseSelected();
        }

        void returnKeyPressed(int) override { owner->chooseSelected(); }

        void selectedRowsChanged(int lastRowSelected) override
        {
            owner->addButton_.setEnabled(lastRowSelected >= 0 && lastRowSelected < owner->visible_.size());
        }
    };

    std::unique_ptr<juce::Component> mainPanel_;
    juce::TextEditor searchBox_;
    juce::ToggleButton showAllToggle_;
    juce::Label emptyLabel_;
    juce::TextButton addButton_;
    juce::TextButton cancelButton_;
    Model model_ { this };
    juce::ListBox listBox_;
    juce::Array<PickerRow> fitting_;
    juce::Array<PickerRow> others_;
    juce::Array<PickerRow> visible_;
    std::function<void(const creation::assets::AssetDescriptor&)> onSelected_;
    juce::String trackDescription_;
};
}

class ManagedDocumentWindow final : public juce::DocumentWindow
{
public:
    ManagedDocumentWindow(const juce::String& title,
                          juce::Colour backgroundColour,
                          int requiredButtons,
                          std::function<void()> onCloseCallback)
        : juce::DocumentWindow(title, backgroundColour, requiredButtons),
          onClose(std::move(onCloseCallback))
    {
    }

    void closeButtonPressed() override
    {
        setVisible(false);
        auto closeCallback = onClose;
        juce::MessageManager::callAsync([closeCallback]() mutable
        {
            if (closeCallback)
                closeCallback();
        });
    }

private:
    std::function<void()> onClose;
};

// Right-click a transport button -> Learn MIDI Binding. Opens armed by default (Any Device) so
// the common case is just "wiggle the hardware and it's bound" - manual entry is the fallback.
class MidiLearnPanel final : public juce::Component,
                             private juce::Timer
{
public:
    struct ExistingBinding
    {
        bool found = false;
        juce::String deviceLabel;
        int channel = 0;
        int number = 0;
        bool isController = false;
    };

    MidiLearnPanel(WorkstationAudioEngine& engineRef, juce::String targetIdIn, const juce::String& displayLabel,
                   const ExistingBinding& existing,
                   WorkstationAudioEngine::MidiLearnKind expectedKindIn = WorkstationAudioEngine::MidiLearnKind::Any)
        : engine(engineRef), targetId(std::move(targetIdIn)), expectedKind(expectedKindIn)
    {
        titleLabel.setText("Learn MIDI Binding: " + displayLabel, juce::dontSendNotification);
        titleLabel.setFont(juce::Font(16.0f).boldened());
        titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
        addAndMakeVisible(titleLabel);

        instructionsLabel.setText(
            "Move a knob or press a button on your controller now - it will be captured "
            "automatically and saved right away. Or, if you already know the values, type them "
            "in below and click \"Save Typed Values\" instead.",
            juce::dontSendNotification);
        instructionsLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        instructionsLabel.setJustificationType(juce::Justification::topLeft);
        addAndMakeVisible(instructionsLabel);

        if (existing.found)
        {
            currentBindingLabel.setText("Currently bound to: " + existing.deviceLabel + ", channel "
                                        + juce::String(existing.channel) + ", "
                                        + (existing.number < 0 ? juce::String("Fader") : (existing.isController ? juce::String("CC ") : juce::String("Note ")) + juce::String(existing.number)),
                                        juce::dontSendNotification);
            currentBindingLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
            addAndMakeVisible(currentBindingLabel);
        }

        deviceLabel.setText("Listen to", juce::dontSendNotification);
        deviceLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(deviceLabel);

        deviceCombo.addItem("Any Device", 1);
        auto itemId = 2;
        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            deviceCombo.addItem(device.name, itemId);
            deviceIdsByItemId[itemId] = device.identifier;
            ++itemId;
        }
        deviceCombo.setSelectedId(1, juce::dontSendNotification);
        deviceCombo.onChange = [this] { armLearn(); };
        deviceCombo.setTooltip("Restrict listening to one device, or Any Device to accept from all");
        addAndMakeVisible(deviceCombo);

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5f93ff));
        statusLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(statusLabel);

        manualHintLabel.setText("Manual entry (only needed if auto-capture above doesn't work):",
                                juce::dontSendNotification);
        manualHintLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71839b));
        addAndMakeVisible(manualHintLabel);

        channelLabel.setText("Channel (1-16)", juce::dontSendNotification);
        channelLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(channelLabel);
        channelEditor.setInputRestrictions(2, "0123456789");
        addAndMakeVisible(channelEditor);

        numberLabel.setText("Note/CC # (0-127)", juce::dontSendNotification);
        numberLabel.setColour(juce::Label::textColourId, juce::Colour(0xffaebbd0));
        addAndMakeVisible(numberLabel);
        numberEditor.setInputRestrictions(3, "0123456789");
        addAndMakeVisible(numberEditor);

        if (existing.found)
        {
            channelEditor.setText(juce::String(existing.channel), juce::dontSendNotification);
            numberEditor.setText(juce::String(existing.number), juce::dontSendNotification);
        }

        isCCToggle.setButtonText("The number above is a CC (Control Change), not a Note");
        isCCToggle.setColour(juce::ToggleButton::textColourId, juce::Colour(0xffaebbd0));
        isCCToggle.setToggleState(existing.isController, juce::dontSendNotification);
        isCCToggle.setTooltip("Check this if the number above is a CC (Control Change) number, not a Note number");
        addAndMakeVisible(isCCToggle);

        applyManualButton.setButtonText("Save Typed Values");
        applyManualButton.onClick = [this] { applyManualEntry(); };
        applyManualButton.setTooltip("Save the channel/number typed in above as the binding");
        addAndMakeVisible(applyManualButton);

        closeButton.setButtonText("Close");
        closeButton.onClick = [this] { if (onCancelled) onCancelled(); };
        closeButton.setTooltip("Close this dialog");
        addAndMakeVisible(closeButton);

        armLearn();
        startTimer(60);
    }

    ~MidiLearnPanel() override
    {
        engine.cancelMidiLearn();
    }

    // Fired every time something is captured/saved - the panel stays open afterward so the user
    // can see what was captured and try again if it's wrong. Only the Close button (or the
    // window's own close control) actually dismisses the dialog.
    std::function<void(juce::String deviceId, int channel, int number, bool isCC)> onLearned;
    std::function<void()> onCancelled;

    void resized() override
    {
        auto area = getLocalBounds().reduced(16);
        titleLabel.setBounds(area.removeFromTop(28));
        area.removeFromTop(6);

        instructionsLabel.setBounds(area.removeFromTop(56));
        area.removeFromTop(8);

        if (currentBindingLabel.isVisible())
        {
            currentBindingLabel.setBounds(area.removeFromTop(20));
            area.removeFromTop(8);
        }

        auto deviceRow = area.removeFromTop(28);
        deviceLabel.setBounds(deviceRow.removeFromLeft(80));
        deviceCombo.setBounds(deviceRow);
        area.removeFromTop(8);

        statusLabel.setBounds(area.removeFromTop(36));
        area.removeFromTop(12);

        manualHintLabel.setBounds(area.removeFromTop(20));
        area.removeFromTop(6);

        auto manualRow = area.removeFromTop(28);
        channelLabel.setBounds(manualRow.removeFromLeft(90));
        channelEditor.setBounds(manualRow.removeFromLeft(50));
        manualRow.removeFromLeft(10);
        numberLabel.setBounds(manualRow.removeFromLeft(110));
        numberEditor.setBounds(manualRow.removeFromLeft(50));
        area.removeFromTop(8);

        isCCToggle.setBounds(area.removeFromTop(24));
        area.removeFromTop(10);

        auto buttonRow = area.removeFromTop(32);
        applyManualButton.setBounds(buttonRow.removeFromLeft(160));
        buttonRow.removeFromLeft(10);
        closeButton.setBounds(buttonRow.removeFromLeft(100));
    }

private:
    void armLearn()
    {
        auto selectedId = deviceCombo.getSelectedId();
        juce::String deviceIdFilter;
        if (selectedId > 1)
        {
            auto it = deviceIdsByItemId.find(selectedId);
            if (it != deviceIdsByItemId.end())
                deviceIdFilter = it->second;
        }
        engine.armMidiLearn(deviceIdFilter, expectedKind);
        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff5f93ff));
        statusLabel.setText("Listening - move or press the control now...", juce::dontSendNotification);
    }

    void applyManualEntry()
    {
        auto channel = channelEditor.getText().getIntValue();
        auto number = numberEditor.getText().getIntValue();
        if (channel < 1 || channel > 16 || number < 0 || number > 127)
        {
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffffb0a8));
            statusLabel.setText("Enter a channel 1-16 and a number 0-127.", juce::dontSendNotification);
            return;
        }

        statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7fe8a0));
        statusLabel.setText("Saved: channel " + juce::String(channel) + ", "
                            + (isCCToggle.getToggleState() ? "CC " : "Note ") + juce::String(number),
                            juce::dontSendNotification);

        if (onLearned)
            onLearned({}, channel, number, isCCToggle.getToggleState());
    }

    void timerCallback() override
    {
        WorkstationAudioEngine::MidiLearnResult result;
        if (engine.takeMidiLearnResult(result))
        {
            channelEditor.setText(juce::String(result.channel), juce::dontSendNotification);
            numberEditor.setText(juce::String(result.number), juce::dontSendNotification);
            isCCToggle.setToggleState(result.isController, juce::dontSendNotification);

            // number == -1 marks a captured fader (pitch wheel) rather than a CC/Note -- see
            // WorkstationAudioEngine::handleIncomingMidiMessage. "CC -1" would be a nonsensical
            // readout for what a user just physically moved.
            statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xff7fe8a0));
            statusLabel.setText("Captured & saved: channel " + juce::String(result.channel) + ", "
                                + (result.number < 0 ? juce::String("Fader") : (result.isController ? juce::String("CC ") : juce::String("Note ")) + juce::String(result.number)),
                                juce::dontSendNotification);

            if (onLearned)
                onLearned(result.deviceId, result.channel, result.number, result.isController);
        }
    }

    WorkstationAudioEngine& engine;
    juce::String targetId;
    WorkstationAudioEngine::MidiLearnKind expectedKind = WorkstationAudioEngine::MidiLearnKind::Any;
    std::map<int, juce::String> deviceIdsByItemId;

    juce::Label titleLabel, instructionsLabel, currentBindingLabel, deviceLabel, statusLabel,
               manualHintLabel, channelLabel, numberLabel;
    juce::ComboBox deviceCombo;
    juce::TextEditor channelEditor, numberEditor;
    juce::ToggleButton isCCToggle;
    juce::TextButton applyManualButton, closeButton;
};

// Encodes a rendered buffer as a WAV in memory: 16/24-bit PCM or 32-bit float. Reducing to a fixed-point depth
// can apply TPDF dither (the low-level noise that keeps quantization from turning into distortion on quiet
// material). The dither uses a fixed seed, so the same render always gives the same file.
bool encodeWavToMemory(const juce::AudioBuffer<float>& source,
                       double sampleRate,
                       int bitsPerSample,
                       bool dither,
                       juce::MemoryBlock& encoded,
                       juce::String& errorMessage)
{
    if (source.getNumChannels() <= 0 || source.getNumSamples() <= 0)
    {
        errorMessage = "There is no audio to save.";
        return false;
    }

    juce::AudioBuffer<float> work(source);
    if (dither && bitsPerSample < 32)
    {
        const float lsb = 1.0f / (float) (1 << (bitsPerSample - 1));
        juce::Random random(0x5eed);
        for (int channel = 0; channel < work.getNumChannels(); ++channel)
        {
            auto* samples = work.getWritePointer(channel);
            for (int i = 0; i < work.getNumSamples(); ++i)
                samples[i] += (random.nextFloat() - random.nextFloat()) * lsb;
        }
    }

    juce::WavAudioFormat wavFormat;
    // The writer takes ownership of (and deletes) the stream, so it must be heap-allocated.
    auto* stream = new juce::MemoryOutputStream(encoded, false);
    std::unique_ptr<juce::AudioFormatWriter> writer(
        wavFormat.createWriterFor(stream, sampleRate, (unsigned int) work.getNumChannels(), bitsPerSample, {}, 0));
    if (writer == nullptr)
    {
        delete stream;
        errorMessage = "Could not create a WAV writer for that format.";
        return false;
    }

    if (! writer->writeFromAudioSampleBuffer(work, 0, work.getNumSamples()))
    {
        errorMessage = "Could not encode the render as a WAV.";
        return false;
    }

    writer.reset(); // finalizes the header and the block
    return true;
}

// A 24-bit WAV of the buffer, in memory.
bool writeWavData(juce::MemoryBlock& wavData,
                  const juce::AudioBuffer<float>& buffer,
                  double sampleRate,
                  juce::String& errorMessage)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        errorMessage = "There is no audio to export.";
        return false;
    }

    wavData.reset();
    auto* stream = new juce::MemoryOutputStream(wavData, false);
    juce::WavAudioFormat wavFormat;
    std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(stream, sampleRate, (unsigned int) buffer.getNumChannels(), 24, {}, 0));
    if (writer == nullptr)
    {
        delete stream;
        errorMessage = "Could not create a WAV writer for this export.";
        return false;
    }

    const auto ok = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    writer.reset(); // finishes the WAV header
    if (! ok)
        errorMessage = "Could not write the WAV export.";
    return ok;
}

bool writeWavFile(const juce::File& destination,
                  const juce::AudioBuffer<float>& buffer,
                  double sampleRate,
                  juce::String& errorMessage)
{
    if (buffer.getNumChannels() <= 0 || buffer.getNumSamples() <= 0)
    {
        errorMessage = "There is no audio to export.";
        return false;
    }

    destination.getParentDirectory().createDirectory();

    juce::WavAudioFormat wavFormat;
    auto outputStream = std::unique_ptr<juce::FileOutputStream>(destination.createOutputStream());
    if (outputStream == nullptr)
    {
        errorMessage = "Could not open the export file for writing.";
        return false;
    }

    auto writer = std::unique_ptr<juce::AudioFormatWriter>(wavFormat.createWriterFor(outputStream.get(),
                                                                                      sampleRate,
                                                                                      (unsigned int) buffer.getNumChannels(),
                                                                                      24,
                                                                                      {},
                                                                                      0));
    if (writer == nullptr)
    {
        errorMessage = "Could not create a WAV writer for this export.";
        return false;
    }

    outputStream.release();
    if (! writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples()))
    {
        errorMessage = "Could not write the WAV export.";
        return false;
    }

    return true;
}

juce::String workspaceModeName(MainComponent::WorkspaceMode mode)
{
    switch (mode)
    {
        case MainComponent::WorkspaceMode::tracker: return "Tracker";
        case MainComponent::WorkspaceMode::signal: return "Signal";
        case MainComponent::WorkspaceMode::library: return "Library";
        case MainComponent::WorkspaceMode::mix: return "Layers";
        case MainComponent::WorkspaceMode::plugins: return "Plugins";
        case MainComponent::WorkspaceMode::code: return "Script";
        case MainComponent::WorkspaceMode::record: return "Capture";
        case MainComponent::WorkspaceMode::score: return "Score";
        case MainComponent::WorkspaceMode::settings: return "Settings";
        case MainComponent::WorkspaceMode::sampler: return "Sampler";
        case MainComponent::WorkspaceMode::foley: return "Foley";
    }

    return "Tracker";
}

constexpr int workspaceModeCount = 12;

int workspaceModeIndex(MainComponent::WorkspaceMode mode)
{
    return juce::jlimit(0, workspaceModeCount - 1, static_cast<int>(mode));
}

juce::String makeRecordingTimestamp()
{
    return juce::Time::getCurrentTime().formatted("%Y-%m-%d_%H-%M-%S");
}

double midiToFrequency(int midiNote)
{
    return 440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
}

juce::AudioBuffer<float> renderScorePreviewBuffer(const ScorePanel::PlaybackRequest& request, double sampleRate)
{
    auto sortedNotes = request.notes;
    std::sort(sortedNotes.begin(), sortedNotes.end(), [](const ScorePanel::NoteEvent& a, const ScorePanel::NoteEvent& b)
    {
        if (a.measure != b.measure)
            return a.measure < b.measure;

        if (a.beat != b.beat)
            return a.beat < b.beat;

        return a.midiNote < b.midiNote;
    });

    auto bpm = juce::jmax(40, request.tempoBpm);
    auto secondsPerBeat = 60.0 / static_cast<double>(bpm);
    auto totalDurationSeconds = 1.0;

    for (const auto& note : sortedNotes)
    {
        if (note.isRest)
            continue;

        auto startBeat = static_cast<double>(note.measure * 4) + static_cast<double>(note.beat - 1.0f);
        auto noteEnd = startBeat + juce::jmax(0.25f, note.durationBeats);
        totalDurationSeconds = juce::jmax(totalDurationSeconds, (noteEnd * secondsPerBeat) + 0.4);
    }

    auto totalSamples = juce::jmax(1, juce::roundToInt(totalDurationSeconds * sampleRate));
    juce::AudioBuffer<float> buffer(2, totalSamples);
    buffer.clear();

    for (const auto& note : sortedNotes)
    {
        if (note.isRest)
            continue;

        auto startBeat = static_cast<double>(note.measure * 4) + static_cast<double>(note.beat - 1.0f);
        auto startSeconds = startBeat * secondsPerBeat;
        auto noteDurationSeconds = secondsPerBeat * juce::jmax(0.20f, note.durationBeats * 0.92f);
        auto attackSeconds = 0.01;
        auto releaseSeconds = juce::jmin(0.12, noteDurationSeconds * 0.35);
        auto sustainSeconds = juce::jmax(0.02, noteDurationSeconds - attackSeconds - releaseSeconds);
        auto frequency = midiToFrequency(note.midiNote);

        auto startSample = juce::jlimit(0, totalSamples - 1, juce::roundToInt(startSeconds * sampleRate));
        auto noteSamples = juce::jmax(1, juce::roundToInt(noteDurationSeconds * sampleRate));
        auto attackSamples = juce::jmax(1, juce::roundToInt(attackSeconds * sampleRate));
        auto sustainSamples = juce::jmax(1, juce::roundToInt(sustainSeconds * sampleRate));
        auto releaseSamples = juce::jmax(1, noteSamples - attackSamples - sustainSamples);
        auto phase = 0.0;
        auto phaseDelta = juce::MathConstants<double>::twoPi * frequency / sampleRate;

        for (int sample = 0; sample < noteSamples && (startSample + sample) < totalSamples; ++sample)
        {
            auto envelope = 1.0f;

            if (sample < attackSamples)
                envelope = static_cast<float>(sample) / static_cast<float>(attackSamples);
            else if (sample >= attackSamples + sustainSamples)
                envelope = 1.0f - (static_cast<float>(sample - attackSamples - sustainSamples) / static_cast<float>(juce::jmax(1, releaseSamples)));

            envelope = juce::jlimit(0.0f, 1.0f, envelope);

            auto body = 0.70 * std::sin(phase);
            auto shimmer = 0.20 * std::sin(phase * 2.0);
            auto air = 0.10 * std::sin(phase * 3.0);
            auto sampleValue = static_cast<float>((body + shimmer + air) * 0.18 * envelope);
            auto targetSample = startSample + sample;

            buffer.addSample(0, targetSample, sampleValue);
            buffer.addSample(1, targetSample, sampleValue);
            phase += phaseDelta;
        }
    }

    buffer.applyGain(0.8f);
    return buffer;
}

bool isAdminRole(const juce::String& role)
{
    auto normalized = role.trim().toLowerCase();
    return normalized == "admin" || normalized == "administrator";
}
}

MainComponent::ViewModeBar::ViewModeBar()
{
    auto setupButton = [this](juce::TextButton& button, const juce::String& tooltip, const std::function<void()>& onClick)
    {
        button.onClick = onClick;
        button.setTooltip(tooltip);
        addAndMakeVisible(button);
    };

    setupButton(projectButton,
                "Project actions for the current Djehuti Station workspace",
                [this]
                {
                    if (onProjectMenuRequested)
                        onProjectMenuRequested(projectButton);
                });
    setupButton(toolsButton,
                "Show or hide docked creative tools",
                [this]
                {
                    if (onToolsMenuRequested)
                        onToolsMenuRequested(toolsButton);
                });
    setupButton(helpButton,
                "Guided tour and help actions",
                [this]
                {
                    if (onHelpMenuRequested)
                        onHelpMenuRequested(helpButton);
                });
}

void MainComponent::ViewModeBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff263140));
    g.drawLine(0.0f,
               static_cast<float>(getHeight()) - 1.0f,
               static_cast<float>(getWidth()),
               static_cast<float>(getHeight()) - 1.0f,
               1.0f);

}

void MainComponent::ViewModeBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    constexpr int buttonWidth = 92;
    projectButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(6);
    toolsButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(6);
    helpButton.setBounds(area.removeFromLeft(buttonWidth));
}

MainComponent::PluginRackBar::PluginRackBar()
{
    setName("Plugin Rack");
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    titleLabel.setFont(juce::Font(18.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    contextLabel.setText("Master", juce::dontSendNotification);
    contextLabel.setJustificationType(juce::Justification::centredLeft);
    contextLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(contextLabel);

    pluginNameLabel.setText("No plugin loaded", juce::dontSendNotification);
    pluginNameLabel.setJustificationType(juce::Justification::centredLeft);
    pluginNameLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));

    catalogLabel.setText("No VST folders configured.", juce::dontSendNotification);
    catalogLabel.setJustificationType(juce::Justification::centredLeft);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff71839b));

    bypassButton.onClick = [this]
    {
        if (onBypassChanged)
            onBypassChanged(bypassButton.getToggleState());
    };
    bypassButton.setTooltip("Bypass this plugin");
    addAndMakeVisible(bypassButton);

    pathsButton.onClick = [this]
    {
        if (onManagePluginPaths)
            onManagePluginPaths();
    };
    pathsButton.setTooltip("Manage VST plugin search folders");
    addAndMakeVisible(pathsButton);

    openEditorButton.onClick = [this]
    {
        if (onOpenPluginEditor)
            onOpenPluginEditor();
    };
    openEditorButton.setTooltip("Open this plugin's own UI window");
    addAndMakeVisible(openEditorButton);

    fxStackButton.onClick = [this]
    {
        if (onOpenFxStack)
            onOpenFxStack();
    };
    fxStackButton.setTooltip("Open the full FX stack for this track");
    addAndMakeVisible(fxStackButton);

    loadButton.onClick = [this]
    {
        if (onLoadPlugin)
            onLoadPlugin();
    };
    loadButton.setTooltip("Load a VST3 plugin into this slot");
    addAndMakeVisible(loadButton);

    unloadButton.onClick = [this]
    {
        if (onUnloadPlugin)
            onUnloadPlugin();
    };
    unloadButton.setTooltip("Remove the loaded plugin from this slot");
    addAndMakeVisible(unloadButton);

    pathsButton.setVisible(false);
    loadButton.setVisible(false);
}

void MainComponent::PluginRackBar::setContextMaster()
{
    context = Context::master;
    selectedTrackIndex = -1;
    titleLabel.setText("Master Insert", juce::dontSendNotification);
    contextLabel.setText("Master", juce::dontSendNotification);
    fxStackButton.setEnabled(false);
}

void MainComponent::PluginRackBar::setContextTrack(int trackIndex, const juce::String& trackName)
{
    context = Context::track;
    selectedTrackIndex = trackIndex;
    titleLabel.setText("Track Insert", juce::dontSendNotification);
    contextLabel.setText(trackName.isNotEmpty() ? ("Track " + juce::String(trackIndex + 1) + " - " + trackName)
                                                : ("Track " + juce::String(trackIndex + 1)),
                         juce::dontSendNotification);
    fxStackButton.setEnabled(true);
}

void MainComponent::PluginRackBar::setPluginName(const juce::String& name)
{
    pluginNameLabel.setText(name.isNotEmpty() ? "Loaded: " + name : "No plugin loaded", juce::dontSendNotification);
    hasPlugin = name.isNotEmpty();
    openEditorButton.setEnabled(hasPlugin);
    bypassButton.setEnabled(hasPlugin);
    unloadButton.setEnabled(hasPlugin);
    fxStackButton.setEnabled(context == Context::track);
}

void MainComponent::PluginRackBar::setCatalogSummary(const juce::String& summary)
{
    catalogLabel.setText(summary, juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setBypassed(bool shouldBypass)
{
    bypassButton.setToggleState(shouldBypass, juce::dontSendNotification);
}

void MainComponent::PluginRackBar::setHasPlugin(bool shouldHavePlugin)
{
    hasPlugin = shouldHavePlugin;
    if (! hasPlugin)
    {
        openEditorButton.setEnabled(false);
        bypassButton.setEnabled(false);
        unloadButton.setEnabled(false);
    }
    fxStackButton.setEnabled(context == Context::track);
}

void MainComponent::PluginRackBar::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff10141a));
    g.setColour(juce::Colour(0xff273243));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MainComponent::PluginRackBar::resized()
{
    auto area = getLocalBounds().reduced(14, 8);
    titleLabel.setBounds(area.removeFromLeft(150));
    contextLabel.setBounds(area.removeFromLeft(220));
    area.removeFromLeft(360); // reserved - no longer shows the plugin name/catalog status text
    unloadButton.setBounds(area.removeFromRight(90));
    openEditorButton.setBounds(area.removeFromRight(110));
    fxStackButton.setBounds(area.removeFromRight(100).reduced(4, 0));
    bypassButton.setBounds(area.removeFromRight(100));
}

MainComponent::FxStackPanel::FxStackPanel()
{
    titleLabel.setText("Track FX Stack", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(20.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    trackLabel.setText("No track selected", juce::dontSendNotification);
    trackLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb2cc));
    addAndMakeVisible(trackLabel);

    pluginList.setRowHeight(34);
    pluginList.setColour(juce::ListBox::backgroundColourId, juce::Colour(0xff0d141d));
    addAndMakeVisible(pluginList);

    removeButton.onClick = [this] { if (onRemovePlugin) onRemovePlugin(getSelectedSlot()); };
    upButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onMovePlugin && slot > 0)
            onMovePlugin(slot, slot - 1);
    };
    downButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onMovePlugin && slot >= 0 && slot < pluginNames.size() - 1)
            onMovePlugin(slot, slot + 1);
    };
    bypassButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onBypassChanged && juce::isPositiveAndBelow(slot, pluginBypassStates.size()))
            onBypassChanged(slot, ! pluginBypassStates[slot]);
    };
    openButton.onClick = [this]
    {
        const auto slot = getSelectedSlot();
        if (onOpenPluginEditor && slot >= 0)
            onOpenPluginEditor(slot);
    };

    removeButton.setTooltip("Remove the selected plugin from the stack");
    upButton.setTooltip("Move the selected plugin up in the chain");
    downButton.setTooltip("Move the selected plugin down in the chain");
    bypassButton.setTooltip("Bypass the selected plugin");
    openButton.setTooltip("Open the selected plugin's own UI window");

    for (auto* button : { &removeButton, &upButton, &downButton, &bypassButton, &openButton })
        addAndMakeVisible(button);

    catalogLabel.setText("Available Plugins", juce::dontSendNotification);
    catalogLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb2cc));
    addAndMakeVisible(catalogLabel);

    searchBox.setTextToShowWhenEmpty("Search plugins...", juce::Colour(0xff6d7d91));
    searchBox.onTextChange = [this]
    {
        catalogBrowser.setFilterText(searchBox.getText());
    };
    addAndMakeVisible(searchBox);

    catalogBrowser.onEntryChosen = [this](const VstPluginCatalog::Entry& entry)
    {
        if (onAddPlugin)
            onAddPlugin(entry);
    };
    addAndMakeVisible(catalogBrowser);

    addButton.onClick = [this] { addSelectedCatalogEntry(); };
    insertButton.onClick = [this] { insertSelectedCatalogEntry(); };
    rescanButton.onClick = [this] { if (onRescanRequested) onRescanRequested(); };

    addButton.setTooltip("Add the selected plugin to the end of the stack");
    insertButton.setTooltip("Insert the selected plugin at the current position");
    rescanButton.setTooltip("Rescan VST folders for plugins");

    for (auto* button : { &addButton, &insertButton, &rescanButton })
        addAndMakeVisible(button);

    refreshButtonState();
}

void MainComponent::FxStackPanel::setTrackName(const juce::String& name)
{
    trackLabel.setText(name.isNotEmpty() ? name : "Selected track", juce::dontSendNotification);
}

void MainComponent::FxStackPanel::setPlugins(const juce::StringArray& names, const juce::Array<bool>& bypassStates)
{
    const auto oldSelection = getSelectedSlot();
    pluginNames = names;
    pluginBypassStates = bypassStates;
    pluginList.updateContent();
    if (pluginNames.isEmpty())
        pluginList.deselectAllRows();
    else
        pluginList.selectRow(juce::jlimit(0, pluginNames.size() - 1, oldSelection), false, true);
    refreshButtonState();
    repaint();
}

void MainComponent::FxStackPanel::setCatalog(const juce::Array<VstPluginCatalog::Entry>& entries)
{
    catalogBrowser.setPlugins(entries);
}

void MainComponent::FxStackPanel::addSelectedCatalogEntry()
{
    if (auto* entry = catalogBrowser.getSelectedEntry())
        if (onAddPlugin)
            onAddPlugin(*entry);
}

void MainComponent::FxStackPanel::insertSelectedCatalogEntry()
{
    const auto slot = getSelectedSlot();
    if (auto* entry = catalogBrowser.getSelectedEntry())
        if (onInsertPlugin && slot >= 0)
            onInsertPlugin(slot, *entry);
}

int MainComponent::FxStackPanel::getSelectedSlot() const noexcept
{
    return pluginList.getSelectedRow();
}

void MainComponent::FxStackPanel::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff101820));
    g.setColour(juce::Colour(0xff27364a));
    g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(1.0f), 10.0f, 1.0f);
}

void MainComponent::FxStackPanel::resized()
{
    auto area = getLocalBounds().reduced(14);
    titleLabel.setBounds(area.removeFromTop(28));
    trackLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(8);

    auto leftColumn = area.removeFromLeft(area.getWidth() / 2 - 8);
    area.removeFromLeft(16);
    auto rightColumn = area;

    auto leftButtonRow = leftColumn.removeFromBottom(38);
    removeButton.setBounds(leftButtonRow.removeFromLeft(90).reduced(3));
    upButton.setBounds(leftButtonRow.removeFromLeft(70).reduced(3));
    downButton.setBounds(leftButtonRow.removeFromLeft(76).reduced(3));
    bypassButton.setBounds(leftButtonRow.removeFromLeft(90).reduced(3));
    openButton.setBounds(leftButtonRow.removeFromLeft(100).reduced(3));
    pluginList.setBounds(leftColumn.reduced(0, 8));

    auto rightButtonRow = rightColumn.removeFromBottom(38);
    addButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));
    insertButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));
    rescanButton.setBounds(rightButtonRow.removeFromLeft(86).reduced(3));

    catalogLabel.setBounds(rightColumn.removeFromTop(20));
    rightColumn.removeFromTop(2);
    searchBox.setBounds(rightColumn.removeFromTop(28));
    rightColumn.removeFromTop(6);
    catalogBrowser.setBounds(rightColumn);
}

int MainComponent::FxStackPanel::getNumRows()
{
    return pluginNames.size();
}

void MainComponent::FxStackPanel::paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected)
{
    auto row = juce::Rectangle<int>(0, 0, width, height).reduced(5, 3);
    g.setColour(rowIsSelected ? juce::Colour(0xff1f5f86) : juce::Colour(0xff172332));
    g.fillRoundedRectangle(row.toFloat(), 6.0f);

    const auto bypassed = juce::isPositiveAndBelow(rowNumber, pluginBypassStates.size()) && pluginBypassStates[rowNumber];
    g.setColour(bypassed ? juce::Colour(0xffff7d7d) : juce::Colour(0xff6fe7ff));
    g.fillEllipse((float) row.getX() + 10.0f, (float) row.getCentreY() - 4.0f, 8.0f, 8.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(14.0f).boldened());
    g.drawText(juce::String(rowNumber + 1).paddedLeft('0', 2) + "  " + pluginNames[rowNumber],
               row.reduced(28, 0),
               juce::Justification::centredLeft,
               true);

    if (bypassed)
    {
        g.setColour(juce::Colour(0xffffa0a0));
        g.setFont(juce::Font(12.0f));
        g.drawText("bypassed", row.reduced(8), juce::Justification::centredRight, true);
    }
}

void MainComponent::FxStackPanel::selectedRowsChanged(int)
{
    refreshButtonState();
}

void MainComponent::FxStackPanel::listBoxItemDoubleClicked(int row, const juce::MouseEvent&)
{
    if (onOpenPluginEditor && juce::isPositiveAndBelow(row, pluginNames.size()))
        onOpenPluginEditor(row);
}

void MainComponent::FxStackPanel::refreshButtonState()
{
    const auto slot = getSelectedSlot();
    const auto hasSelection = juce::isPositiveAndBelow(slot, pluginNames.size());
    insertButton.setEnabled(hasSelection);
    removeButton.setEnabled(hasSelection);
    upButton.setEnabled(hasSelection && slot > 0);
    downButton.setEnabled(hasSelection && slot < pluginNames.size() - 1);
    bypassButton.setEnabled(hasSelection);
    openButton.setEnabled(hasSelection);
}

MainComponent::MainComponent()
    : MainComponent(StartupProgressCallback{})
{
}

MainComponent::MainComponent(StartupProgressCallback startupProgressCallback)
{
    auto reportStartup = [&startupProgressCallback](const juce::String& statusText, float progress)
    {
        if (startupProgressCallback)
            startupProgressCallback(statusText, juce::jlimit(0.0f, 1.0f, progress));
    };

    reportStartup("Preparing application shell...", 0.08f);
    setWantsKeyboardFocus(true);
    transportBar.setAppTitle("Djehuti Station");
    transportBar.setLogoImage(creation::ui::getSuiteLogoImage(creation::ui::SuiteLogoId::station));

    appManifest = CreationStationAppManifest::createDefault(
        juce::JUCEApplicationBase::getInstance() != nullptr
            ? juce::JUCEApplicationBase::getInstance()->getApplicationVersion()
            : "0.5.1");

    {
        juce::String helpError;
        if (! helpLibrary.loadEmbedded(helpError))
            juce::Logger::writeToLog("Help failed to load: " + helpError);
        for (const auto& problem : helpLibrary.validate())
            juce::Logger::writeToLog("Help: " + problem);
    }

    reportStartup("Opening audio engine...", 0.18f);
    {
        // AudioDeviceManager::initialise scans/opens real audio hardware --
        // confirmed via a real user hang report that this can block the
        // message thread for a long time on real machines (slow or
        // misbehaving drivers), which Windows then reports as "Not
        // Responding" with the spinning-wheel cursor. deviceManager is only
        // ever touched here and on this one helper thread, never both at
        // once (this thread just pumps messages while it waits), so this
        // stays safe despite AudioDeviceManager not being documented as
        // thread-safe in general. The rest of startup still runs in the
        // same order right after, unchanged -- this only changes how the
        // wait for this one slow step is spent.
        std::atomic<bool> deviceInitDone { false };
        std::thread initThread([this, &deviceInitDone]
        {
            deviceManager.initialise(32, 2, nullptr, true, {}, nullptr);
            deviceInitDone.store(true, std::memory_order_release);
        });

        pumpMessagesWhile(deviceInitDone);
        initThread.join();
    }
    engine.attachToDevice(deviceManager);
    engine.setPlaying(false);
    transportBar.setPlaybackVisualState(false, false);

    reportStartup("Building the studio surface...", 0.28f);
    setSize(1400, 900);
    menuBar = std::make_unique<juce::MenuBarComponent>(static_cast<juce::MenuBarModel*>(this));
    // Nothing in this app sets a suite-wide dark LookAndFeel, so MenuBarComponent
    // falls back to LookAndFeel_V4::drawMenuBarItem/drawMenuBarBackground, which key
    // off TextButton colour ids (not PopupMenu's) -- on this JUCE version that default
    // scheme renders dark text on a dark-on-dark bar, invisible against the studio's
    // navy chrome. Force explicit colours instead of depending on the LookAndFeel default.
    menuBar->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1c2230));
    menuBar->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff2a3244));
    menuBar->setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    menuBar->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    dockManager = std::make_unique<CreationDock::DockManager>(*this);
    // A resized video view asks for its next frame at the new size, so a bigger window is a sharper picture.
    videoView.onSizeChanged = [this] { for (auto& feed : videoFeeds) feed.second->requestKey = {}; };
    dockManager->onPanelActivated = [this](const juce::String& panelId)
    {
        if (panelId == trackerPanelId) setWorkspaceMode(WorkspaceMode::tracker);
        else if (panelId == samplerPanelId) setWorkspaceMode(WorkspaceMode::sampler);
        else if (panelId == signalPanelId) setWorkspaceMode(WorkspaceMode::signal);
        else if (panelId == pluginsPanelId) setWorkspaceMode(WorkspaceMode::plugins);
        else if (panelId == scorePanelId) setWorkspaceMode(WorkspaceMode::score);
        else if (panelId == settingsPanelId) setWorkspaceMode(WorkspaceMode::settings);
        else if (panelId == foleyPanelId) setWorkspaceMode(WorkspaceMode::foley);
        else if (panelId == layersPanelId) setWorkspaceMode(WorkspaceMode::mix);
    };

    addAndMakeVisible(authGateView);
    transportBarSafe = &transportBar;
    pluginRackBarSafe = &pluginRackBar;
    mixerPanelSafe = &mixerPanel;
    authGateView.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    authGateView.onLogoutRequested = [this]
    {
        authSession.clearSession();
        authenticated = false;
        refreshAuthState();
    };
    authSession.onStatusChanged = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
    };
    authSession.onError = [this](const juce::String& text)
    {
        authGateView.setStatusText(text);
        transportBar.setStatusText(text);
        authGateView.setBusy(false);
        transportBar.signInButton.setEnabled(true);
    };
    authSession.onBusyChanged = [this](bool shouldBeBusy)
    {
        authGateView.setBusy(shouldBeBusy);
        transportBar.signInButton.setEnabled(! shouldBeBusy);
    };
    authSession.onAuthenticated = [this](const creation::ui::SuiteDesktopAuthSession::SessionData& session)
    {
        authenticated = true;
        transportBar.setProfile(makeHeaderProfile(session));
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Signed in. Loading your workspace...");
        refreshAuthState();
    };
    authSession.onSessionCleared = [this]
    {
        authenticated = false;
        appContextSyncInProgress = false;
        transportBar.clearProfile();
        authGateView.setAccountText("Not signed in yet.");
        authGateView.setStatusText("Session cleared.");
        refreshAuthState();
    };
    suiteShellController.configure({ "Djehuti Station",
                                     creation::assets::SuiteAppDomain::station,
                                     juce::Colour(0xff15181d),
                                     creation::ui::SuiteAssetManagerCapability{ "Djehuti Station",
                                                                                creation::assets::SuiteAppDomain::station,
                                                                                { ".frust" },
                                                                                {},
                                                                                [this]()
                                                                                {
                                                                                    return projectSession.getManifest().assetCatalog.assets;
                                                                                },
                                                                                [](const creation::assets::AssetDescriptor& asset)
                                                                                {
                                                                                    // What the manifest records about the asset. Reading it never opens or copies the
                                                                                    // asset's file, however big it is.
                                                                                    juce::String details;
                                                                                    const auto& d = asset.details;
                                                                                    if (d.getValue("durationSeconds", {}).isNotEmpty())
                                                                                        details << "\nLength: " << juce::String(d.getValue("durationSeconds", {}).getDoubleValue(), 2) << " s";
                                                                                    if (d.getValue("width", {}).isNotEmpty() && d.getValue("height", {}).isNotEmpty())
                                                                                        details << "\nPicture: " << d.getValue("width", {}) << " x " << d.getValue("height", {});
                                                                                    if (d.getValue("frameRate", {}).isNotEmpty())
                                                                                        details << "\nFrame rate: " << juce::String(d.getValue("frameRate", {}).getDoubleValue(), 2) << " fps";
                                                                                    if (d.getValue("sampleRate", {}).isNotEmpty())
                                                                                        details << "\nSample rate: " << d.getValue("sampleRate", {}) << " Hz";
                                                                                    if (d.getValue("channels", {}).isNotEmpty())
                                                                                        details << "\nChannels: " << d.getValue("channels", {});
                                                                                    if (d.getValue("hasAudio", {}).isNotEmpty())
                                                                                        details << "\nHas sound: " << (d.getValue("hasAudio", {}) == "1" ? "yes" : "no");
                                                                                    return details;
                                                                                },
                                                                                [this](const creation::assets::AssetDescriptor& asset)
                                                                                {
                                                                                    openProjectAsset(asset);
                                                                                },
                                                                                [this](const creation::assets::AssetDescriptor&)
                                                                                {
                                                                                    engine.stopAssetPreview();
                                                                                    transportBar.setStatusText("Stopped project asset preview.");
                                                                                },
                                                                                [this](const creation::assets::AssetDescriptor& asset)
                                                                                {
                                                                                    placeProjectAssetOnTracker(asset);
                                                                                },
                                                                                [this](const creation::assets::AssetDescriptor& asset)
                                                                                {
                                                                                    exportProjectAssetRaw(asset);
                                                                                } } },
                                   [this](const juce::String& status)
                                   {
                                       transportBar.setStatusText(status);
                                       if (status.containsIgnoreCase("saved suite-wide"))
                                       {
                                           juce::String suiteSettingsError;
                                           suiteSettings = suiteSettingsStore.load(suiteSettingsError);
                                           if (suiteSettingsError.isNotEmpty())
                                               transportBar.setStatusText(suiteSettingsError);
                                       }
                                       if (status.containsIgnoreCase("AI routing"))
                                           loadSuiteAiProviderSettings();
                                   });
    suiteShellController.onProjectOpenRequested = [this](const juce::String& projectId)
    {
        openProject(projectId);
    };
    transportBar.onProjectMenuRequested = [this]
    {
        suiteShellController.showProjectBrowser();
    };
    transportBar.onAudioRequested = [this]
    {
        showAudioSettings();
    };
    transportBar.onAssetManagerRequested = [this]
    {
        suiteShellController.showAssetManager();
    };
    transportBar.onSuiteRequested = [this]
    {
        suiteShellController.showSuiteSettings();
    };
    transportBar.onTourRequested = [this]
    {
        showTour();
    };
    transportBar.onLearnMidiRequested = [this](const juce::String& targetId, const juce::String& displayLabel)
    {
        showMidiLearnDialog(targetId, displayLabel);
    };
    if (authSession.hasValidSession())
    {
        authenticated = true;
        const auto& session = authSession.getSession();
        transportBar.setProfile(makeHeaderProfile(session));
        authGateView.setAccountText(session.user.displayName.isNotEmpty()
                                        ? session.user.displayName + " <" + session.user.email + ">"
                                        : session.user.email);
        authGateView.setStatusText("Restored your saved login.");
    }

    reportStartup("Loading storage and settings...", 0.40f);
    juce::String storageError;
    if (! true)
    {
        auto startupStorageMessage = storageError.isNotEmpty()
                                       ? storageError
                                       : "Storage is not configured yet. The studio can still open. Set it up later when you want to save projects or manage content.";
        transportBar.setStatusText(startupStorageMessage);
    }

    loadAppSettings();
    {
        juce::String suiteSettingsError;
        suiteSettings = suiteSettingsStore.load(suiteSettingsError);
        if (suiteSettingsError.isNotEmpty())
            transportBar.setStatusText(suiteSettingsError);
    }
    reportStartup("Applying audio device settings...", 0.50f);
    applySelectedAudioDeviceSettings();

    reportStartup("Scanning VST plugin folders...", 0.60f);
    rescanVstCatalog();

    reportStartup("Opening project workspace...", 0.72f);
    auto loadedAutoloadProject = false;
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        juce::String activeError;
        loadedAutoloadProject = ensureProjectSessionActive(activeError);
        if (! loadedAutoloadProject && activeError.isNotEmpty())
            transportBar.setStatusText(activeError);
    }
    transportBar.setProjectLabel("Project: " + (projectSession.isValid() ? projectSession.getManifest().projectName : juce::String("No project open")));
    settingsPanel.setStoragePath(suiteSettings.suiteVfsRoot.isNotEmpty() ? juce::File(suiteSettings.suiteVfsRoot).getFullPathName() : "");
    if (projectSession.isValid())
        settingsPanel.setProjectMetadata(projectSession.getManifest());
    settingsPanel.setAutoloadEnabled(true);
    loadSuiteAiProviderSettings();
    refreshAiAccountModelCachesAtStartup();

    if (! suiteSettings.suiteVfsRoot.isNotEmpty() && storageError.isEmpty())
        contentPanel.setStatusText("Storage is not configured yet. You can keep using the studio and set up saving/content later.");

    if (loadedAutoloadProject)
    {
        reportStartup("Restoring project tracks and clips...", 0.80f);
        loadSessionFromDisk();
    }

    reportStartup("Loading control-surface maps...", 0.86f);
    juce::String controlSurfaceError;
    auto controlSurfaceEntry = creation::services::SuiteVfsJsonStore::loadJson("station-control-surface-mappings.json", controlSurfaceError);
    if (! controlSurfaceMappings.loadFromVar(controlSurfaceEntry, controlSurfaceError)
        || controlSurfaceMappings.getProfiles().isEmpty())
    {
        controlSurfaceMappings = ControlSurfaceMappingStore::createDefaultLibrary();
        creation::services::SuiteVfsJsonStore::saveJson("station-control-surface-mappings.json", controlSurfaceMappings.toVar(), controlSurfaceError);
    }
    auto* activePreset = controlSurfaceMappings.findProfileById(controlSurfaceMappings.getActivePresetId());
    transportBar.setMidiStatusText("Control preset: " + (activePreset != nullptr ? activePreset->displayName
                                                                                 : controlSurfaceMappings.getActivePresetId()));
    midiSurface.setControlSurfaceMappings(controlSurfaceMappings);
    midiSurface.setEngineForMidiLearn(engine);

    reportStartup("Creating studio panels...", 0.92f);
    addAndMakeVisible(transportBar);
    addAndMakeVisible(*menuBar);
    addAndMakeVisible(*dockManager);
    // setSize() above ran before menuBar/dockManager existed, so the resized() it
    // triggered laid out only transportBar (its own null checks skipped the rest) --
    // menuBar and dockManager were left at their default zero bounds. Force one more
    // layout pass now that every child actually exists.
    resized();
    poppedWorkspacePlaceholder.setJustificationType(juce::Justification::centred);
    poppedWorkspacePlaceholder.setFont(juce::Font(20.0f).boldened());
    poppedWorkspacePlaceholder.setColour(juce::Label::textColourId, juce::Colour(0xffc7d7ef));
    poppedWorkspacePlaceholder.setColour(juce::Label::backgroundColourId, juce::Colour(0xff121a25));
    addChildComponent(poppedWorkspacePlaceholder);
    addChildComponent(tourOverlay);

    menuItemsChanged();
    initialiseDockingWorkspace();

    pluginRackBar.setContextMaster();
    refreshPluginsPanel();
    trackerPanel.setTimelineModel(&timelineModel);
    trackerPanel.setTrackCount(engine.getTrackCount());
    recordView.setTrackCount(engine.getTrackCount());
    armedTracks.resize((size_t) engine.getTrackCount(), false);
    monitoredTracks.resize((size_t) engine.getTrackCount(), false);
    recordView.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
        {
            armedTracks[(size_t) trackIndex] = shouldArm;
            engine.setTrackRecordingArmed(trackIndex, shouldArm);
            trackerPanel.setTrackArmed(trackIndex, shouldArm);
            saveSessionToDisk();
        }
    };

    refreshAuthState();

    transportBar.onPlay = [this]
    {
        syncActiveModeToFocus();
        engine.stopAssetPreview();

        if (activeMode == WorkspaceMode::signal)
        {
            signalLabPanel.triggerTransportPlay();
            transportBar.setPlaybackVisualState(true, false);
            return;
        }

        if (! prepareTrackerPlayback())
            return;

        openVideoViewForPlayback();

        transportIsWaitingForLoopDelay = false;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        transportStartTimelineSeconds = timelineModel.getTransportSeconds();
        engine.setPlaybackPositionSeconds(transportStartTimelineSeconds);
        engine.setPlaying(true);
        midiSurface.setTransportState(true, false);
        transportBar.setPlaybackVisualState(true, false);
    };
    transportBar.onPause = [this]
    {
        syncActiveModeToFocus();
        engine.stopAssetPreview();

        if (activeMode == WorkspaceMode::signal)
        {
            signalLabPanel.stopTransport();
            transportBar.setPlaybackVisualState(false, false);
            return;
        }

        transportIsWaitingForLoopDelay = false;
        engine.setPlaying(false);
        midiSurface.setTransportState(false, false);
        transportBar.setPlaybackVisualState(false, false);
    };
    transportBar.onStop = [this]
    {
        syncActiveModeToFocus();
        stopRecordingSession();
        engine.stopAssetPreview();

        if (activeMode == WorkspaceMode::signal)
        {
            signalLabPanel.stopTransport();
        }

        transportIsWaitingForLoopDelay = false;
        engine.setPlaying(false);
        midiSurface.setTransportState(false, false);
        transportBar.setPlaybackVisualState(false, false);
    };
    transportBar.onRecord = [this]
    {
        engine.stopAssetPreview();
        if (engine.isRecording() || engine.isMidiRecording())
            stopRecordingSession();
        else if (startRecordingSession())
        {
            refreshTrackerPlaybackClips();
            transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            transportStartTimelineSeconds = timelineModel.getTransportSeconds();
            engine.setPlaying(true);
            midiSurface.setTransportState(true, true);
            transportBar.setPlaybackVisualState(true, true);
        }
    };
    transportBar.onRewind = [this]
    {
        auto previousSeconds = timelineModel.getPreviousBoundarySeconds(timelineModel.getTransportSeconds());
        timelineModel.setTransportSeconds(previousSeconds);
        transportStartTimelineSeconds = previousSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(previousSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: previous boundary");
    };
    transportBar.onFastForward = [this]
    {
        auto nextSeconds = timelineModel.getNextBoundarySeconds(timelineModel.getTransportSeconds());
        timelineModel.setTransportSeconds(nextSeconds);
        transportStartTimelineSeconds = nextSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(nextSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: next boundary");
    };
    transportBar.onRewindToStart = [this]
    {
        constexpr double startSeconds = 0.0;
        timelineModel.setTransportSeconds(startSeconds);
        transportStartTimelineSeconds = startSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(startSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: start");
    };
    transportBar.onFastForwardToEnd = [this]
    {
        auto endSeconds = timelineModel.getTotalDurationSeconds();
        timelineModel.setTransportSeconds(endSeconds);
        transportStartTimelineSeconds = endSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        engine.setPlaybackPositionSeconds(endSeconds);
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
        transportBar.setStatusText("Transport: end");
    };
    transportBar.onLoopChanged = [this](bool loopEnabled)
    {
        timelineModel.setLoopEnabled(loopEnabled);
        transportBar.setStatusText(loopEnabled ? "Transport: loop on" : "Transport: loop off");
    };
    transportBar.onLoopDelayChanged = [this](double delaySeconds)
    {
        timelineModel.setLoopDelaySeconds(delaySeconds);
        transportBar.setStatusText("Transport: loop delay " + juce::String(delaySeconds, 1) + "s");
    };
    transportBar.onMetronomeModeChanged = [this](CreationSuiteHeaderBar::MetronomeMode mode)
    {
        switch (mode)
        {
            case CreationSuiteHeaderBar::MetronomeMode::off:
                engine.setMetronomeMode(WorkstationAudioEngine::MetronomeMode::off);
                break;
            case CreationSuiteHeaderBar::MetronomeMode::playOrRecord:
                engine.setMetronomeMode(WorkstationAudioEngine::MetronomeMode::playOrRecord);
                break;
            case CreationSuiteHeaderBar::MetronomeMode::always:
                engine.setMetronomeMode(WorkstationAudioEngine::MetronomeMode::always);
                break;
        }
    };
    transportBar.onSignInRequested = [this]
    {
        authSession.beginLogin();
    };
    transportBar.onOpenProfilePageRequested = [this]
    {
        suiteShellController.openSuiteProfile();
    };
    transportBar.onLogoutRequested = [this]
    {
        authSession.clearSession();
    };

    auto refreshInsertRack = [this]
    {
        if (pluginRackBar.isTrackContext())
        {
            auto trackIndex = pluginRackBar.getTrackIndex();
            pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
            pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
        }
        else
        {
            pluginRackBar.setPluginName(engine.getMasterPluginName());
            pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
        }
    };

    auto refreshVisibleBank = [this, refreshInsertRack]
    {
        auto bankOffset = mixerPanel.getBankOffset();
        auto visibleCount = mixerPanel.getVisibleChannelCount();
        auto selectedTrackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() : -1;

        midiSurface.setBankOffset(bankOffset);
        mixerPanel.setSelectedChannel(selectedTrackIndex);

        for (int slot = 0; slot < visibleCount; ++slot)
        {
            auto trackIndex = bankOffset + slot;
            if (trackIndex >= engine.getTrackCount())
                continue;

            auto name = engine.getTrackName(trackIndex);
            auto gain = engine.getTrackGain(trackIndex);
            auto pan = engine.getTrackPan(trackIndex);
            auto muted = engine.isTrackMuted(trackIndex);
            auto soloed = engine.isTrackSoloed(trackIndex);
            auto pluginName = engine.getTrackPluginName(trackIndex);
            auto pluginBypassed = engine.isTrackPluginBypassed(trackIndex);

            mixerPanel.setChannelName(trackIndex, name);
            mixerPanel.setChannelInsertName(trackIndex, pluginName.isNotEmpty() ? ("FX: " + pluginName) : "FX: none");
            mixerPanel.setChannelInsertBypassed(trackIndex, pluginBypassed);
            mixerPanel.setChannelGain(trackIndex, gain);
            mixerPanel.setChannelPan(trackIndex, pan);
            mixerPanel.setChannelMuted(trackIndex, muted);
            mixerPanel.setChannelSoloed(trackIndex, soloed);

            midiSurface.setChannelName(trackIndex, name);
            midiSurface.setChannelGain(trackIndex, gain);
            midiSurface.setChannelPan(trackIndex, pan);
            midiSurface.setChannelMuted(trackIndex, muted);
            midiSurface.setChannelSoloed(trackIndex, soloed);
            trackerPanel.setTrackLevel(trackIndex, engine.getTrackLevel(trackIndex));
        }

        auto masterGain = engine.getMasterGain();
        mixerPanel.setMasterGain(masterGain);
        midiSurface.setMasterFaderValue(masterGain);
        midiSurface.refreshVisibleWindow();
        refreshInsertRack();
    };

    auto selectTrack = [this, refreshVisibleBank](int trackIndex)
    {
        if (engine.getTrackCount() == 0)
            return;

        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        auto visibleStart = mixerPanel.getBankOffset();
        auto visibleEnd = visibleStart + mixerPanel.getVisibleChannelCount();
        if (trackIndex < visibleStart || trackIndex >= visibleEnd)
            mixerPanel.setBankOffset((trackIndex / mixerPanel.getVisibleChannelCount()) * mixerPanel.getVisibleChannelCount());

        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
        trackerPanel.setSelectedTrack(trackIndex);
        refreshVisibleBank();
    };

    trackerPanel.onTrackSelected = [selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    trackerPanel.onTrackFxRequested = [this, selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
        showFxStackWindow();
    };

    trackerPanel.onEmptyTrackContextMenuRequested = [this](int trackIndex, double startSeconds, juce::Point<int> screenPos)
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Add Track");
        menu.addItem(2, "Add Clip...");
        
        auto area = juce::Rectangle<int>(screenPos.x, screenPos.y, 1, 1);
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea(area),
            [this, trackIndex, startSeconds](int result)
            {
                if (result == 1)
                {
                    addTrack();
                }
                else if (result == 2)
                {
                    showAddClipPicker(trackIndex, startSeconds);
                }
            });
    };

    trackerPanel.onRemoveTrackRequested = [this](int trackIndex)
    {
        removeTrack(trackIndex);
    };

    trackerPanel.onTrackNameChanged = [this](int trackIndex, const juce::String& name)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackName(trackIndex, name);
        timelineModel.setTrackName(trackIndex, name);
        syncTrackViews();
        midiSurface.refreshVisibleWindow();
        saveSessionToDisk();
    };

    trackerPanel.onTrackKindChanged = [this](int trackIndex, cs::TrackKind kind)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        timelineModel.setTrackKind(trackIndex, kind);
        trackerPanel.setTrackKind(trackIndex, kind);
        engine.setTrackIsMidiKind(trackIndex, kind == cs::TrackKind::midi);
        engine.setTrackIsAutomationKind(trackIndex, kind == cs::TrackKind::automation);

        if (kind == cs::TrackKind::automation)
        {
            timelineModel.ensureAutomationClip(trackIndex);
            pushAutomationDataToEngine(trackIndex);
            trackerPanel.repaint();
        }

        saveSessionToDisk();
    };

    trackerPanel.onAutomationTargetRequested = [this](int trackIndex)
    {
        showAutomationTargetPicker(trackIndex);
    };

    trackerPanel.onMoveToFolderRequested = [this](int trackIndex)
    {
        showMoveToFolderPicker(trackIndex);
    };

    trackerPanel.onTrackReorderRequested = [this](int trackIndex, int destinationIndex)
    {
        if (performTrackMove(trackIndex, destinationIndex))
        {
            syncTrackViews();
            saveSessionToDisk();
        }
    };

    trackerPanel.onAutomationPointChanged = [this](int trackIndex)
    {
        pushAutomationDataToEngine(trackIndex);
    };

    trackerPanel.onAutomationDataCommitted = [this](int trackIndex)
    {
        pushAutomationDataToEngine(trackIndex);
        saveSessionToDisk();
    };

    trackerPanel.onAutomationRecordModeChanged = [this](int trackIndex, cs::AutomationRecordMode mode)
    {
        timelineModel.setAutomationRecordMode(trackIndex, mode);
        saveSessionToDisk();
    };

    trackerPanel.onAutomationRecordingRateChanged = [this](int trackIndex, int pointsPerSecond)
    {
        timelineModel.setAutomationRecordingRate(trackIndex, pointsPerSecond);
        saveSessionToDisk();
    };

    trackerPanel.onTrackArmChanged = [this](int trackIndex, bool shouldArm)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
            return;

        armedTracks[(size_t) trackIndex] = shouldArm;

        // An automation track has no audio input of its own - arming it means "record manual
        // control changes into this lane while playing," not "record audio," so it shouldn't
        // also flip the engine's audio-input recording-armed state.
        if (timelineModel.getTrackKind(trackIndex) != cs::TrackKind::automation)
            engine.setTrackRecordingArmed(trackIndex, shouldArm);

        recordView.setTrackCount(engine.getTrackCount());
        syncTrackViews();
        saveSessionToDisk();
    };

    trackerPanel.onTrackMuteChanged = [this](int trackIndex, bool muted)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackMuted(trackIndex, muted);
        mixerPanel.setChannelMuted(trackIndex, muted);
        midiSurface.setChannelMuted(trackIndex, muted);
        saveSessionToDisk();
    };

    trackerPanel.onTrackSoloChanged = [this](int trackIndex, bool soloed)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackSoloed(trackIndex, soloed);
        mixerPanel.setChannelSoloed(trackIndex, soloed);
        midiSurface.setChannelSoloed(trackIndex, soloed);
        saveSessionToDisk();
    };

    trackerPanel.onTrackMonitorChanged = [this](int trackIndex, bool monitored)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) monitoredTracks.size()))
            return;

        monitoredTracks[(size_t) trackIndex] = monitored;
        engine.setTrackMonitoringEnabled(trackIndex, monitored);
        trackerPanel.setTrackMonitored(trackIndex, monitored);
        saveSessionToDisk();
    };

    trackerPanel.onTrackStereoChanged = [this](int trackIndex, bool stereo)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackStereoEnabled(trackIndex, stereo);
        timelineModel.setTrackChannelMode(trackIndex, stereo ? cs::TrackChannelMode::stereo
                                                             : cs::TrackChannelMode::mono);
        trackerPanel.setTrackStereo(trackIndex, stereo);
        saveSessionToDisk();
    };

    trackerPanel.onTrackGainChanged = [this](int trackIndex, float gain)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        engine.setTrackGain(trackIndex, gain);
        recordAutomationWriteIfArmed(trackIndex, cs::AutomationTargetKind::trackVolume, gain);
        mixerPanel.setChannelGain(trackIndex, gain);
        midiSurface.setChannelGain(trackIndex, gain);
        saveSessionToDisk();
    };

    trackerPanel.onTrackInputChanged = [this](int trackIndex, int inputChannel)
    {
        if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
            return;

        if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::midi)
        {
            // inputChannel here is 0 = Omni (all channels), 1-16 = a specific MIDI channel -
            // a separate axis from audio input routing, which doesn't apply to a MIDI track.
            engine.setTrackMidiInputChannel(trackIndex, inputChannel);
            trackerPanel.setTrackInput(trackIndex, inputChannel);
            saveSessionToDisk();
            return;
        }

        auto resolvedChannel = studioIOModel.getChannelForInputIndex(inputChannel);
        engine.setTrackInputChannel(trackIndex, resolvedChannel);
        trackerPanel.setTrackInput(trackIndex, inputChannel);
        saveSessionToDisk();
    };

    trackerPanel.onPlayheadPositionChanged = [this](double seconds)
    {
        timelineModel.setTransportSeconds(seconds);
        engine.setPlaybackPositionSeconds(seconds);
        transportStartTimelineSeconds = seconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onLoopRegionChanged = [this](double startSeconds, double endSeconds)
    {
        timelineModel.setLoopRegion(startSeconds, endSeconds);
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onLoopRegionCleared = [this]
    {
        timelineModel.clearLoopRegion();
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onTimelineSnapChanged = [this](bool enabled)
    {
        timelineModel.setTimelineSnapEnabled(enabled);
        saveSessionToDisk();
    };

    trackerPanel.onTimelineGridChanged = [this](double gridBeats)
    {
        timelineModel.setTimelineGridBeats(gridBeats);
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onMarkerAddAtRequested = [this](double seconds)
    {
        timelineModel.addMarker(seconds);
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onMarkerMoved = [this](const juce::String& markerId, double seconds)
    {
        timelineModel.moveMarker(markerId, seconds);
        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onMarkerMoveCommitted = [this](const juce::String&)
    {
        saveSessionToDisk();
    };

    trackerPanel.onTrackHeightCommitted = [this](int)
    {
        saveSessionToDisk();
    };

    trackerPanel.onMarkerDeleteRequested = [this](const juce::String& markerId)
    {
        timelineModel.removeMarker(markerId);
        trackerPanel.refreshTimelineView();
        saveSessionToDisk();
    };

    trackerPanel.onArrangementSaveRequested = [this](const juce::String& name)
    {
        if (! projectSession.isValid())
        {
            transportBar.setStatusText("Open or create a project first so Tracker can save arrangements into it.");
            return;
        }

        auto arrangementState = timelineModel.createState();
        auto xml = arrangementState.createXml();
        if (xml == nullptr)
        {
            transportBar.setStatusText("Could not serialize the arrangement.");
            return;
        }
        auto xmlString = xml->toString();
        juce::MemoryBlock data(xmlString.toRawUTF8(), xmlString.getNumBytesAsUTF8());

        creation::assets::ProjectAssetService::ImportOptions options;
        options.kind = creation::assets::AssetKind::trackerArrangement;
        options.displayName = name;
        options.logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot + slugForProjectAssetName(name) + ".csarrangement";
        options.mediaType = "application/x-creation-station-arrangement";
        options.sourceApp = "Djehuti Station";
        options.sourceTool = "Tracker";
        options.description = "Saved set of Tracker tracks/clips.";

        creation::assets::AssetDescriptor savedAsset;
        juce::String errorMessage;
        if (! creation::assets::ProjectAssetService::saveGeneratedAsset(projectSession, data, options, savedAsset, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Save Failed", "Could not save arrangement:\n" + errorMessage);
            transportBar.setStatusText("Save failed.");
            return;
        }

        currentArrangementAssetId = savedAsset.id;
        trackerPanel.setCurrentArrangementName(savedAsset.displayName);
        markArrangementClean();

        if (! projectSession.commit(errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Commit Failed", "Arrangement saved but project commit failed:\n" + errorMessage);
            transportBar.setStatusText("Commit failed.");
            return;
        }

        refreshProjectAssets();
        saveSessionToDisk(true);
        transportBar.setStatusText("Saved arrangement: " + savedAsset.displayName);
    };

    trackerPanel.onArrangementLoadRequested = [this]
    {
        if (! projectSession.isValid())
        {
            transportBar.setStatusText("Open or create a project first so Tracker can load saved arrangements from it.");
            return;
        }

        auto arrangementAssets = projectSession.getManifest().assetCatalog.query({ creation::assets::AssetKind::trackerArrangement });
        if (arrangementAssets.isEmpty())
        {
            transportBar.setStatusText("This project does not have any saved arrangements yet.");
            return;
        }

        std::sort(arrangementAssets.begin(), arrangementAssets.end(), [](const auto& left, const auto& right)
        {
            if (left.modifiedAt != right.modifiedAt)
                return left.modifiedAt > right.modifiedAt;
            return left.displayName.compareIgnoreCase(right.displayName) < 0;
        });

        juce::PopupMenu menu;
        menu.addSectionHeader("Load Arrangement From Project");
        for (int index = 0; index < arrangementAssets.size(); ++index)
        {
            const auto& asset = arrangementAssets.getReference(index);
            auto label = asset.displayName.isNotEmpty() ? asset.displayName : asset.logicalPath;
            auto detail = asset.modifiedAt != juce::Time() ? "  •  " + asset.modifiedAt.toString(true, true) : juce::String{};
            menu.addItem(index + 1, label + detail);
        }

        auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }),
                           [this, arrangementAssets](int result)
                           {
                               if (result <= 0 || result > arrangementAssets.size())
                                   return;

                               const auto& asset = arrangementAssets.getReference(result - 1);
                               if (! restoreArrangementAsset(asset))
                               {
                                   transportBar.setStatusText("Could not load saved arrangement: " + asset.displayName);
                                   return;
                               }

                               saveSessionToDisk(true);
                               transportBar.setStatusText("Loaded arrangement: " + asset.displayName);
                           });
    };

    trackerPanel.onMarkerClicked = [this](const juce::String& markerId)
    {
        for (const auto& marker : timelineModel.getMarkers())
        {
            if (marker.id == markerId)
            {
                timelineModel.setTransportSeconds(marker.seconds);
                engine.setPlaybackPositionSeconds(marker.seconds);
                transportStartTimelineSeconds = marker.seconds;
                transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
                trackerPanel.refreshTimelineView();
                break;
            }
        }
    };

    trackerPanel.onClipMoved = [this](int clipIndex, int trackIndex, double startSeconds)
    {
        if (! clipDragUndoCaptured)
        {
            pushTimelineUndoState();
            clipDragUndoCaptured = true;
        }

        if (! timelineModel.moveClip(clipIndex, trackIndex, startSeconds))
            return;

        trackerPanel.setSelectedTrack(trackIndex);
        if (juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        {
            pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
            mixerPanel.setSelectedChannel(trackIndex);
        }
        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onClipMoveCommitted = [this]
    {
        clipDragUndoCaptured = false;
        refreshTrackerPlaybackClips();
        saveSessionToDisk();
    };

    trackerPanel.onClipTrimmed = [this](int clipIndex, bool leftEdge, double boundarySeconds)
    {
        if (! clipDragUndoCaptured)
        {
            pushTimelineUndoState();
            clipDragUndoCaptured = true;
        }

        const auto trimmed = leftEdge ? timelineModel.trimClipStart(clipIndex, boundarySeconds)
                                      : timelineModel.trimClipEnd(clipIndex, boundarySeconds);
        if (! trimmed)
            return;

        trackerPanel.refreshTimelineView();
    };

    trackerPanel.onClipTrimCommitted = [this]
    {
        clipDragUndoCaptured = false;
        refreshTrackerPlaybackClips();
        saveSessionToDisk();
    };

    trackerPanel.onClipSelected = [this](int clipIndex)
    {
        selectedClipIndex = clipIndex;
        trackerPanel.setSelectedClip(clipIndex);
    };

    trackerPanel.onClipRenameRequested = [this](int clipIndex)
    {
        renameClip(clipIndex);
    };

    trackerPanel.onClipSplitRequested = [this](int clipIndex, double splitSeconds)
    {
        splitClipAt(clipIndex, splitSeconds);
    };

    trackerPanel.onClipDuplicateRequested = [this](int clipIndex)
    {
        duplicateClip(clipIndex);
    };

    trackerPanel.onClipSoundAction = [this](int clipIndex, int action)
    {
        handleClipSoundAction(clipIndex, action);
    };

    trackerPanel.onClipDeleteRequested = [this](int clipIndex)
    {
        deleteClip(clipIndex);
    };

    trackerPanel.onClipEditRequested = [this](int clipIndex)
    {
        showMidiEditorWindow(clipIndex);
    };

    trackerPanel.onEmptyMidiClipRequested = [this](int trackIndex, double seconds)
    {
        auto defaultBeats = 4.0;
        auto durationSeconds = timelineModel.beatToSeconds(defaultBeats);

        juce::String errorMessage;
        auto clipIndex = timelineModel.addClip(cs::ClipKind::midi,
                                               trackIndex,
                                               "MIDI",
                                               "",
                                               "midi-editor",
                                               juce::File(),
                                               seconds,
                                               durationSeconds,
                                               errorMessage);
        if (clipIndex < 0)
        {
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not create MIDI clip.");
            return;
        }

        projectDirty = true;
        trackerPanel.refreshTimelineView();
        showMidiEditorWindow(clipIndex);
    };
    trackerPanel.onAudioFilesDropped = [this](const juce::StringArray& files, int trackIndex, double startSeconds)
    {
        importAudioFilesToTracker(files, trackIndex, startSeconds);
    };

    trackerPanel.onVideoFilesDropped = [this](const juce::StringArray& files, int trackIndex, double startSeconds)
    {
        importVideoFilesToTracker(files, trackIndex, startSeconds);
    };

    trackerPanel.onTempoChanged = [this](double bpm)
    {
        timelineModel.setTempo(bpm, timelineModel.getTimeSignatureNumerator(), timelineModel.getTimeSignatureDenominator());
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                                   timelineModel.getTimeSignatureNumerator(),
                                   timelineModel.getTimeSignatureDenominator(),
                                   timelineModel.getMusicalKey());
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onTimeSignatureChanged = [this](int numerator, int denominator)
    {
        timelineModel.setTempo(timelineModel.getTempoBpm(), numerator, denominator);
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                                   timelineModel.getTimeSignatureNumerator(),
                                   timelineModel.getTimeSignatureDenominator(),
                                   timelineModel.getMusicalKey());
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onKeyChanged = [this](const juce::String& key)
    {
        timelineModel.setMusicalKey(key);
        trackerPanel.refreshTimelineView();
        projectDirty = true;
        saveSessionToDisk();
    };

    trackerPanel.onPitchPipeTriggered = [this](double noteHz)
    {
        constexpr double sampleRate = 44100.0;
        constexpr double durationSeconds = 2.0;
        const int numSamples = (int) (sampleRate * durationSeconds);

        juce::AudioBuffer<float> toneBuffer(1, numSamples);
        toneBuffer.clear();
        auto* samples = toneBuffer.getWritePointer(0);

        for (int i = 0; i < numSamples; ++i)
        {
            double t = (double) i / sampleRate;
            double attack = juce::jmin(1.0, t / 0.03);
            double release = juce::jmax(0.0, 1.0 - ((t - 1.5) / 0.5));
            double env = attack * (t > 1.5 ? release : 1.0);

            double wave = 0.70 * std::sin(2.0 * juce::MathConstants<double>::pi * noteHz * t)
                        + 0.22 * std::sin(4.0 * juce::MathConstants<double>::pi * noteHz * t)
                        + 0.08 * std::sin(6.0 * juce::MathConstants<double>::pi * noteHz * t);
            samples[i] = (float) (wave * env * 0.4);
        }

        juce::String error;
        if (engine.previewGeneratedBuffer(toneBuffer, sampleRate, error))
            transportBar.setStatusText("Pitch Pipe: " + juce::String(juce::roundToInt(noteHz)) + " Hz tone played");
        else if (error.isNotEmpty())
            transportBar.setStatusText("Pitch Pipe error: " + error);
    };

    signalLabPanel.onPreviewRequested = [this](const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& suggestedName)
    {
        juce::String errorMessage;
        if (engine.previewGeneratedBuffer(buffer, sampleRate, errorMessage))
            transportBar.setStatusText("Previewing signal: " + suggestedName);
        else if (errorMessage.isNotEmpty())
            transportBar.setStatusText(errorMessage);
    };

    signalLabPanel.onStopRequested = [this]
    {
        engine.stopAssetPreview();
        transportBar.setStatusText("Stopped signal preview.");
    };

    signalLabPanel.onAudioSettingsRequested = [this]
    {
        showAudioSettings();
    };

    signalLabPanel.onMidiLearnRequested = [this](const juce::String& displayLabel, bool wantsContinuousControl,
                                                 std::function<void(juce::String, int, int, bool)> onLearned)
    {
        auto expectedKind = wantsContinuousControl ? WorkstationAudioEngine::MidiLearnKind::Continuous
                                                    : WorkstationAudioEngine::MidiLearnKind::Discrete;
        requestGenericMidiLearn(displayLabel, std::move(onLearned), expectedKind);
    };

    // Signal Lab live playback -- thin passthrough to the matching
    // WorkstationAudioEngine wrapper methods (see PatchLiveVoice.h for the
    // actual design). Same callback-injection pattern as
    // onPreviewRequested/onRenderRequested above.
    signalLabPanel.onLiveGraphRebuildRequested = [this](const cw::PatchDocument& patch, const PatchLiveBindingMap& liveBindings)
    {
        engine.rebuildSignalLabLiveGraph(patch, liveBindings);
    };
    signalLabPanel.onLiveStartRequested = [this](double durationSeconds)
    {
        engine.startSignalLabLivePlayback(durationSeconds);
    };
    signalLabPanel.onLiveStopRequested = [this]
    {
        engine.stopSignalLabLivePlayback();
    };
    signalLabPanel.onLiveIsActiveRequested = [this]
    {
        return engine.isSignalLabLivePlaybackActive();
    };
    signalLabPanel.onLiveFinishedFlagRequested = [this]
    {
        return engine.takeSignalLabLivePlaybackFinishedFlag();
    };
    signalLabPanel.onLiveMidiValueChanged = [this](const juce::String& nodeId, float value)
    {
        engine.setSignalLabLiveMidiValue(nodeId, value);
    };
    signalLabPanel.onMidiFaderFeedbackRequested = [this](int channel, float value)
    {
        midiSurface.sendRawFaderFeedback(channel, value);
    };
    signalLabPanel.onFaderChannelClaimsChanged = [this](const juce::Array<int>& channels)
    {
        midiSurface.setClaimedFaderChannels(channels);
    };
    signalLabPanel.onLiveScopeSamplesRequested = [this](const juce::String& nodeId, juce::AudioBuffer<float>& dest, int numSamples)
    {
        return engine.copySignalLabLiveScopeSamples(nodeId, dest, numSamples);
    };
    signalLabPanel.onLiveScopeTapsChanged = [this](const juce::Array<juce::String>& tapNodeIds)
    {
        engine.updateSignalLabLiveScopeTaps(tapNodeIds);
    };
    signalLabPanel.onLiveScopeSampleRateRequested = [this]
    {
        return engine.getSignalLabLiveSampleRate();
    };

    signalLabPanel.onUndoCheckpointRequested = [this](const juce::ValueTree& stateBeforeEdit, const juce::String& label)
    {
        pushSignalUndoState(stateBeforeEdit, label);
        // Fires on every real Signal Lab edit (add/move/wire a node, change
        // a value) -- every other panel's equivalent "something changed"
        // callback already does this (see trackerPanel.onTempoChanged etc.),
        // but Signal Lab never did, so quitting after only touching Signal
        // Lab skipped confirmCloseApplication()'s save-prompt entirely.
        projectDirty = true;
    };

    signalLabPanel.onInteractionStarted = [this]
    {
        undoService.setActiveContext(signalUndoContextId);
    };

    signalLabPanel.onRenderRequested = [this](const juce::AudioBuffer<float>& buffer, double sampleRate, const juce::String& suggestedName)
    {
        juce::String projectError;
        if (! ensureProjectSessionActive(projectError))
        {
            transportBar.setStatusText(projectError.isNotEmpty() ? projectError : "Could not initialize project for rendered sounds.");
            return;
        }

        juce::String errorMessage;
        auto assetSlug = slugForProjectAssetName(suggestedName);
        auto logicalPath = creation::assets::ProjectContainerPaths::derivedAssetRoot
                         + assetSlug + "-" + makeRecordingTimestamp() + ".wav";

        juce::MemoryBlock fileData;
        if (writeWavData(fileData, buffer, sampleRate, errorMessage))
        {
            if (! projectSession.writeEntry(logicalPath, fileData, juce::Time::getCurrentTime()))
            {
                transportBar.setStatusText("Could not write the rendered WAV into the project.");
                return;
            }

            creation::assets::AssetDescriptor renderedAsset;
            renderedAsset.id = "asset:" + juce::Uuid().toString();
            renderedAsset.version = "1";
            renderedAsset.versionId = renderedAsset.id + "@1";
            renderedAsset.displayName = suggestedName.isNotEmpty() ? suggestedName + " Render" : "Signal Render";
            renderedAsset.logicalPath = logicalPath;
            renderedAsset.kind = creation::assets::AssetKind::render;
            renderedAsset.mediaType = "audio/wav";
            renderedAsset.fileSizeBytes = (int64) fileData.getSize();
            renderedAsset.createdAt = renderedAsset.modifiedAt = juce::Time::getCurrentTime();
            renderedAsset.sourceApp = "Djehuti Station";
            renderedAsset.description = "Rendered from Signal Lab on command.";
            projectSession.upsertAssetDescriptor(renderedAsset);

            if (! projectSession.commit(errorMessage))
            {
                transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not save the rendered WAV asset.");
                return;
            }

            refreshProjectAssets();
            refreshContentLibrary();
            saveSessionToDisk(true);
            transportBar.setStatusText("Rendered Signal Lab WAV asset: " + renderedAsset.displayName);
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    signalLabPanel.onPatchExportRequested = [this](const juce::String& patchJson, const juce::String& suggestedName)
    {
        auto defaultName = slugForProjectAssetName(suggestedName) + ".cspatch";
        rawAssetExportChooser = std::make_unique<juce::FileChooser>("Export Signal Lab sound",
                                                                    juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                        .getChildFile(defaultName),
                                                                    "*.cspatch",
                                                                    true);
        auto chooser = rawAssetExportChooser.get();
        chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                             [this, chooser, patchJson](const juce::FileChooser& result)
                             {
                                 auto destination = result.getResult();
                                 if (chooser == rawAssetExportChooser.get())
                                     rawAssetExportChooser.reset();

                                 if (destination.getFullPathName().isEmpty())
                                     return;

                                 if (destination.getFileExtension().isEmpty())
                                     destination = destination.withFileExtension(".cspatch");

                                 if (! destination.replaceWithText(patchJson))
                                 {
                                     transportBar.setStatusText("Could not export the Signal Lab sound file.");
                                     return;
                                 }

                                 transportBar.setStatusText("Exported Signal Lab sound file: " + destination.getFileName());
                             });
    };

    signalLabPanel.onPatchSaveToLibraryRequested = [this](const juce::String& patchJson, const juce::String& suggestedName)
    {
        if (! projectSession.isValid())
        {
            juce::String projectError;
            if (! creation::assets::ProjectWorkspaceService::createProject(suiteSettings, creation::assets::SuiteAppDomain::station, "New Project", "1.0.0", "1.0.0", projectSession, projectError))
            {
                transportBar.setStatusText("Could not create a project for this sound.");
                return;
            }

            transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        }

        juce::MemoryBlock patchData(patchJson.toRawUTF8(), (size_t) patchJson.getNumBytesAsUTF8());

        creation::assets::ProjectAssetService::ImportOptions options;
        options.kind = creation::assets::AssetKind::patch;
        options.displayName = suggestedName.isNotEmpty() ? suggestedName : "Signal Design";
        options.logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot + slugForProjectAssetName(suggestedName) + ".cspatch";
        options.mediaType = "application/x-creation-station-patch";
        options.sourceApp = "Djehuti Station";
        options.sourceTool = "Signal Lab";
        options.description = "Editable Signal Lab sound design object.";

        creation::assets::AssetDescriptor patchAsset;
        juce::String errorMessage;
        if (! creation::assets::ProjectAssetService::saveGeneratedAsset(projectSession, patchData, options, patchAsset, errorMessage))
        {
            auto message = errorMessage.isNotEmpty() ? errorMessage : "Could not save the Signal Lab design asset.";
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Save Failed", message);
            transportBar.setStatusText("Save failed.");
            return;
        }

        currentSignalLabAssetId = patchAsset.id;

        if (! projectSession.commit(errorMessage))
        {
            auto message = errorMessage.isNotEmpty() ? errorMessage : "Could not save the Signal Lab design asset.";
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Commit Failed", message);
            transportBar.setStatusText("Commit failed.");
            return;
        }

        refreshProjectAssets();
        refreshContentLibrary();
        // Any Signal clip built from this patch must be re-rendered from the new patch content.
        signalRenderBytes.clear();
        signalPatchDocs.clear();
        refreshTrackerPlaybackClips();
        saveSessionToDisk(true);
        transportBar.setStatusText("Saved Signal Lab design asset: " + patchAsset.displayName);
    };

    signalLabPanel.onPatchLoadRequested = [this]
    {
        if (! projectSession.isValid())
        {
            transportBar.setStatusText("Open or create a project first so Signal Lab can load saved sounds from the project.");
            return;
        }

        auto patchAssets = projectSession.getManifest().assetCatalog.query({ creation::assets::AssetKind::patch });

        if (patchAssets.isEmpty())
        {
            transportBar.setStatusText("This project does not have any saved Signal Lab sounds yet.");
            return;
        }

        std::sort(patchAssets.begin(), patchAssets.end(), [](const auto& left, const auto& right)
        {
            if (left.modifiedAt != right.modifiedAt)
                return left.modifiedAt > right.modifiedAt;

            return left.displayName.compareIgnoreCase(right.displayName) < 0;
        });

        juce::PopupMenu menu;
        menu.addSectionHeader("Load Sound From Project");
        for (int index = 0; index < patchAssets.size(); ++index)
        {
            const auto& asset = patchAssets.getReference(index);
            auto label = asset.displayName.isNotEmpty() ? asset.displayName : asset.logicalPath;
            auto detail = asset.modifiedAt != juce::Time()
                ? "  •  " + asset.modifiedAt.toString(true, true)
                : juce::String{};
            menu.addItem(index + 1, label + detail);
        }

        auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }),
                           [this, patchAssets](int result)
                           {
                               if (result <= 0 || result > patchAssets.size())
                                   return;

                               openProjectAsset(patchAssets.getReference(result - 1));
                           });
    };

    foleyPanel.onSetupSaveRequested = [this](const juce::String& name)
    {
        if (! projectSession.isValid())
        {
            transportBar.setStatusText("Open or create a project first so Foley can save setups into it.");
            return;
        }

        auto frgraphText = foleyPanel.serializeGraph();
        juce::MemoryBlock data(frgraphText.toRawUTF8(), frgraphText.getNumBytesAsUTF8());

        creation::assets::ProjectAssetService::ImportOptions options;
        options.kind = creation::assets::AssetKind::foleyPatch;
        options.displayName = name;
        options.logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot + slugForProjectAssetName(name) + ".frgraph";
        options.mediaType = "application/x-creation-node-graph";
        options.sourceApp = "Djehuti Station";
        options.sourceTool = "Foley";
        options.description = "Saved Foley node-graph setup.";

        creation::assets::AssetDescriptor savedAsset;
        juce::String errorMessage;
        if (! creation::assets::ProjectAssetService::saveGeneratedAsset(projectSession, data, options, savedAsset, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Save Failed", "Could not save Foley setup:\n" + errorMessage);
            transportBar.setStatusText("Save failed.");
            return;
        }

        currentFoleyAssetId = savedAsset.id;

        if (! projectSession.commit(errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::AlertWindow::WarningIcon, "Commit Failed", "Foley setup saved but project commit failed:\n" + errorMessage);
            transportBar.setStatusText("Commit failed.");
            return;
        }

        refreshProjectAssets();
        saveSessionToDisk(true);
        transportBar.setStatusText("Saved Foley setup: " + savedAsset.displayName);
    };

    foleyPanel.onSetupLoadRequested = [this]
    {
        if (! projectSession.isValid())
        {
            transportBar.setStatusText("Open or create a project first so Foley can load saved setups from it.");
            return;
        }

        auto setupAssets = projectSession.getManifest().assetCatalog.query({ creation::assets::AssetKind::foleyPatch });
        if (setupAssets.isEmpty())
        {
            transportBar.setStatusText("This project does not have any saved Foley setups yet.");
            return;
        }

        std::sort(setupAssets.begin(), setupAssets.end(), [](const auto& left, const auto& right)
        {
            if (left.modifiedAt != right.modifiedAt)
                return left.modifiedAt > right.modifiedAt;
            return left.displayName.compareIgnoreCase(right.displayName) < 0;
        });

        juce::PopupMenu menu;
        menu.addSectionHeader("Load Setup From Project");
        for (int index = 0; index < setupAssets.size(); ++index)
        {
            const auto& asset = setupAssets.getReference(index);
            auto label = asset.displayName.isNotEmpty() ? asset.displayName : asset.logicalPath;
            auto detail = asset.modifiedAt != juce::Time() ? "  •  " + asset.modifiedAt.toString(true, true) : juce::String{};
            menu.addItem(index + 1, label + detail);
        }

        auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }),
                           [this, setupAssets](int result)
                           {
                               if (result <= 0 || result > setupAssets.size())
                                   return;

                               const auto& asset = setupAssets.getReference(result - 1);
                               if (! restoreFoleyAsset(asset))
                               {
                                   transportBar.setStatusText("Could not load Foley setup: " + asset.displayName);
                                   return;
                               }

                               saveSessionToDisk(true);
                               transportBar.setStatusText("Loaded Foley setup: " + asset.displayName);
                           });
    };

    foleyPanel.onPodBuildRequested = [this](const juce::String& podName, const juce::String& source)
    {
        if (! projectSession.isValid())
        {
            foleyPanel.setBuildStatus("Open or create a project before building a Foley pod.", false);
            return;
        }
        juce::String status;
        const bool ok = frustPodService.buildGeneratedNodePod(projectSession, podName, source,
                                                               foleyPanel.nodeLibraries(), status);
        if (ok)
        {
            foleyPanel.refreshNodePalette();
            refreshProjectAssets();
        }
        foleyPanel.setBuildStatus(status, ok);
        transportBar.setStatusText(status);
    };

    foleyPanel.onRegistryPodLoadRequested = [this](const juce::String& podName, const juce::String& version)
    {
        juce::String status;
        const bool ok = frustPodService.loadRegistryNodePod(projectSession, podName, version,
                                                            foleyPanel.nodeLibraries(), status);
        if (ok) foleyPanel.refreshNodePalette();
        foleyPanel.setBuildStatus(status, ok);
        transportBar.setStatusText(status);
    };

    dslPanel.onCompileRequested = [this](const juce::String& sourceText) -> DslPanel::CompileOutcome
    {
        DslPanel::CompileOutcome outcome;
        if (! projectSession.isValid())
        {
            outcome.output = "Open or create a project first: FRust source is kept in the project.";
            return outcome;
        }
        const auto result = frustPodService.checkScript(projectSession, sourceText);
        outcome.ok = result.ok;
        outcome.output = result.output;
        return outcome;
    };

    dslPanel.onSourceExportRequested = [this](const juce::String& sourceText, const juce::String& suggestedName)
    {
        auto startDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
        frustSourceChooser = std::make_unique<juce::FileChooser>("Export FRust source",
                                                                    startDirectory.getChildFile(suggestedName + ".frust"),
                                                                    "*.frust");

        frustSourceChooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                                           [this, sourceText](const juce::FileChooser& chooser)
                                           {
                                               auto file = chooser.getResult();
                                               frustSourceChooser.reset();

                                               if (file == juce::File())
                                                   return;

                                               if (file.replaceWithText(sourceText))
                                                   transportBar.setStatusText("Exported FRust source: " + file.getFileName());
                                               else
                                                   transportBar.setStatusText("Could not write " + file.getFileName());
                                           });
    };

    dslPanel.onSourceSaveToLibraryRequested = [this](const juce::String& sourceText, const juce::String& suggestedName)
    {
        creation::services::SuiteVfsServiceClient client;
        if (! client.discover())
        {
            transportBar.setStatusText("Could not reach the suite VFS service.");
            return;
        }

        auto logicalPath = "library/frust/" + slugForProjectAssetName(suggestedName) + ".frust";
        const juce::MemoryBlock data(sourceText.toRawUTF8(), sourceText.getNumBytesAsUTF8());
        if (client.writeEntry(logicalPath, data))
            transportBar.setStatusText("Saved to your library: " + suggestedName);
        else
            transportBar.setStatusText("Could not save " + suggestedName + " to your library.");
    };

    auto showFrustLoadFromLibraryMenu = [this]
    {
        creation::services::SuiteVfsServiceClient client;
        if (! client.discover())
        {
            transportBar.setStatusText("Could not reach the suite VFS service.");
            return;
        }

        juce::StringArray allPaths;
        client.listEntries(allPaths);

        juce::StringArray frustPaths;
        for (const auto& path : allPaths)
            if (path.startsWith("library/frust/"))
                frustPaths.add(path);

        if (frustPaths.isEmpty())
        {
            transportBar.setStatusText("Your library has no saved FRust sources yet.");
            return;
        }

        frustPaths.sort(true);

        juce::PopupMenu menu;
        menu.addSectionHeader("Load From Your Library");
        for (int index = 0; index < frustPaths.size(); ++index)
        {
            auto displayName = juce::File(frustPaths[index]).getFileNameWithoutExtension();
            menu.addItem(index + 1, displayName);
        }

        auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
        menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }),
                           [this, frustPaths](int result)
                           {
                               if (result <= 0 || result > frustPaths.size())
                                   return;

                               creation::services::SuiteVfsServiceClient loadClient;
                               if (! loadClient.discover())
                               {
                                   transportBar.setStatusText("Could not reach the suite VFS service.");
                                   return;
                               }

                               juce::MemoryBlock data;
                               if (! loadClient.readEntry(frustPaths[result - 1], data))
                               {
                                   transportBar.setStatusText("Could not load that library item.");
                                   return;
                               }

                               auto sourceText = juce::String::createStringFromData(data.getData(), (int) data.getSize());
                               dslPanel.setSourceText(sourceText);
                               transportBar.setStatusText("Loaded from your library: "
                                                          + juce::File(frustPaths[result - 1]).getFileNameWithoutExtension());
                           });
    };

    auto showFrustLoadFromDiskChooser = [this]
    {
        auto startDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);

        frustSourceChooser = std::make_unique<juce::FileChooser>("Load a FRust source file",
                                                                    startDirectory,
                                                                    "*.frust");

        frustSourceChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                           [this](const juce::FileChooser& chooser)
                                           {
                                               auto file = chooser.getResult();
                                               frustSourceChooser.reset();

                                               if (! file.existsAsFile())
                                                   return;

                                               dslPanel.loadSourceFromFile(file);
                                               transportBar.setStatusText("Loaded FRust source: " + file.getFileName());
                                               setWorkspaceMode(WorkspaceMode::code);
                                           });
    };

    dslPanel.onSourceLoadRequested = [this, showFrustLoadFromLibraryMenu, showFrustLoadFromDiskChooser]
    {
        juce::PopupMenu menu;
        menu.addItem(1, "Load From Your Library...");
        menu.addItem(2, "Load From File...");

        menu.showMenuAsync(juce::PopupMenu::Options(),
                           [showFrustLoadFromLibraryMenu, showFrustLoadFromDiskChooser](int result)
                           {
                               if (result == 1)
                                   showFrustLoadFromLibraryMenu();
                               else if (result == 2)
                                   showFrustLoadFromDiskChooser();
                           });
    };

    contextEngine.onContextReady = [this](const CreationStationContextEngine::ContextPacket& packet)
    {
        pendingAiContextPacket = packet;
        pendingAiContextPacketValid = true;
        aiPanel.setContextPacket(packet);
        aiPanel.setTaskPlan(taskPlanner.buildPlan(packet.request.prompt, packet));
        transportBar.setStatusText("AI context packet and task plan ready.");
        if (pendingAiPrompt.isNotEmpty() && ! aiCompletionInFlight)
            launchAiCompletion(packet);
    };

    aiPanel.onModeChanged = [this](AiPanel::GuidanceMode mode)
    {
        juce::String status;
        switch (mode)
        {
            case AiPanel::GuidanceMode::normal: status = "AI mode: Normal."; break;
            case AiPanel::GuidanceMode::learn: status = "AI mode: Learn."; break;
            case AiPanel::GuidanceMode::research: status = "AI mode: Research."; break;
        }

        transportBar.setStatusText(status);
    };

    aiPanel.onAccessChanged = [this](AiPanel::AccessLevel level)
    {
        juce::String status = "AI access: ";
        switch (level)
        {
            case AiPanel::AccessLevel::askFirst: status += "Ask first"; break;
            case AiPanel::AccessLevel::appOnly: status += "App only"; break;
            case AiPanel::AccessLevel::fileChanges: status += "Files"; break;
            case AiPanel::AccessLevel::fullAccess: status += "Full access"; break;
        }

        transportBar.setStatusText(status + ".");
    };

    aiPanel.onModelChanged = [this](const juce::String& modelName)
    {
        selectAiAccountForStation(aiPanel.getSelectedAccountId(), modelName.trim());
    };

    aiPanel.onAccountChanged = [this](const juce::String& accountId)
    {
        selectAiAccountForStation(accountId);
        transportBar.setStatusText("AI account: " + aiProviderSettings.providerDisplayName + ".");
    };

    aiPanel.onChatsRequested = [this] { showConversationManager(); };
    aiPanel.onNewConversationRequested = [this] { startNewConversation(); };

    aiPanel.onPromptSubmitted = [this](const juce::String& submittedPrompt)
    {
        refreshAiContextStore();
        pendingAiPrompt = submittedPrompt;
        pendingAiQuestion = aiPanel.getLastQuestion();
        recordConversationTurn("user", pendingAiQuestion);   // saved before the request starts

        CreationStationContextEngine::RetrievalRequest request;
        request.prompt = pendingAiPrompt;
        request.workspaceMode = workspaceModeName(activeMode).toLowerCase();
        request.projectName = projectSession.isValid() ? projectSession.getManifest().projectName : juce::String();
        request.maxItems = 6;

        contextEngine.submitRequest(request);
        transportBar.setStatusText("Building AI context packet...");
    };

    aiPanel.onExecuteNextStep = [this](const CreationStationTaskPlanner::TaskStep& step)
    {
        executeAiTaskStep(step);
    };
    aiPanel.onCollapsedChanged = [this](bool shouldCollapse)
    {
        aiSidebarCollapsed = shouldCollapse;
        resized();
    };

    settingsPanel.onNewProjectRequested = [this] { createNewProject(); };
    settingsPanel.onOpenProjectRequested = [this] { openProject(); };
    settingsPanel.onSaveProjectRequested = [this] { saveProject(); };
    settingsPanel.onRevealProjectFolderRequested = [this] { revealProjectFolder(); };
    settingsPanel.onChangeStorageRequested = [this] { suiteShellController.showSuiteSettings(); };
    settingsPanel.onProjectMetadataChanged = [this](const creation::assets::ProjectManifest& metadata)
    {
        juce::String errorMessage;
        if (! true)
        {
            transportBar.setStatusText(errorMessage);
            return;
        }

        settingsPanel.setProjectMetadata(projectSession.getManifest());
        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        saveSessionToDisk(true);

        if (! projectDirty)
            transportBar.setStatusText("Project metadata updated.");
    };
    settingsPanel.onOpenAudioRequested = [this]
    {
        showAudioSettings();
    };
    settingsPanel.onOpenDriverControlPanelRequested = [this]
    {
        if (auto* device = deviceManager.getCurrentAudioDevice())
        {
            if (device->hasControlPanel())
            {
                device->showControlPanel();
                refreshAudioDeviceSettingsView();
                refreshTrackInputSources();
                transportBar.setStatusText("Opened audio driver panel.");
                return;
            }
        }

        transportBar.setStatusText("The active audio driver does not expose a control panel.");
        refreshAudioDeviceSettingsView();
    };
    settingsPanel.onRefreshStudioInputsRequested = [this]
    {
        refreshAudioDeviceSettingsView();
        refreshTrackInputSources();
        transportBar.setStatusText("Studio inputs refreshed.");
    };
    settingsPanel.onAudioSystemChanged = [this](const juce::String& audioSystem)
    {
        setAudioSystem(audioSystem);
    };
    settingsPanel.onAudioInputDeviceChanged = [this](const juce::String& inputDeviceName)
    {
        setAudioInputDevice(inputDeviceName);
    };
    settingsPanel.onAudioOutputDeviceChanged = [this](const juce::String& outputDeviceName)
    {
        setAudioOutputDevice(outputDeviceName);
    };
    settingsPanel.onStudioInputNameChanged = [this](int inputIndex, const juce::String& inputName)
    {
        studioIOModel.setInputName(inputIndex, inputName);
        juce::Array<juce::String> trackerInputNames;
        for (const auto& name : studioIOModel.getNames())
            trackerInputNames.add(name);
        trackerPanel.setInputSources(trackerInputNames);
        syncTrackViews();
        saveAppSettings();
        transportBar.setStatusText("Studio input renamed.");
    };
    settingsPanel.onManageVstPathsRequested = [this]
    {
        // Route to the real management UI (add/remove/import-list/rescan) rather than
        // configureVstSearchPaths()'s bare single-folder browse-and-append flow -- that flow
        // stays as PluginsPanel's own "Add Folder" mechanism, not the whole feature.
        setWorkspaceMode(WorkspaceMode::plugins);
    };
    settingsPanel.onManageControlSurfaceMappingsRequested = [this]
    {
        editControlSurfaceMappings();
    };
    settingsPanel.onRefreshMidiDevicesRequested = [this]
    {
        refreshMidiDeviceSettings();
    };
    settingsPanel.onMidiInputDeviceEnabledChanged = [this](const juce::String& deviceId, bool enabled)
    {
        deviceManager.setMidiInputDeviceEnabled(deviceId, enabled);

        if (enabled)
            disabledMidiInputDeviceIds.removeString(deviceId);
        else
            disabledMidiInputDeviceIds.addIfNotAlreadyThere(deviceId);

        saveAppSettings();
        refreshMidiDeviceSettings();
    };
    settingsPanel.onMidiInputDeviceRouteChanged = [this](const juce::String& deviceId, int trackIndexOrMinusOne)
    {
        // Only one track can own a given device at a time - clear it from whichever track had
        // it before applying the new choice.
        for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
            if (engine.getTrackMidiInputDeviceId(trackIndex) == deviceId)
                engine.setTrackMidiInputDeviceId(trackIndex, {});

        if (juce::isPositiveAndBelow(trackIndexOrMinusOne, engine.getTrackCount()))
            engine.setTrackMidiInputDeviceId(trackIndexOrMinusOne, deviceId);

        projectDirty = true;
        saveSessionToDisk();
        refreshMidiDeviceSettings();
    };
    settingsPanel.onAutoloadChanged = [this](bool enabled)
    {
        juce::ignoreUnused(enabled);
        autoloadLastProject = true;
        settingsPanel.setAutoloadEnabled(true);
        saveAppSettings();
    };
    contentPanel.onRefreshRequested = [this]
    {
        refreshContentLibrary();
    };

    contentPanel.onOpenContentFolderRequested = [this]
    {
        if (! ensureStorageRootConfigured())
            return;

        creation::suite::getContentDirectory(suiteSettings).revealToUser();
    };

    contentPanel.onAdminPublishRequested = [this]
    {
        if (! authenticated || ! isAdminRole(authSession.getSession().user.role))
        {
            transportBar.setStatusText("Admin publishing is only available to admin accounts.");
            return;
        }

        if (! ensureStorageRootConfigured())
            return;

        contentUploadChooser = std::make_unique<juce::FileChooser>("Choose a content package to publish",
                                                                   creation::suite::getContentDirectory(suiteSettings),
                                                                   "*.cspatch;*.cspack;*.zip;*.wav");

        contentUploadChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                                          [this](const juce::FileChooser& chooser)
                                          {
                                              auto selectedFile = chooser.getResult();
                                              contentUploadChooser.reset();

                                              if (! selectedFile.existsAsFile())
                                                  return;

                                              auto* dialog = new juce::AlertWindow("Admin Publish Content",
                                                                                   "Enter content metadata for upload.",
                                                                                   juce::MessageBoxIconType::QuestionIcon);
                                              dialog->addTextEditor("name", selectedFile.getFileNameWithoutExtension());
                                              dialog->addTextEditor("type", selectedFile.hasFileExtension(".cspatch") ? "patch"
                                                                                     : selectedFile.hasFileExtension(".cspack") ? "pack"
                                                                                     : selectedFile.hasFileExtension(".wav") ? "sample-pack"
                                                                                     : "pack");
                                              dialog->addTextEditor("version", "0.1.0");
                                              dialog->addTextEditor("description", "Published from Djehuti Station.");
                                              dialog->addTextEditor("tags", "creation-station");
                                              dialog->addTextEditor("tier", "");
                                              dialog->addTextEditor("minAppVersion", "0.2.0");
                                              dialog->addButton("Publish", 1);
                                              dialog->addButton("Cancel", 0);

                                              auto safeThis = juce::Component::SafePointer<MainComponent>(this);
                                              dialog->enterModalState(true, juce::ModalCallbackFunction::create(
                                                  [safeThis, dialog, selectedFile](int result) mutable
                                                  {
                                                      std::unique_ptr<juce::AlertWindow> ownedDialog(dialog);
                                                      if (result != 1 || safeThis == nullptr)
                                                          return;

                                                      ContentApiClient::AdminUploadRequest request;
                                                      request.productSlug = "creation-station";
                                                      request.name = ownedDialog->getTextEditorContents("name").trim();
                                                      request.itemType = ownedDialog->getTextEditorContents("type").trim();
                                                      request.version = ownedDialog->getTextEditorContents("version").trim();
                                                      request.description = ownedDialog->getTextEditorContents("description").trim();
                                                      request.tags.addTokens(ownedDialog->getTextEditorContents("tags"), ",", "\"");
                                                      request.tags.trim();
                                                      request.tags.removeEmptyStrings();
                                                      request.requiredTierId = ownedDialog->getTextEditorContents("tier").trim();
                                                      request.minAppVersion = ownedDialog->getTextEditorContents("minAppVersion").trim();
                                                      request.fileType = selectedFile.getFileExtension().trimCharactersAtStart(".").toLowerCase();
                                                      request.packageFile = selectedFile;

                                                      safeThis->transportBar.setStatusText("Publishing content to LagDaemon...");
                                                      auto token = safeThis->authSession.getSession().token;

                                                      std::thread([safeThis, token, request]()
                                                      {
                                                          juce::String errorMessage;
                                                          juce::String createdId;

                                                          if (! safeThis->contentApiClient.createAdminContent(token, request, createdId, errorMessage)
                                                              || ! safeThis->contentApiClient.uploadAdminContentFile(token, createdId, request.packageFile, errorMessage))
                                                          {
                                                              juce::MessageManager::callAsync([safeThis, errorMessage]
                                                              {
                                                                  if (safeThis != nullptr)
                                                                      safeThis->transportBar.setStatusText(errorMessage);
                                                              });
                                                              return;
                                                          }

                                                          juce::MessageManager::callAsync([safeThis]
                                                          {
                                                              if (safeThis != nullptr)
                                                              {
                                                                  safeThis->transportBar.setStatusText("Content published to LagDaemon.");
                                                                  safeThis->refreshContentLibrary();
                                                              }
                                                          });
                                                      }).detach();
                                                  }), true);
                                          });
    };

    contentPanel.onDownloadRequested = [this](const ContentLibrary::Item& item)
    {
        downloadContentItem(item);
    };

    contentPanel.onRevealItemRequested = [this](const ContentLibrary::Item& item)
    {
        activateContentItem(item);
    };

    contentPanel.onOpenProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        openProjectAsset(asset);
    };

    contentPanel.onPlaceProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        placeProjectAssetOnTracker(asset);
    };

    // The header's small status label is gone: an error there went unnoticed for an hour. Errors get a
    // dialog; everything else gets a toast that is large enough to read and clears itself.
    addChildComponent(toast);
    transportBar.onErrorStatus = [this](const juce::String& message) { reportError(message); };
    transportBar.onInfoStatus = [this](const juce::String& message) { showToast(message); };
    transportBar.setStatusLabelVisible(false);
    contentPanel.onErrorStatus = [this](const juce::String& message) { reportError(message); };
    contentPanel.onPreviewProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        toggleProjectAssetPreview(asset);
    };
    contentPanel.onExportProjectAssetRequested = [this](const creation::assets::AssetDescriptor& asset)
    {
        exportProjectAssetRaw(asset);
    };

    contentPanel.onLaunchTutorialRequested = [this](const ContentPanel::TutorialItem& item)
    {
        launchTutorialItem(item);
    };

    contentPanel.onRevealTutorialRequested = [this](const ContentPanel::TutorialItem& item)
    {
        if (item.file.existsAsFile())
        {
            item.file.revealToUser();
            transportBar.setStatusText("Revealed tutorial: " + item.file.getFileName());
        }
    };

    scorePanel.onPlayRequested = [this](const ScorePanel::PlaybackRequest& request)
    {
        if (request.notes.isEmpty())
        {
            transportBar.setStatusText("Add a few notes to the score first.");
            return;
        }

        auto buffer = renderScorePreviewBuffer(request, 48000.0);
        juce::String errorMessage;

        engine.stopAssetPreview();
        engine.setPlaying(false);

        if (engine.previewGeneratedBuffer(buffer, 48000.0, errorMessage))
        {
            transportBar.setStatusText("Previewing score: " + request.songTitle);
        }
        else if (errorMessage.isNotEmpty())
        {
            transportBar.setStatusText(errorMessage);
        }
    };

    pluginRackBar.onLoadPlugin = [this, refreshVisibleBank]
    {
        showPluginLoadMenu([this, refreshVisibleBank](const juce::File& file)
        {
            loadPluginIntoCurrentInsert(file);
            refreshVisibleBank();
        });
    };

    pluginRackBar.onManagePluginPaths = [this]
    {
        setWorkspaceMode(WorkspaceMode::plugins);
    };

    pluginRackBar.onUnloadPlugin = [this, refreshVisibleBank]
    {
        if (pluginRackBar.isTrackContext())
        {
            closePluginEditorWindowsForTrack(pluginRackBar.getTrackIndex());
            engine.unloadTrackPlugin(pluginRackBar.getTrackIndex());
        }
        else
        {
            closePluginEditorWindow("master-plugin");
            engine.unloadMasterPlugin();
        }

        refreshVisibleBank();
    };

    pluginRackBar.onBypassChanged = [this, refreshVisibleBank](bool shouldBypass)
    {
        if (pluginRackBar.isTrackContext())
            engine.setTrackPluginBypassed(pluginRackBar.getTrackIndex(), shouldBypass);
        else
            engine.setMasterPluginBypassed(shouldBypass);

        refreshVisibleBank();
    };

    pluginRackBar.onOpenPluginEditor = [this]
    {
        const auto isTrackContext = pluginRackBar.isTrackContext();
        const auto hasPlugin = isTrackContext ? engine.hasTrackPlugin(pluginRackBar.getTrackIndex())
                                              : engine.hasMasterPlugin();

        if (! hasPlugin)
            return;

        if (isTrackContext)
        {
            auto trackIndex = pluginRackBar.getTrackIndex();
            auto windowKey = "track-rack-" + juce::String(trackIndex);
            if (auto* existingWindow = findPluginEditorWindow(windowKey))
            {
                existingWindow->toFront(true);
                return;
            }

            if (auto* editor = engine.createTrackPluginEditor(trackIndex))
            {
                auto windowTitle = "Track " + juce::String(trackIndex + 1) + " Editor";
                auto window = std::make_unique<ManagedDocumentWindow>(windowTitle,
                                                                      juce::Colour(0xff11151c),
                                                                      juce::DocumentWindow::allButtons,
                                                                      [this, windowKey]
                                                                      {
                                                                          closePluginEditorWindow(windowKey);
                                                                      });
                window->setUsingNativeTitleBar(true);
                window->setResizable(true, true);
                window->setContentOwned(editor, true);
                window->centreWithSize(900, 650);
                window->setVisible(true);
                pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });
                pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
            }
        }
        else
        {
            constexpr auto* windowKey = "master-plugin";
            if (auto* existingWindow = findPluginEditorWindow(windowKey))
            {
                existingWindow->toFront(true);
                return;
            }

            if (auto* editor = engine.createMasterPluginEditor())
            {
                auto window = std::make_unique<ManagedDocumentWindow>("Master Editor",
                                                                      juce::Colour(0xff11151c),
                                                                      juce::DocumentWindow::allButtons,
                                                                      [this, windowKey]
                                                                      {
                                                                          closePluginEditorWindow(windowKey);
                                                                      });
                window->setUsingNativeTitleBar(true);
                window->setResizable(true, true);
                window->setContentOwned(editor, true);
                window->centreWithSize(900, 650);
                window->setVisible(true);
                pluginEditorWindows.push_back({ windowKey, -1, std::move(window) });
            }
        }
    };

    pluginRackBar.onOpenFxStack = [this]
    {
        showFxStackWindow();
    };

    pluginsPanel.onAddPathRequested = [this]
    {
        configureVstSearchPaths();
    };

    pluginsPanel.onImportPathListRequested = [this]
    {
        importVstPathList();
    };

    pluginsPanel.onRemovePathRequested = [this](int pathIndex)
    {
        auto currentPaths = vstPluginCatalog.getSearchPaths();
        if (! juce::isPositiveAndBelow(pathIndex, currentPaths.size()))
            return;

        currentPaths.remove(pathIndex);

        vstPluginCatalog.setSearchPaths(currentPaths);
        saveAppSettings();
        rescanVstCatalog();
        transportBar.setStatusText("Removed VST folder.");
    };

    pluginsPanel.onRescanRequested = [this]
    {
        rescanVstCatalog();
        transportBar.setStatusText(vstPluginCatalog.describeSummary());
    };

    pluginsPanel.onBuildSamplePackRequested = [this]
    {
        setWorkspaceMode(WorkspaceMode::sampler);
    };

    pluginsPanel.onLoadIntoInsertRequested = [this](const VstPluginCatalog::Entry& entry)
    {
        loadPluginIntoCurrentInsert(entry.file);
    };

    mixerPanel.onGainChanged = [this](int index, float value)
    {
        if (index == 8)
            engine.setMasterGain(value);
        else
        {
            engine.setTrackGain(index, value);
            recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackVolume, value);
        }

        if (index == 8)
            midiSurface.setMasterFaderValue(value);
        else
            midiSurface.setChannelGain(index, value);
    };

    mixerPanel.onBankOffsetChanged = [this, refreshVisibleBank](int)
    {
        refreshVisibleBank();
    };

    mixerPanel.onInsertButtonClicked = [this, refreshVisibleBank](int trackIndex)
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        refreshVisibleBank();
    };

    mixerPanel.onPanChanged = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
        {
            engine.setTrackPan(index, value);
            recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackPan, juce::jmap(value, -1.0f, 1.0f, 0.0f, 1.0f));
        }

        midiSurface.setChannelPan(index, value);
    };

    mixerPanel.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        midiSurface.setChannelMuted(index, muted);
    };

    mixerPanel.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        midiSurface.setChannelSoloed(index, soloed);
    };

    midiSurface.onBankStep = [this, refreshVisibleBank](int step)
    {
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank, step]
        {
            if (safeThis == nullptr)
                return;

            safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + step);
            refreshVisibleBank();
        });
    };

    midiSurface.onChannelSelected = [this, selectTrack](int trackIndex)
    {
        selectTrack(trackIndex);
    };

    midiSurface.onSpecialButtonPressed = [this, selectTrack, refreshVisibleBank](const juce::String& button)
    {
        auto advanceMode = [this](int step)
        {
            auto modeIndex = static_cast<int>(activeMode);
            constexpr int modeCount = static_cast<int>(WorkspaceMode::sampler) + 1;
            modeIndex = (modeIndex + step + modeCount) % modeCount;
            setWorkspaceMode(static_cast<WorkspaceMode>(modeIndex));
        };

        auto setMode = [this](WorkspaceMode mode)
        {
            setWorkspaceMode(mode);
        };

        if (button == creation::ui::surface_actions::cursorLeft)
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() - 1 : mixerPanel.getBankOffset();
            selectTrack(juce::jmax(0, trackIndex));
        }
        else if (button == creation::ui::surface_actions::cursorRight)
        {
            auto trackIndex = pluginRackBar.isTrackContext() ? pluginRackBar.getTrackIndex() + 1 : mixerPanel.getBankOffset();
            if (engine.getTrackCount() > 0)
                selectTrack(juce::jlimit(0, engine.getTrackCount() - 1, trackIndex));
        }
        else if (button == creation::ui::surface_actions::cursorUp)
        {
            advanceMode(-1);
        }
        else if (button == creation::ui::surface_actions::cursorDown)
        {
            advanceMode(1);
        }
        else if (button == creation::ui::surface_actions::assignTrack)
        {
            setMode(WorkspaceMode::mix);
        }
        else if (button == creation::ui::surface_actions::assignSend)
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == creation::ui::surface_actions::assignPan)
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == creation::ui::surface_actions::assignPlugin)
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == creation::ui::surface_actions::assignEq)
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == creation::ui::surface_actions::assignInstrument)
        {
            setMode(WorkspaceMode::score);
        }
        else if (button == "global_view")
        {
            setMode(WorkspaceMode::tracker);
        }
        else if (button == "view_midi_tracks")
        {
            setMode(WorkspaceMode::record);
        }
        else if (button == "view_inputs")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "view_audio_tracks")
        {
            setMode(WorkspaceMode::mix);
        }
        else if (button == "view_audio_instrument")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "view_aux")
        {
            setMode(WorkspaceMode::library);
        }
        else if (button == "view_busses")
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == "view_outputs")
        {
            setMode(WorkspaceMode::record);
        }
        else if (button == "view_user")
        {
            showAiSidebar();
        }
        else if (button == "f1")
        {
            showProjectMenu();
        }
        else if (button == "f2")
        {
            saveProject();
        }
        else if (button == "f3")
        {
            showAudioSettings();
        }
        else if (button == "f4")
        {
            setMode(WorkspaceMode::plugins);
        }
        else if (button == "f5")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "f6")
        {
            setMode(WorkspaceMode::signal);
        }
        else if (button == "f7")
        {
            setMode(WorkspaceMode::score);
        }
        else if (button == "f8")
        {
            showAiSidebar();
        }
        else if (button == "bank_left_full")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() - safeThis->mixerPanel.getVisibleChannelCount());
                refreshVisibleBank();
            });
        }
        else if (button == "bank_right_full")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + safeThis->mixerPanel.getVisibleChannelCount());
                refreshVisibleBank();
            });
        }
        else if (button == "channel_left")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() - 1);
                refreshVisibleBank();
            });
        }
        else if (button == "channel_right")
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), refreshVisibleBank]
            {
                if (safeThis == nullptr)
                    return;

                safeThis->mixerPanel.setBankOffset(safeThis->mixerPanel.getBankOffset() + 1);
                refreshVisibleBank();
            });
        }
        else if (button == creation::ui::surface_actions::transportCycle)
        {
            transportBar.loopButton.triggerClick();
        }
        else if (button == creation::ui::surface_actions::transportSolo)
        {
            auto selectedChannel = mixerPanel.getSelectedChannel();
            if (selectedChannel < 0)
                selectedChannel = mixerPanel.getBankOffset();

            if (selectedChannel >= 0 && selectedChannel < engine.getTrackCount())
            {
                auto soloed = ! engine.isTrackSoloed(selectedChannel);
                engine.setTrackSoloed(selectedChannel, soloed);
                mixerPanel.setChannelSoloed(selectedChannel, soloed);
                midiSurface.setChannelSoloed(selectedChannel, soloed);
                transportBar.setStatusText("Solo " + juce::String(selectedChannel + 1) + (soloed ? " on" : " off"));
            }
        }
        else if (button == "transport_click")
        {
            transportBar.clickButton.triggerClick();
        }
        else if (button == creation::ui::surface_actions::transportMarker)
        {
            transportBar.setStatusText("Marker pressed.");
        }
        else if (button == creation::ui::surface_actions::transportNudge)
        {
            transportBar.setStatusText("Nudge pressed.");
        }
        else if (button == creation::ui::surface_actions::transportDrop)
        {
            transportBar.setStatusText("Drop pressed.");
        }
        else if (button == creation::ui::surface_actions::transportReplace)
        {
            transportBar.setStatusText("Replace pressed.");
        }
        else if (button == creation::ui::surface_actions::zoom)
        {
            if (pluginRackBar.isTrackContext() && engine.hasTrackPlugin(pluginRackBar.getTrackIndex()))
            {
                auto trackIndex = pluginRackBar.getTrackIndex();
                auto windowKey = "track-rack-" + juce::String(trackIndex);
                if (auto* existingWindow = findPluginEditorWindow(windowKey))
                {
                    existingWindow->toFront(true);
                }
                else if (auto* editor = engine.createTrackPluginEditor(trackIndex))
                {
                    auto window = std::make_unique<ManagedDocumentWindow>("Track Editor",
                                                                          juce::Colour(0xff11151c),
                                                                          juce::DocumentWindow::allButtons,
                                                                          [this, windowKey]
                                                                          {
                                                                              closePluginEditorWindow(windowKey);
                                                                          });
                    window->setUsingNativeTitleBar(true);
                    window->setResizable(true, true);
                    window->setContentOwned(editor, true);
                    window->centreWithSize(900, 650);
                    window->setVisible(true);
                    pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });
                    pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
                }
            }
            else
            {
                setWorkspaceMode(WorkspaceMode::signal);
            }
        }
        else if (button == creation::ui::surface_actions::scrub)
        {
            midiScrubModeEnabled = ! midiScrubModeEnabled;
            transportBar.setScrubModeEnabled(midiScrubModeEnabled);
            transportBar.setStatusText(midiScrubModeEnabled
                ? "X-Touch scrub mode armed. Jog wheel hookup is next."
                : "X-Touch scrub mode off.");
        }
        else if (button == creation::ui::surface_actions::userA)
        {
            saveSessionToDisk();
            transportBar.setStatusText("Session saved from X-Touch");
        }
        else if (button == creation::ui::surface_actions::userB)
        {
            loadSessionFromDisk();
            refreshVisibleBank();
            transportBar.setStatusText("Session reloaded from X-Touch");
        }
    };

    midiSurface.onFaderMoved = [this](int index, float value)
    {
        engine.setTrackGain(index, value);

        // Runs on the MIDI input thread, not the message thread - TimelineModel/TrackerPanel
        // mutation must be deferred, same as the mixerPanel update just below.
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), index, value]
        {
            if (safeThis != nullptr)
                safeThis->recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackVolume, value);
        });

        midiSurface.setChannelGain(index, value);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelGain(index, value);
            });
        }
    };

    midiSurface.onMasterFaderMoved = [this](float value)
    {
        engine.setMasterGain(value);
        mixerPanel.setMasterGain(value);

        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, value]
            {
                if (safeBar != nullptr)
                    safeBar->setStatusText("Master: " + juce::String(value, 2));
            });
        }
    };

    midiSurface.onPanMoved = [this](int index, float value)
    {
        if (index < engine.getTrackCount())
        {
            engine.setTrackPan(index, value);

            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), index, value]
            {
                if (safeThis != nullptr)
                    safeThis->recordAutomationWriteIfArmed(index, cs::AutomationTargetKind::trackPan,
                                                           juce::jmap(value, -1.0f, 1.0f, 0.0f, 1.0f));
            });
        }

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, value]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelPan(index, value);
            });
        }
    };

    midiSurface.onMuteChanged = [this](int index, bool muted)
    {
        if (index < engine.getTrackCount())
            engine.setTrackMuted(index, muted);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, muted]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelMuted(index, muted);
            });
        }
    };

    midiSurface.onSoloChanged = [this](int index, bool soloed)
    {
        if (index < engine.getTrackCount())
            engine.setTrackSoloed(index, soloed);

        auto safePanel = mixerPanelSafe;
        if (safePanel != nullptr)
        {
            juce::MessageManager::callAsync([safePanel, index, soloed]
            {
                if (safePanel != nullptr)
                    safePanel->setChannelSoloed(index, soloed);
            });
        }
    };

    midiSurface.onJogWheelMoved = [this](int delta)
    {
        if (! midiScrubModeEnabled || delta == 0 || engine.isPlaying())
            return;

        auto stepSeconds = 0.05 * (double) std::abs(delta);
        auto signedStep = delta > 0 ? stepSeconds : -stepSeconds;
        auto newSeconds = juce::jmax(0.0, timelineModel.getTransportSeconds() + signedStep);

        timelineModel.setTransportSeconds(newSeconds);
        engine.setPlaybackPositionSeconds(newSeconds);
        transportStartTimelineSeconds = newSeconds;
        transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
        previewScrubAudioAt(newSeconds);
    };

    midiSurface.onTransportCommand = [this](XTouchControlSurface::TransportCommand command)
    {
        auto safeBar = transportBarSafe;

        switch (command)
        {
            case XTouchControlSurface::TransportCommand::play:
                // One hardware button drives both directions: if already playing, this same
                // trigger pauses instead of restarting playback from the top.
                if (engine.isPlaying())
                {
                    engine.stopAssetPreview();
                    engine.setPlaying(false);
                    midiSurface.setTransportState(false, false);
                    if (safeBar != nullptr)
                        juce::MessageManager::callAsync([safeBar]
                        {
                            if (safeBar != nullptr)
                            {
                                safeBar->setStatusText("Transport: pause");
                                safeBar->setPlaybackVisualState(false, false);
                            }
                        });
                    break;
                }

                if (! prepareTrackerPlayback())
                    break;

                transportStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
                transportStartTimelineSeconds = timelineModel.getTransportSeconds();
                engine.setPlaybackPositionSeconds(transportStartTimelineSeconds);
                engine.setPlaying(true);
                midiSurface.setTransportState(true, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: play");
                            safeBar->setPlaybackVisualState(true, false);
                        }
                    });
                break;
            case XTouchControlSurface::TransportCommand::stop:
                stopRecordingSession();
                engine.setPlaying(false);
                midiSurface.setTransportState(false, false);
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: stop");
                            safeBar->setPlaybackVisualState(false, false);
                        }
                    });
                break;
            case XTouchControlSurface::TransportCommand::record:
                if (engine.isRecording() || engine.isMidiRecording())
                {
                    stopRecordingSession();
                }
                else if (startRecordingSession())
                {
                    refreshTrackerPlaybackClips();
                    engine.setPlaying(true);
                    midiSurface.setTransportState(true, true);
                    if (safeBar != nullptr)
                        juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                        {
                            safeBar->setStatusText("Transport: record armed");
                            safeBar->setPlaybackVisualState(true, true);
                        }
                    });
                }
                break;
            case XTouchControlSurface::TransportCommand::rewind:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->rewindButton.triggerClick();
                    });
                break;
            case XTouchControlSurface::TransportCommand::fastForward:
                if (safeBar != nullptr)
                    juce::MessageManager::callAsync([safeBar]
                    {
                        if (safeBar != nullptr)
                            safeBar->fastForwardButton.triggerClick();
                    });
                break;
        }
    };

    midiSurface.onStatusMessage = [this](juce::String text)
    {
        auto safeBar = transportBarSafe;
        if (safeBar != nullptr)
        {
            juce::MessageManager::callAsync([safeBar, text = std::move(text)]
            {
                if (safeBar != nullptr)
                    safeBar->setMidiStatusText(text);
            });
        }
    };

    midiSurface.attachToDeviceManager(deviceManager);
    midiSurface.setTrackCount(engine.getTrackCount());
    midiSurface.setBankOffset(0);

    mixerPanel.setChannelCount(engine.getTrackCount());
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        mixerPanel.setChannelName(index, engine.getTrackName(index));
        trackerPanel.setTrackName(index, engine.getTrackName(index));
    }

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        const auto gain = index == 1 ? 0.18f : index == 2 ? 0.22f : 0.12f;
        const auto pan = index == 1 ? -0.15f : index == 2 ? 0.12f : 0.0f;
        mixerPanel.setChannelGain(index, gain);
        mixerPanel.setChannelPan(index, pan);
        engine.setTrackGain(index, gain);
        engine.setTrackPan(index, pan);
    }

    mixerPanel.setMasterGain(0.5f);
    engine.setMasterGain(0.5f);
    midiSurface.setMasterFaderValue(0.5f);
    engine.setPlaying(false);
    midiSurface.setTransportState(false, false);
    transportBar.setPlaybackVisualState(false, false);

    refreshVisibleBank();
    refreshInsertRack();
    refreshAudioDeviceSettingsView();
    refreshRecentTakes();
    refreshContentLibrary();
    refreshTutorialLibrary();

    configureTutorialOverlay();
    loadLayoutFromDisk();

    metricsCollector.setBearerTokenProvider([this] { return authSession.getSession().token; });

    // Loaded off the message thread and deliberately not awaited here: this
    // may be the very first VFS entry ever created for this install (no
    // feedback-settings.json exists yet on a fresh install), and a
    // first-time-create round trip through the VFS service has been
    // observed to stall for a long time. Startup must never block on it --
    // feedback/metrics are opt-in and off by default, so arriving a moment
    // late (or not at all, if this never completes) changes nothing about
    // whether the app is usable.
    std::thread([safeThis = juce::Component::SafePointer<MainComponent>(this)]
    {
        auto settings = creation_station::FeedbackSettingsStore::load();
        juce::MessageManager::callAsync([safeThis, settings]
        {
            if (safeThis == nullptr)
                return;
            safeThis->feedbackSettings = settings;
            safeThis->metricsCollector.setInstallId(settings.installId);
            safeThis->metricsCollector.setOptedIn(settings.metricsOptIn);
            safeThis->metricsCollector.logEvent("session", "session_start");
        });
    }).detach();

    reportStartup("Djehuti Station is ready.", 1.0f);
    startTimerHz(30);
}

MainComponent::~MainComponent()
{
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
        top->removeKeyListener(this);

    metricsCollector.logEvent("session", "session_end");
    metricsCollector.flush();

    saveLayoutToDisk(true);
    stopTimer();
    pluginEditorWindows.clear();
    midiSurface.detachFromDeviceManager(deviceManager);
    engine.detachFromDevice(deviceManager);
}

void MainComponent::confirmCloseApplication(const std::function<void(bool shouldClose)>& onDecision)
{
    if (! projectDirty)
    {
        if (onDecision)
            onDecision(true);
        return;
    }

    // Was previously a silent auto-save on quit -- now actually asks, same
    // Save/Don't Save/Cancel pattern as removeTrack()'s confirmation below.
    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::QuestionIcon)
        .withTitle("Unsaved Changes")
        .withMessage("You have unsaved changes in this project. Save before quitting?")
        .withButton("Save")
        .withButton("Don't Save")
        .withButton("Cancel");

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(options,
                                 [safeThis, onDecision](int result)
                                 {
                                     if (safeThis == nullptr)
                                     {
                                         if (onDecision)
                                             onDecision(false);
                                         return;
                                     }

                                     if (result == 1) // Save
                                     {
                                         safeThis->saveSessionToDisk(true);
                                         if (onDecision)
                                             onDecision(true);
                                     }
                                     else if (result == 2) // Don't Save
                                     {
                                         if (onDecision)
                                             onDecision(true);
                                     }
                                     else // Cancel / dismissed
                                     {
                                         if (onDecision)
                                             onDecision(false);
                                     }
                                 });
}

void MainComponent::syncActiveModeToFocus()
{
    const auto modeBefore = activeMode;
    struct RefreshMenuIfTargetChanged
    {
        MainComponent& owner;
        WorkspaceMode before;
        ~RefreshMenuIfTargetChanged()
        {
            if ((before == WorkspaceMode::signal) != (owner.activeMode == WorkspaceMode::signal))
                owner.menuItemsChanged();
        }
    } refreshMenu { *this, modeBefore };

    const auto isInside = [](juce::Component& panel, const juce::Component* focused)
    {
        return focused != nullptr && (focused == &panel || panel.isParentOf(focused));
    };

    const auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (isInside(signalLabPanel, focused))
    {
        activeMode = WorkspaceMode::signal;
        return;
    }
    if (isInside(trackerPanel, focused))
    {
        activeMode = WorkspaceMode::tracker;
        return;
    }

    // Focus is somewhere else (a transport button, a menu): keep the last choice while its panel is on screen, and
    // when it is not, use the one of the two that is.
    const auto signalShowing = signalLabPanel.isShowing();
    const auto trackerShowing = trackerPanel.isShowing();
    if (activeMode == WorkspaceMode::signal && ! signalShowing && trackerShowing)
        activeMode = WorkspaceMode::tracker;
    else if (activeMode != WorkspaceMode::signal && signalShowing && ! trackerShowing)
        activeMode = WorkspaceMode::signal;
}

void MainComponent::timerCallback()
{
    syncActiveModeToFocus();

    if (const auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001; nowSeconds - arrangementTitleCheckedAtSeconds > 0.5)
    {
        arrangementTitleCheckedAtSeconds = nowSeconds;
        refreshArrangementTitle();
    }

    // A preview started from the asset list ends by itself; flip its card back from Stop to Play.
    if (previewingProjectAssetId.isNotEmpty() && ! engine.isPreviewingAsset())
    {
        previewingProjectAssetId = {};
        contentPanel.setPreviewingAssetId({});
    }

    refreshTrackInputSources();

    pollHostedPluginStateAutosave();

    {
        juce::Array<WorkstationAudioEngine::LiveMidiControlChange> midiChanges;
        if (engine.takeLiveMidiControlChanges(midiChanges))
        {
            juce::Array<SignalLabPanel::MidiControlChange> forwarded;
            for (auto& change : midiChanges)
                forwarded.add({ change.deviceId, change.channel, change.number, change.isController, change.value });
            signalLabPanel.applyLiveMidiControlChanges(forwarded);
        }
    }

    if (layoutDirty && juce::Time::getMillisecondCounterHiRes() * 0.001 - layoutLastChangeWallSeconds > 0.75)
        saveLayoutToDisk();

    if (activeMode == WorkspaceMode::signal && engine.takeSignalLabLivePlaybackFinishedFlag())
    {
        if (timelineModel.isLoopEnabled() && !transportIsWaitingForLoopDelay)
        {
            auto delaySeconds = timelineModel.getLoopDelaySeconds();
            if (delaySeconds > 0.001)
            {
                transportIsWaitingForLoopDelay = true;
                transportLoopDelayWaitUntilWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001 + delaySeconds;
            }
            else
            {
                signalLabPanel.triggerTransportPlay();
            }
        }
        else
        {
            transportBar.setPlaybackVisualState(false, false);
            engine.stopSignalLabLivePlayback(); // Reset state
        }
    }

    if (activeMode == WorkspaceMode::signal && transportIsWaitingForLoopDelay)
    {
        auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        if (nowSeconds >= transportLoopDelayWaitUntilWallSeconds)
        {
            transportIsWaitingForLoopDelay = false;
            signalLabPanel.triggerTransportPlay();
        }
    }

    if (midiPlaybackRefreshPending && juce::Time::getMillisecondCounterHiRes() * 0.001 - midiPlaybackRefreshLastChangeWallSeconds > 0.35)
    {
        midiPlaybackRefreshPending = false;
        refreshTrackerPlaybackClips();
    }

    if (engine.isPlaying() || engine.isRecording())
    {
        auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
        auto elapsed = juce::jmax(0.0, nowSeconds - transportStartWallSeconds);
        auto timelineSeconds = transportStartTimelineSeconds + elapsed;

        if (transportIsWaitingForLoopDelay)
        {
            if (nowSeconds >= transportLoopDelayWaitUntilWallSeconds)
            {
                transportIsWaitingForLoopDelay = false;
                if (activeMode == WorkspaceMode::signal)
                {
                    signalLabPanel.triggerTransportPlay();
                }
                else
                {
                    transportStartTimelineSeconds = timelineModel.getLoopStartSeconds();
                    transportStartWallSeconds = nowSeconds;
                    timelineSeconds = transportStartTimelineSeconds;
                    engine.setPlaybackPositionSeconds(timelineSeconds);
                    engine.setPlaying(true);
                }
            }
            else
            {
                timelineSeconds = timelineModel.getLoopEndSeconds();
            }
        }
        else if (engine.isPlaying() && ! engine.isRecording() && timelineModel.isLoopEnabled()
            && timelineModel.getLoopEndSeconds() > timelineModel.getLoopStartSeconds()
            && timelineSeconds >= timelineModel.getLoopEndSeconds())
        {
            auto delaySeconds = timelineModel.getLoopDelaySeconds();
            if (delaySeconds > 0.001)
            {
                transportIsWaitingForLoopDelay = true;
                transportLoopDelayWaitUntilWallSeconds = nowSeconds + delaySeconds;
                engine.setPlaying(false);
                timelineSeconds = timelineModel.getLoopEndSeconds();
            }
            else
            {
                timelineSeconds = timelineModel.getLoopStartSeconds();
                transportStartTimelineSeconds = timelineSeconds;
                transportStartWallSeconds = nowSeconds;
                engine.setPlaybackPositionSeconds(timelineSeconds);
            }
        }

        timelineModel.setTransportSeconds(timelineSeconds);

        if (engine.isRecording())
        {
            timelineModel.updateRecordingClip(timelineSeconds);
            for (int index = 0; index < engine.getTrackCount(); ++index)
                if (juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index])
                    timelineModel.addRecordingPeak(index, engine.consumeTrackRecordingPeak(index));
        }

        trackerPanel.centerTransportInView();
        trackerPanel.refreshTimelineView();
    }

    // Runs every tick regardless of transport state -- it must hide itself when the playhead
    // isn't over a video clip (including "no clips at all"), not just while playing, or it's
    // stuck showing its "Decoding..." placeholder forever once shown.
    updateVideoView(timelineModel.getTransportSeconds());

    if (midiEditorPanel != nullptr)
    {
        if (engine.isPlaying() || engine.isRecording())
        {
            auto clipIndex = midiEditorPanel->getEditingClipIndex();
            if (juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
            {
                const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
                auto clipRelativeSeconds = juce::jmax(0.0, timelineModel.getTransportSeconds() - clip.startSeconds);
                midiEditorPanel->setDisplayedTransportSeconds(clipRelativeSeconds, true);
            }
            midiEditorPanel->setPlaybackState(true, engine.isRecording() || engine.isMidiRecording());
        }
        else if (midiEditorPreviewPlaying)
        {
            auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
            auto localSeconds = midiEditorPreviewStartLocalSeconds + juce::jmax(0.0, nowSeconds - midiEditorPreviewStartWallSeconds);
            if (localSeconds >= midiEditorPreviewEndLocalSeconds)
            {
                if (midiEditorPreviewLoopEnabled && midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds)
                {
                    midiEditorPreviewStartLocalSeconds = midiEditorPreviewLoopStartSeconds;
                    midiEditorPreviewStartWallSeconds = nowSeconds;
                    updateMidiEditorPreviewNotes(midiEditorPreviewLoopStartSeconds, true);
                    localSeconds = midiEditorPreviewLoopStartSeconds;
                }
                else
                {
                    stopMidiEditorPreview();
                }
            }

            if (midiEditorPreviewPlaying)
            {
                updateMidiEditorPreviewNotes(localSeconds, false);
                midiEditorPanel->setDisplayedTransportSeconds(localSeconds, false);
                midiEditorPanel->setPlaybackState(true, false);
            }
        }
        else
        {
            midiEditorPanel->setPlaybackState(false, false);
        }
    }

    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));

        // Keeps the header gain readout honest while an automation lane is driving this track's
        // volume - otherwise the slider only ever shows the last value the user dragged it to,
        // even while the engine is actually applying a different, automation-driven gain.
        trackerPanel.setTrackGain(index, engine.getTrackGain(index));
    }

    updateAutomationRecordModes();
}

void MainComponent::pollHostedPluginStateAutosave()
{
    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (nowSeconds - pluginStateLastPollWallSeconds >= 0.5)
    {
        pluginStateLastPollWallSeconds = nowSeconds;
        auto currentSignature = engine.createHostedPluginStateSignature();

        if (lastObservedPluginStateSignature.isEmpty())
        {
            lastObservedPluginStateSignature = currentSignature;
        }
        else if (currentSignature != lastObservedPluginStateSignature)
        {
            lastObservedPluginStateSignature = currentSignature;
            pluginStateAutosavePending = true;
            pluginStateLastChangeWallSeconds = nowSeconds;
            projectDirty = true;
        }
    }

    if (! pluginStateAutosavePending || nowSeconds - pluginStateLastChangeWallSeconds <= 0.9)
        return;

    pluginStateAutosavePending = false;

    if (! projectSession.isValid() || ! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    projectSession.writeEntry("Project/state.xml", juce::MemoryBlock());
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
    g.setColour(juce::Colour(0xff1c2230));
    auto area = getLocalBounds().toFloat().reduced(18.0f);
    g.fillRoundedRectangle(area, 24.0f);
    g.setColour(juce::Colour(0xff2a3244));
    g.drawRoundedRectangle(area, 24.0f, 1.0f);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto transportArea = area.removeFromTop(CreationSuiteHeaderBar::preferredHeight);
    transportBar.setBounds(transportArea);

    auto menuArea = area.removeFromTop(28);
    if (menuBar != nullptr)
        menuBar->setBounds(menuArea);

    if (dockManager != nullptr)
        dockManager->setBounds(area);
    poppedWorkspacePlaceholder.setBounds(area.reduced(24));
    authGateView.setBounds(getLocalBounds());
    tourOverlay.setBounds(getLocalBounds());
    markLayoutDirty();
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "View", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex(int topLevelMenuIndex, const juce::String&)
{
    juce::PopupMenu menu;

    // Projects belong to the suite (the header creates, opens and switches them); this menu only holds what
    // this app does with the project it is a tenant in.
    if (topLevelMenuIndex == 0)
    {
        // File follows whichever of the Tracker or the Signal Lab has focus.
        menu.addItem(menuIdFileImport, "Import...");
        menu.addSeparator();

        if (activeMode == WorkspaceMode::signal)
        {
            menu.addItem(menuIdFileNewSignal, "New Signal");
            menu.addItem(menuIdFileOpenSignal, "Open Signal...");
            menu.addItem(menuIdFileSaveSignal, "Save Signal...");
            menu.addSeparator();
            menu.addItem(menuIdFileRenderSignal, "Render Signal to Project");
            menu.addSeparator();
            menu.addItem(menuIdFileSave, "Save Project", projectSession.isValid());
            return menu;
        }

        menu.addItem(menuIdFileSave, "Save", projectSession.isValid());
        menu.addSeparator();
        menu.addItem(menuIdFileNewArrangement, "New Arrangement");
        menu.addItem(menuIdFileSaveArrangement, "Save Arrangement...", projectSession.isValid());
        menu.addItem(menuIdFileLoadArrangement, "Load Arrangement...", projectSession.isValid());
        menu.addSeparator();
        menu.addItem(menuIdFileRender, "Render Full Mix...");
        menu.addItem(menuIdFileExportWav, "Export Full Mix as WAV...");
        return menu;
    }

    if (topLevelMenuIndex == 1)
    {
        const auto hasClip = selectedClipIndex >= 0;
        const auto addWithKey = [&menu](int id, const juce::String& label, const juce::String& shortcut, bool enabled)
        {
            juce::PopupMenu::Item item(label);
            item.itemID = id;
            item.isEnabled = enabled;
            item.shortcutKeyDescription = shortcut;
            menu.addItem(item);
        };

        addWithKey(menuIdEditUndo, "Undo", "Ctrl+Z", true);
        addWithKey(menuIdEditRedo, "Redo", "Ctrl+Y", true);
        menu.addSeparator();
        addWithKey(menuIdEditDuplicate, "Duplicate Clip", "Ctrl+D", hasClip);
        addWithKey(menuIdEditSplit, "Split at Playhead", "Ctrl+Shift+S", hasClip);
        addWithKey(menuIdEditRename, "Rename Clip", "F2", hasClip);
        addWithKey(menuIdEditDelete, "Delete Clip", "Delete", hasClip);
        return menu;
    }

    if (topLevelMenuIndex == 2)
    {
        const auto isOpen = [this](const juce::String& id)
        {
            return dockManager != nullptr && dockManager->isRegistered(id);
        };

        menu.addItem(menuIdToolTracker, "Tracker", true, isOpen(trackerPanelId));
        menu.addItem(menuIdToolSampler, "Sampler", true, isOpen(samplerPanelId));
        menu.addItem(menuIdToolSignal, "Signal", true, isOpen(signalPanelId));
        menu.addItem(menuIdToolLayers, "Layers", true, isOpen(layersPanelId));
        menu.addItem(menuIdToolPlugins, "Plugins", true, isOpen(pluginsPanelId));
        menu.addItem(menuIdToolScript, "Script", true, isOpen(scriptPanelId));
        menu.addItem(menuIdToolCapture, "Capture", true, isOpen(capturePanelId));
        menu.addItem(menuIdToolScore, "Score", true, isOpen(scorePanelId));
        menu.addItem(menuIdToolSettings, "Settings", true, isOpen(settingsPanelId));
        menu.addItem(menuIdToolFoley, "Foley", true, isOpen(foleyPanelId));
        menu.addItem(menuIdToolVideo, "Video", true, isOpen(videoPanelId));
        menu.addSeparator();
        menu.addItem(menuIdToolTrackInsert, "Track Insert", true, isOpen(trackInsertPanelId));
        menu.addItem(menuIdToolVirtualEngineer, "Virtual Engineer", true, isOpen(virtualEngineerPanelId));
        menu.addSeparator();
        menu.addItem(menuIdToolResetLayout, "Reset Dock Layout");
        return menu;
    }

    menu.addItem(menuIdHelpContents, "Help Topics	F1");
    menu.addItem(menuIdHelpTour, "Guided Tour");
    menu.addSeparator();
    menu.addItem(menuIdHelpFeedback, "Send Feedback...");
    return menu;
}

void MainComponent::menuItemSelected(int menuItemID, int topLevelMenuIndex)
{
    if (menuItemID == 0)
        return;

    if (topLevelMenuIndex == 0)
    {
        switch (menuItemID)
        {
            case menuIdFileSave: saveProject(); break;
            case menuIdFileNewArrangement: newArrangement(); break;
            case menuIdFileImport: showImportWindow(); break;
            case menuIdFileSaveArrangement: trackerPanel.promptSaveArrangement(); break;
            case menuIdFileLoadArrangement: trackerPanel.requestLoadArrangement(); break;
            case menuIdFileRender: showRenderDialog(RenderRequest::Destination::project); break;
            case menuIdFileExportWav: showRenderDialog(RenderRequest::Destination::file); break;
            case menuIdFileNewSignal: signalLabPanel.newSignal(); break;
            case menuIdFileOpenSignal: signalLabPanel.openSignal(); break;
            case menuIdFileSaveSignal: signalLabPanel.saveSignal(); break;
            case menuIdFileRenderSignal: signalLabPanel.renderSignalToProject(); break;
            default: break;
        }
        return;
    }

    if (topLevelMenuIndex == 1)
    {
        const auto clipCount = (int) timelineModel.getClips().size();
        const auto clipSelected = juce::isPositiveAndBelow(selectedClipIndex, clipCount);
        switch (menuItemID)
        {
            case menuIdEditUndo:
                if (activeMode == WorkspaceMode::signal) undoSignalEdit(); else undoTimelineEdit();
                break;
            case menuIdEditRedo:
                if (activeMode == WorkspaceMode::signal) redoSignalEdit(); else redoTimelineEdit();
                break;
            case menuIdEditDuplicate: if (clipSelected) duplicateClip(selectedClipIndex); break;
            case menuIdEditSplit: if (clipSelected) splitClipAt(selectedClipIndex, timelineModel.getTransportSeconds()); break;
            case menuIdEditRename: if (clipSelected) renameClip(selectedClipIndex); break;
            case menuIdEditDelete: if (clipSelected) deleteClip(selectedClipIndex); break;
            default: break;
        }
        return;
    }

    if (topLevelMenuIndex == 2)
    {
        switch (menuItemID)
        {
            case menuIdToolTracker: toggleToolWindow(WorkspaceMode::tracker); break;
            case menuIdToolSampler: toggleToolWindow(WorkspaceMode::sampler); break;
            case menuIdToolSignal: toggleToolWindow(WorkspaceMode::signal); break;
            case menuIdToolLayers: toggleToolWindow(WorkspaceMode::mix); break;
            case menuIdToolPlugins: toggleToolWindow(WorkspaceMode::plugins); break;
            case menuIdToolScript: toggleToolWindow(WorkspaceMode::code); break;
            case menuIdToolCapture: toggleToolWindow(WorkspaceMode::record); break;
            case menuIdToolScore: toggleToolWindow(WorkspaceMode::score); break;
            case menuIdToolSettings: toggleToolWindow(WorkspaceMode::settings); break;
            case menuIdToolFoley: toggleToolWindow(WorkspaceMode::foley); break;
            case menuIdToolVideo: toggleDockPanel(videoPanelId, CreationDock::DockTargetZone::Right); break;
            case menuIdToolVirtualEngineer: toggleAiToolWindow(); break;
            case menuIdToolTrackInsert: toggleDockPanel(trackInsertPanelId, CreationDock::DockTargetZone::Bottom); break;
            case menuIdToolResetLayout: resetDockLayout(); break;
            default: break;
        }

        menuItemsChanged();
        return;
    }

    if (menuItemID == menuIdHelpContents)
        showHelpWindow();
    else if (menuItemID == menuIdHelpTour)
        showTour();
    else if (menuItemID == menuIdHelpFeedback)
        showFeedbackWindow();
}

CreationDock::DockPanel* MainComponent::registerNamedDockPanel(const juce::String& panelId, CreationDock::DockTargetZone zone)
{
    if (dockManager == nullptr)
        return nullptr;

    if (panelId == trackInsertPanelId)
        return dockManager->registerPanel(panelId, "Track Insert", std::make_unique<NonOwningPanelHost>(pluginRackBar), zone);
    if (panelId == trackerPanelId)
        return dockManager->registerPanel(panelId, "Tracker", std::make_unique<NonOwningPanelHost>(trackerPanel), zone);
    if (panelId == samplerPanelId)
        return dockManager->registerPanel(panelId, "Sampler", std::make_unique<NonOwningPanelHost>(samplePackBuilderPanel), zone);
    if (panelId == signalPanelId)
        return dockManager->registerPanel(panelId, "Signal", std::make_unique<NonOwningPanelHost>(signalLabPanel), zone);
    if (panelId == layersPanelId)
        return dockManager->registerPanel(panelId, "Layers", std::make_unique<NonOwningPanelHost>(mixerPanel), zone);
    if (panelId == pluginsPanelId)
        return dockManager->registerPanel(panelId, "Plugins", std::make_unique<NonOwningPanelHost>(pluginsPanel), zone);
    if (panelId == scriptPanelId)
        return dockManager->registerPanel(panelId, "Script", std::make_unique<NonOwningPanelHost>(dslPanel), zone);
    if (panelId == capturePanelId)
        return dockManager->registerPanel(panelId, "Capture", std::make_unique<NonOwningPanelHost>(recordView), zone);
    if (panelId == scorePanelId)
        return dockManager->registerPanel(panelId, "Score", std::make_unique<NonOwningPanelHost>(scorePanel), zone);
    if (panelId == settingsPanelId)
        return dockManager->registerPanel(panelId, "Settings", std::make_unique<NonOwningPanelHost>(settingsPanel), zone);
    if (panelId == foleyPanelId)
        return dockManager->registerPanel(panelId, "Foley", std::make_unique<NonOwningPanelHost>(foleyPanel), zone);
    if (panelId == videoPanelId)
        return dockManager->registerPanel(panelId, "Video", std::make_unique<NonOwningPanelHost>(videoPanelHost), zone);
    if (panelId == virtualEngineerPanelId)
        return dockManager->registerPanel(panelId, "Virtual Engineer", std::make_unique<NonOwningPanelHost>(aiPanel), zone);

    // Not a panel this app has (for instance an id from a layout saved by a version that had one that is gone).
    return nullptr;
}

void MainComponent::initialiseDockingWorkspace()
{
    if (dockManager == nullptr)
        return;

    // Only Tracker starts open; every other tool panel is registered on demand the first
    // time it's shown (View menu / toggleDockPanel) and fully unregistered when closed --
    // the shared DockManager has no separate "registered but hidden" state, see
    // registerNamedDockPanel().
    //
    // Real bug fixed here: this used to call registerNamedDockPanel()
    // unconditionally, unlike every other call site in this file (all of
    // which check isRegistered() first). An autoloaded project's
    // loadSessionFromDisk() calls setWorkspaceMode(tracker) earlier in this
    // same constructor, which already registers Tracker via the properly-
    // guarded activateDockPanel() -- this function then ran anyway and
    // registered a second, independent "tracker" DockPanel.
    // registerNamedDockPanel() has no internal dedupe, so both got created;
    // since both wrapped a fresh NonOwningPanelHost around the SAME
    // trackerPanel member, the second one's addAndMakeVisible() reparented
    // trackerPanel away from the first, leaving one tab a real "Tracker"
    // and the other a permanently blank duplicate. Confirmed via a real
    // dock-registration trace, not just code reading.
    if (! dockManager->isRegistered(trackerPanelId))
        registerNamedDockPanel(trackerPanelId, CreationDock::DockTargetZone::CenterTab);
    dockManager->activatePanel(trackerPanelId);
}

void MainComponent::setWorkspaceMode(WorkspaceMode mode)
{
    if (mode == WorkspaceMode::library)
        mode = WorkspaceMode::tracker;

    if (activeMode == WorkspaceMode::settings && mode != WorkspaceMode::settings)
        saveAppSettings();

    if (mode == WorkspaceMode::settings)
        refreshMidiDeviceSettings();

    if (mode != activeMode)
        metricsCollector.logFeatureUsage("workspace_opened:" + workspaceModeName(mode));

    const auto signalTargetBefore = activeMode == WorkspaceMode::signal;
    activeMode = mode;
    if (signalTargetBefore != (activeMode == WorkspaceMode::signal))
        menuItemsChanged();
    activateDockPanel(mode == WorkspaceMode::tracker ? trackerPanelId
                     : mode == WorkspaceMode::sampler ? samplerPanelId
                     : mode == WorkspaceMode::signal ? signalPanelId
                     : mode == WorkspaceMode::mix ? layersPanelId
                     : mode == WorkspaceMode::plugins ? pluginsPanelId
                     : mode == WorkspaceMode::code ? scriptPanelId
                     : mode == WorkspaceMode::record ? capturePanelId
                     : mode == WorkspaceMode::score ? scorePanelId
                     : mode == WorkspaceMode::settings ? settingsPanelId
                     : mode == WorkspaceMode::foley ? foleyPanelId
                     : trackerPanelId,
                     mode == WorkspaceMode::mix ? CreationDock::DockTargetZone::Bottom
                     : mode == WorkspaceMode::plugins || mode == WorkspaceMode::sampler
                       || mode == WorkspaceMode::record || mode == WorkspaceMode::foley ? CreationDock::DockTargetZone::Left
                     : mode == WorkspaceMode::settings || mode == WorkspaceMode::code ? CreationDock::DockTargetZone::Right
                     : CreationDock::DockTargetZone::CenterTab);

    markLayoutDirty();
}

void MainComponent::resetDockLayout()
{
    if (dockManager != nullptr)
        dockManager->resetLayout();

    markLayoutDirty();
    menuItemsChanged();
}

void MainComponent::toggleToolWindow(WorkspaceMode mode)
{
    if (mode == WorkspaceMode::library)
        return;

    const auto panelId = mode == WorkspaceMode::tracker ? trackerPanelId
                        : mode == WorkspaceMode::sampler ? samplerPanelId
                        : mode == WorkspaceMode::signal ? signalPanelId
                        : mode == WorkspaceMode::mix ? layersPanelId
                        : mode == WorkspaceMode::plugins ? pluginsPanelId
                        : mode == WorkspaceMode::code ? scriptPanelId
                        : mode == WorkspaceMode::record ? capturePanelId
                        : mode == WorkspaceMode::score ? scorePanelId
                        : mode == WorkspaceMode::settings ? settingsPanelId
                        : mode == WorkspaceMode::foley ? foleyPanelId
                        : trackerPanelId;
    const auto fallbackZone = mode == WorkspaceMode::mix ? CreationDock::DockTargetZone::Bottom
                              : mode == WorkspaceMode::plugins || mode == WorkspaceMode::sampler
                                || mode == WorkspaceMode::record || mode == WorkspaceMode::foley ? CreationDock::DockTargetZone::Left
                              : mode == WorkspaceMode::settings || mode == WorkspaceMode::code ? CreationDock::DockTargetZone::Right
                              : CreationDock::DockTargetZone::CenterTab;

    toggleDockPanel(panelId, fallbackZone);

    if (dockManager != nullptr && dockManager->isRegistered(panelId))
        activeMode = mode;

    markLayoutDirty();
    menuItemsChanged();
}

void MainComponent::toggleAiToolWindow()
{
    toggleDockPanel(virtualEngineerPanelId, CreationDock::DockTargetZone::Right);
    markLayoutDirty();
    menuItemsChanged();
}

void MainComponent::toggleDockPanel(const juce::String& panelId, CreationDock::DockTargetZone fallbackZone)
{
    if (dockManager == nullptr)
        return;

    if (dockManager->isRegistered(panelId))
        dockManager->unregisterPanel(panelId);
    else
        registerNamedDockPanel(panelId, fallbackZone);
}

void MainComponent::activateDockPanel(const juce::String& panelId, CreationDock::DockTargetZone fallbackZone)
{
    if (dockManager == nullptr)
        return;

    if (! dockManager->isRegistered(panelId))
        registerNamedDockPanel(panelId, fallbackZone);
    else
        dockManager->activatePanel(panelId);

    menuItemsChanged();
}

void MainComponent::refreshModeVisibility()
{
    transportBar.setVisible(true);
    if (menuBar != nullptr)
        menuBar->setVisible(true);
    if (dockManager != nullptr)
        dockManager->setVisible(true);
    poppedWorkspacePlaceholder.setVisible(false);
    authGateView.setVisible(false);
    if (tourOverlay.isActive())
        tourOverlay.toFront(true);
}

juce::ValueTree MainComponent::createLayoutState() const
{
    juce::ValueTree layout("Layout");
    layout.setProperty("format", "creation-station-layout", nullptr);
    layout.setProperty("formatVersion", 1, nullptr);
    layout.setProperty("activeMode", static_cast<int>(activeMode), nullptr);
    layout.setProperty("aiSidebarCollapsed", aiSidebarCollapsed, nullptr);
    layout.setProperty("aiEnterSends", aiPanel.getEnterSendsMessage(), nullptr);
    layout.setProperty("aiRole", aiPanel.getRole() == AiPanel::Role::producer ? "producer" : "engineer", nullptr);
    if (dockManager != nullptr)
        layout.setProperty("dockLayoutJson", juce::JSON::toString(dockManager->captureLayout()), nullptr);

    if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
    {
        auto bounds = window->getBounds();
        layout.setProperty("mainWindowX", bounds.getX(), nullptr);
        layout.setProperty("mainWindowY", bounds.getY(), nullptr);
        layout.setProperty("mainWindowW", bounds.getWidth(), nullptr);
        layout.setProperty("mainWindowH", bounds.getHeight(), nullptr);
    }

    return layout;
}

void MainComponent::restoreLayoutState(const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    auto savedActiveMode = static_cast<WorkspaceMode>(juce::jlimit(0,
                                                                    static_cast<int>(WorkspaceMode::foley),
                                                                    (int) state.getProperty("activeMode", static_cast<int>(WorkspaceMode::tracker))));
    if (savedActiveMode == WorkspaceMode::library)
        savedActiveMode = WorkspaceMode::tracker;
    activeMode = savedActiveMode;

    aiSidebarCollapsed = (bool) state.getProperty("aiSidebarCollapsed", false);
    aiPanel.setCollapsed(aiSidebarCollapsed);
    aiPanel.setEnterSendsMessage((bool) state.getProperty("aiEnterSends", false));
    aiPanel.setRole(state.getProperty("aiRole", "engineer").toString() == "producer" ? AiPanel::Role::producer : AiPanel::Role::engineer);

    auto dockLayoutJson = state.getProperty("dockLayoutJson").toString();
    if (dockManager != nullptr && dockLayoutJson.isNotEmpty())
    {
        const auto layout = juce::JSON::parse(dockLayoutJson);

        // The dock manager only re-docks panels that already exist, and only the Tracker does at startup -
        // so a panel that was open when the app closed (Signal Lab, Plugins, Video, ...) was silently skipped.
        // Create every panel the saved layout names first, in the zone it was saved in; applyLayout then puts
        // them back in their saved order, sizes and floating positions.
        const std::pair<const char*, CreationDock::DockTargetZone> zoneKeys[] = {
            { "left", CreationDock::DockTargetZone::Left },
            { "center", CreationDock::DockTargetZone::CenterTab },
            { "right", CreationDock::DockTargetZone::Right },
            { "bottom", CreationDock::DockTargetZone::Bottom } };

        for (const auto& [key, zone] : zoneKeys)
            if (auto* ids = layout.getProperty("zones", {}).getProperty(key, {}).getProperty("panels", {}).getArray())
                for (const auto& id : *ids)
                    if (! dockManager->isRegistered(id.toString()))
                        registerNamedDockPanel(id.toString(), zone);

        if (auto* floating = layout.getProperty("floating", {}).getArray())
            for (const auto& entry : *floating)
                if (const auto id = entry.getProperty("id", {}).toString(); id.isNotEmpty() && ! dockManager->isRegistered(id))
                    registerNamedDockPanel(id, CreationDock::DockTargetZone::CenterTab);

        dockManager->applyLayout(layout);
    }

    refreshModeVisibility();

    auto mainX = (int) state.getProperty("mainWindowX", -1);
    auto mainY = (int) state.getProperty("mainWindowY", -1);
    auto mainW = (int) state.getProperty("mainWindowW", -1);
    auto mainH = (int) state.getProperty("mainWindowH", -1);
    if (mainX >= 0 && mainY >= 0 && mainW > 0 && mainH > 0)
    {
        if (auto* window = findParentComponentOfClass<juce::DocumentWindow>())
            window->setBounds(mainX, mainY, mainW, mainH);
    }

    setWorkspaceMode(savedActiveMode);
    layoutDirty = false;
}

void MainComponent::markLayoutDirty()
{
    layoutDirty = true;
    layoutLastChangeWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void MainComponent::saveLayoutToDisk(bool userInitiated)
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    if (! userInitiated && ! layoutDirty)
        return;

    auto state = createLayoutState();
    auto layoutXml = state.createXml();
    if (layoutXml == nullptr)
        return;

    // Previously wrapped in a ZIP package with its own manifest.json, purely so the save could be
    // done atomically via a temp-file-then-rename dance -- the VFS service's own writeEntry()
    // already commits atomically server-side (see services/VfsService/Source/Main.cpp's PUT
    // /suite/entry handler), so that wrapper was solving a problem that no longer exists here.
    auto* root = new juce::DynamicObject();
    root->setProperty("layoutXml", layoutXml->toString());

    juce::String errorMessage;
    if (creation::services::SuiteVfsJsonStore::saveJson("station-layout.json", juce::var(root), errorMessage))
        layoutDirty = false;
}

void MainComponent::loadLayoutFromDisk()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    juce::String errorMessage;
    auto parsed = creation::services::SuiteVfsJsonStore::loadJson("station-layout.json", errorMessage);
    auto* state = parsed.getDynamicObject();
    if (state == nullptr)
        return;

    auto layoutXmlText = state->getProperty("layoutXml").toString();
    if (layoutXmlText.isEmpty())
        return;

    auto xml = juce::parseXML(layoutXmlText);
    if (xml == nullptr)
        return;

    restoreLayoutState(juce::ValueTree::fromXml(*xml));
}

juce::Component* MainComponent::getWorkspaceComponent(WorkspaceMode mode)
{
    switch (mode)
    {
        case WorkspaceMode::tracker: return &trackerPanel;
        case WorkspaceMode::sampler: return &samplePackBuilderPanel;
        case WorkspaceMode::signal: return &signalLabPanel;
        case WorkspaceMode::library: return nullptr;
        case WorkspaceMode::mix: return &mixerPanel;
        case WorkspaceMode::plugins: return &pluginsPanel;
        case WorkspaceMode::code: return &dslPanel;
        case WorkspaceMode::record: return &recordView;
        case WorkspaceMode::foley: return &foleyPanel;
        case WorkspaceMode::score: return &scorePanel;
        case WorkspaceMode::settings: return &settingsPanel;
    }

    return nullptr;
}

void MainComponent::refreshAuthState()
{
    refreshModeVisibility();
    contentPanel.setAuthState(authenticated, authenticated && isAdminRole(authSession.getSession().user.role));

    if (authenticated)
    {
        const auto& session = authSession.getSession();
        transportBar.setProfile(makeHeaderProfile(session));
        transportBar.setStatusText("Signed in. Welcome back.");
        syncSemanticAppContext();
    }
    else
    {
        transportBar.clearProfile();
        transportBar.setStatusText("Ready. Sign in from the top-right when you want sync.");
        appContextSyncInProgress = false;
    }
}

void MainComponent::openLagDaemonProfile()
{
    juce::URL("https://lagdaemon.com/profile").launchInDefaultBrowser();
}

void MainComponent::showAudioSettings()
{
    if (audioDeviceWindow != nullptr)
    {
        audioDeviceWindow->toFront(true);
        return;
    }

    auto selector = std::make_unique<juce::AudioDeviceSelectorComponent>(deviceManager,
                                                                         0, 2,
                                                                         0, 2,
                                                                         true, true, true, false);

    auto window = std::make_unique<ManagedDocumentWindow>("Audio Devices",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              audioDeviceWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(false);
    window->setResizable(true, true);
    window->setContentOwned(selector.release(), true);
    window->centreWithSize(720, 540);
    window->setVisible(true);
    audioDeviceWindow = std::move(window);
}

void MainComponent::showFxStackWindow()
{
    if (! pluginRackBar.isTrackContext())
    {
        transportBar.setStatusText("Select a track to edit its FX stack.");
        return;
    }

    if (fxStackWindow != nullptr)
    {
        fxStackWindow->toFront(true);
        refreshFxStackWindow();
        return;
    }

    auto panel = std::make_unique<FxStackPanel>();
    fxStackPanel = panel.get();

    panel->onAddPlugin = [this](const VstPluginCatalog::Entry& entry)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();
        juce::String errorMessage;
        if (! engine.loadTrackPlugin(trackIndex, entry.file, errorMessage))
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not add plugin.");

        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onInsertPlugin = [this](int slotIndex, const VstPluginCatalog::Entry& entry)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();
        juce::String errorMessage;
        if (! engine.insertTrackPlugin(trackIndex, slotIndex, entry.file, errorMessage))
            transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not insert plugin.");

        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onRemovePlugin = [this](int slotIndex)
    {
        const auto trackIndex = pluginRackBar.getTrackIndex();

        // Close any open editors for this track before unloading - otherwise their editor
        // components would outlive the processors they belong to.
        closePluginEditorWindowsForTrack(trackIndex);

        engine.unloadTrackPlugin(trackIndex, slotIndex);
        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onMovePlugin = [this](int fromSlot, int toSlot)
    {
        if (engine.moveTrackPlugin(pluginRackBar.getTrackIndex(), fromSlot, toSlot))
        {
            refreshInsertRack();
            syncTrackViews();
            projectDirty = true;
        }
    };

    panel->onBypassChanged = [this](int slotIndex, bool shouldBypass)
    {
        engine.setTrackPluginBypassed(pluginRackBar.getTrackIndex(), slotIndex, shouldBypass);
        refreshInsertRack();
        syncTrackViews();
        projectDirty = true;
    };

    panel->onOpenPluginEditor = [this](int slotIndex)
    {
        openTrackPluginEditor(pluginRackBar.getTrackIndex(), slotIndex);
    };

    panel->onRescanRequested = [this]
    {
        rescanVstCatalog();
    };

    panel->setCatalog(vstPluginCatalog.getEntries());

    auto window = std::make_unique<ManagedDocumentWindow>("Djehuti Station - Track FX Stack",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              fxStackPanel = nullptr;
                                                              fxStackWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(760, 420, 1600, 1000);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(1000, 560);
    window->setVisible(true);
    fxStackWindow = std::move(window);
    refreshFxStackWindow();
}

void MainComponent::showFeedbackWindow()
{
    if (feedbackWindow != nullptr)
    {
        feedbackWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<FeedbackDialog>();
    auto* panelRaw = panel.get();
    panel->setOptIns(feedbackSettings.feedbackOptIn, feedbackSettings.metricsOptIn);

    panel->onOptInsChanged = [this](bool feedbackOptIn, bool metricsOptIn)
    {
        feedbackSettings.feedbackOptIn = feedbackOptIn;
        feedbackSettings.metricsOptIn = metricsOptIn;
        metricsCollector.setOptedIn(metricsOptIn);

        // Saved off the message thread -- see the startup load's comment on
        // why a VFS write is never done synchronously here. The toggle's
        // on-screen state already flipped instantly; this just persists it.
        auto settingsToSave = feedbackSettings;
        std::thread([settingsToSave]
        {
            juce::String saveError;
            creation_station::FeedbackSettingsStore::save(settingsToSave, saveError);
        }).detach();
    };

    panel->onSubmitRequested = [this](const juce::String& message, const juce::String& category)
    {
        if (feedbackDialogPanel != nullptr)
            feedbackDialogPanel->setSubmitInProgress(true);

        auto installId = feedbackSettings.installId;
        auto bearerToken = authSession.getSession().token;

        // Captures the client by value, not `this` -- stays safe even if the
        // feedback window (and this component) is closed/destroyed before
        // the background send finishes.
        std::thread([client = feedbackMetricsClient, installId, message, category, bearerToken,
                    safeThis = juce::Component::SafePointer<MainComponent>(this)]
        {
            juce::String errorMessage;
            auto success = client.submitFeedback(installId, message, category, bearerToken, errorMessage);
            juce::MessageManager::callAsync([safeThis, success, errorMessage]
            {
                if (safeThis == nullptr || safeThis->feedbackDialogPanel == nullptr)
                    return;
                safeThis->feedbackDialogPanel->setSubmitInProgress(false);
                safeThis->feedbackDialogPanel->setSubmitResult(success,
                    success ? "Thanks -- feedback sent." : errorMessage);
            });
        }).detach();
    };

    auto window = std::make_unique<ManagedDocumentWindow>("Djehuti Station - Feedback",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              feedbackDialogPanel = nullptr;
                                                              feedbackWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(520, 480, 900, 800);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(600, 560);
    window->setVisible(true);
    feedbackWindow = std::move(window);
    feedbackDialogPanel = panelRaw;
}

juce::String MainComponent::currentHelpId() const
{
    const auto isInside = [](const juce::Component& panel, const juce::Component* focused)
    {
        return focused != nullptr && (focused == &panel || panel.isParentOf(focused));
    };
    const auto* focused = juce::Component::getCurrentlyFocusedComponent();

    if (isInside(signalLabPanel, focused)) return "djehuti.station.view.signal-lab";
    if (isInside(trackerPanel, focused)) return "djehuti.station.view.tracker";
    if (isInside(samplePackBuilderPanel, focused)) return "djehuti.station.view.sampler";
    if (isInside(mixerPanel, focused)) return "djehuti.station.view.layers";
    if (isInside(pluginsPanel, focused)) return "djehuti.station.view.plugins";
    if (isInside(dslPanel, focused)) return "djehuti.station.view.script";
    if (isInside(recordView, focused)) return "djehuti.station.view.capture";
    if (isInside(foleyPanel, focused)) return "djehuti.station.view.foley";
    if (isInside(scorePanel, focused)) return "djehuti.station.view.score";
    if (isInside(aiPanel, focused)) return "djehuti.station.view.virtual-engineer";
    if (isInside(settingsPanel, focused)) return "djehuti.station.view.settings";

    switch (activeMode)
    {
        case WorkspaceMode::signal: return "djehuti.station.view.signal-lab";
        case WorkspaceMode::tracker: return "djehuti.station.view.tracker";
        default: break;
    }
    return {};
}

void MainComponent::showHelpWindow(const juce::String& helpIdIn)
{
    if (helpLibrary.topics().empty())
    {
        transportBar.setStatusText("Help is not available in this build.");
        return;
    }

    const auto helpId = helpIdIn.isNotEmpty() ? helpIdIn : currentHelpId();

    if (helpWindow != nullptr && helpPanel != nullptr)
    {
        if (helpId.isNotEmpty())
            helpPanel->showHelpId(helpId);
        helpWindow->setVisible(true);
        helpWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<cs::help::HelpPanel>(helpLibrary);
    auto* panelRaw = panel.get();
    if (helpId.isNotEmpty())
        panel->showHelpId(helpId);

    auto window = std::make_unique<ManagedDocumentWindow>("Djehuti Station - Help",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              helpPanel = nullptr;
                                                              helpWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(640, 420, 1600, 1200);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(980, 640);
    window->setVisible(true);
    helpWindow = std::move(window);
    helpPanel = panelRaw;
}

void MainComponent::showMidiEditorWindow(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return;

    if (midiEditorWindow != nullptr)
    {
        if (midiEditorPanel != nullptr)
            midiEditorPanel->setClip(&timelineModel, clipIndex);
        midiEditorWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<MidiEditorPanel>();
    midiEditorPanel = panel.get();

    panel->onNotesChanged = [this]
    {
        projectDirty = true;
        saveSessionToDisk();

        // Re-rendering a MIDI clip through its instrument plugin is too slow to do on every
        // single drag step (each call loads a fresh plugin instance) - coalesce rapid edits
        // and refresh once the user pauses, via the existing 30Hz UI timer below.
        midiPlaybackRefreshPending = true;
        midiPlaybackRefreshLastChangeWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    };

    panel->onCloseRequested = [this]
    {
        midiEditorPanel = nullptr;
        midiEditorWindow.reset();
    };

    panel->onPlayRequested = [this]
    {
        startMidiEditorPreview();
    };

    panel->onStopRequested = [this]
    {
        stopMidiEditorPreview();
    };

    panel->onLoopEnabledChanged = [this](bool enabled)
    {
        midiEditorPreviewLoopEnabled = enabled;
    };

    panel->onTransportChanged = [this](double seconds)
    {
        midiEditorPreviewStartLocalSeconds = juce::jmax(0.0, seconds);
        if (midiEditorPanel != nullptr)
            midiEditorPanel->setDisplayedTransportSeconds(midiEditorPreviewStartLocalSeconds, false);
    };

    panel->onLoopRegionChanged = [this](double startSeconds, double endSeconds)
    {
        midiEditorPreviewLoopStartSeconds = juce::jmax(0.0, startSeconds);
        midiEditorPreviewLoopEndSeconds = juce::jmax(midiEditorPreviewLoopStartSeconds, endSeconds);
        midiEditorPreviewLoopEnabled = midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds;
    };

    panel->onLoopRegionCleared = [this]
    {
        midiEditorPreviewLoopEnabled = false;
        midiEditorPreviewLoopStartSeconds = 0.0;
        midiEditorPreviewLoopEndSeconds = 0.0;
    };

    panel->onAuditionNote = [this](int pitch, int velocity, bool isOn)
    {
        if (midiEditorPanel == nullptr)
            return;

        auto editingClipIndex = midiEditorPanel->getEditingClipIndex();
        if (! juce::isPositiveAndBelow(editingClipIndex, (int) timelineModel.getClips().size()))
            return;

        auto trackIndex = timelineModel.getClips()[(size_t) editingClipIndex].trackIndex;
        if (isOn)
            engine.auditionNoteOn(trackIndex, pitch, velocity);
        else
            engine.auditionNoteOff(trackIndex, pitch);
    };

    panel->setClip(&timelineModel, clipIndex);
    panel->setPlaybackState(engine.isPlaying(), engine.isRecording() || engine.isMidiRecording());
    panel->setDisplayedTransportSeconds(0.0, false);

    auto window = std::make_unique<ManagedDocumentWindow>("Djehuti Station - MIDI Editor",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              midiEditorPanel = nullptr;
                                                              midiEditorWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setResizeLimits(820, 470, 1800, 1100);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(1100, 660);
    window->setVisible(true);
    midiEditorWindow = std::move(window);
}

bool MainComponent::startMidiEditorPreview()
{
    if (midiEditorPanel == nullptr)
        return false;

    const auto clipIndex = midiEditorPanel->getEditingClipIndex();
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return false;

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    if (clip.kind != cs::ClipKind::midi || clip.midiNotes.empty())
    {
        transportBar.setStatusText("This MIDI clip has no notes to preview.");
        return false;
    }

    if (! juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
    {
        transportBar.setStatusText("That MIDI clip is not assigned to a valid track.");
        return false;
    }

    auto instrumentPluginFile = engine.getTrackInstrumentPluginFile(clip.trackIndex);
    if (! instrumentPluginFile.existsAsFile())
    {
        transportBar.setStatusText("Load an instrument on this track to preview the MIDI clip.");
        return false;
    }

    auto localStart = juce::jlimit(0.0, clip.durationSeconds, midiEditorPanel->getLocalTransportSeconds());
    auto previewEnd = clip.durationSeconds;
    if (midiEditorPreviewLoopEnabled && midiEditorPreviewLoopEndSeconds > midiEditorPreviewLoopStartSeconds)
    {
        localStart = juce::jlimit(midiEditorPreviewLoopStartSeconds, midiEditorPreviewLoopEndSeconds, localStart);
        previewEnd = midiEditorPreviewLoopEndSeconds;
    }

    releaseMidiEditorPreviewNotes();
    midiEditorPreviewPlaying = true;
    midiEditorPreviewStartWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    midiEditorPreviewStartLocalSeconds = localStart;
    midiEditorPreviewLastLocalSeconds = localStart;
    midiEditorPreviewEndLocalSeconds = previewEnd;
    if (midiEditorPanel != nullptr)
    {
        midiEditorPanel->setDisplayedTransportSeconds(localStart, false);
        midiEditorPanel->setPlaybackState(true, false);
    }
    updateMidiEditorPreviewNotes(localStart, true);
    transportBar.setStatusText("Previewing MIDI clip inside the editor.");
    return true;
}

void MainComponent::stopMidiEditorPreview(bool resetPlayheadToLoopStart)
{
    releaseMidiEditorPreviewNotes();

    midiEditorPreviewPlaying = false;
    auto nextLocalSeconds = midiEditorPreviewStartLocalSeconds;
    if (resetPlayheadToLoopStart && midiEditorPreviewLoopEnabled)
        nextLocalSeconds = midiEditorPreviewLoopStartSeconds;

    if (midiEditorPanel != nullptr)
    {
        midiEditorPanel->setDisplayedTransportSeconds(nextLocalSeconds, false);
        midiEditorPanel->setPlaybackState(engine.isPlaying(), engine.isRecording() || engine.isMidiRecording());
    }
}

void MainComponent::updateMidiEditorPreviewNotes(double currentLocalSeconds, bool restartCycle)
{
    if (midiEditorPanel == nullptr)
        return;

    const auto clipIndex = midiEditorPanel->getEditingClipIndex();
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return;

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    if (! juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
        return;

    if (restartCycle)
        releaseMidiEditorPreviewNotes();

    for (const auto& note : clip.midiNotes)
    {
        const auto noteStartSeconds = timelineModel.beatToSeconds(note.startBeats);
        const auto noteEndSeconds = timelineModel.beatToSeconds(note.startBeats + note.lengthBeats);
        const bool shouldBeActive = currentLocalSeconds >= noteStartSeconds && currentLocalSeconds < noteEndSeconds;
        const bool isActive = midiEditorPreviewActiveNoteIds.contains(note.id);

        if (shouldBeActive && ! isActive)
        {
            engine.auditionNoteOn(clip.trackIndex, note.pitch, note.velocity);
            midiEditorPreviewActiveNoteIds.addIfNotAlreadyThere(note.id);
        }
        else if (! shouldBeActive && isActive)
        {
            engine.auditionNoteOff(clip.trackIndex, note.pitch);
            midiEditorPreviewActiveNoteIds.removeString(note.id);
        }
    }

    midiEditorPreviewLastLocalSeconds = currentLocalSeconds;
}

void MainComponent::releaseMidiEditorPreviewNotes()
{
    if (midiEditorPanel != nullptr)
    {
        const auto clipIndex = midiEditorPanel->getEditingClipIndex();
        if (juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        {
            const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
            if (juce::isPositiveAndBelow(clip.trackIndex, engine.getTrackCount()))
            {
                for (const auto& note : clip.midiNotes)
                {
                    if (midiEditorPreviewActiveNoteIds.contains(note.id))
                        engine.auditionNoteOff(clip.trackIndex, note.pitch);
                }
            }
        }
    }

    midiEditorPreviewActiveNoteIds.clear();
    engine.requestAllNotesOff();
}

void MainComponent::refreshFxStackWindow()
{
    if (fxStackPanel == nullptr || ! pluginRackBar.isTrackContext())
        return;

    const auto trackIndex = pluginRackBar.getTrackIndex();
    fxStackPanel->setTrackName("Track " + juce::String(trackIndex + 1) + " - " + engine.getTrackName(trackIndex));
    fxStackPanel->setPlugins(engine.getTrackPluginNames(trackIndex),
                             engine.getTrackPluginBypassStates(trackIndex));
}

juce::DocumentWindow* MainComponent::findPluginEditorWindow(const juce::String& key) const
{
    auto it = std::find_if(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                           [&key](const PluginEditorWindowEntry& entry)
                           {
                               return entry.key == key && entry.window != nullptr;
                           });
    return it != pluginEditorWindows.end() ? it->window.get() : nullptr;
}

void MainComponent::refreshTrackPluginEditorState(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto hasOpenEditor = std::any_of(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                                     [trackIndex](const PluginEditorWindowEntry& entry)
                                     {
                                         return entry.trackIndex == trackIndex && entry.window != nullptr;
                                     });
    engine.setTrackHasOpenEditor(trackIndex, hasOpenEditor);
}

void MainComponent::closePluginEditorWindow(const juce::String& key)
{
    auto it = std::find_if(pluginEditorWindows.begin(), pluginEditorWindows.end(),
                           [&key](const PluginEditorWindowEntry& entry)
                           {
                               return entry.key == key;
                           });
    if (it == pluginEditorWindows.end())
        return;

    auto trackIndex = it->trackIndex;
    it->window.reset();
    pluginEditorWindows.erase(it);
    if (trackIndex >= 0)
        refreshTrackPluginEditorState(trackIndex);
}

void MainComponent::closePluginEditorWindowsForTrack(int trackIndex)
{
    for (auto it = pluginEditorWindows.begin(); it != pluginEditorWindows.end();)
    {
        if (it->trackIndex == trackIndex)
        {
            it->window.reset();
            it = pluginEditorWindows.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (trackIndex >= 0)
        refreshTrackPluginEditorState(trackIndex);
}

void MainComponent::openTrackPluginEditor(int trackIndex, int slotIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto windowKey = "track-plugin-" + juce::String(trackIndex) + "-" + juce::String(slotIndex);
    if (auto* existingWindow = findPluginEditorWindow(windowKey))
    {
        existingWindow->toFront(true);
        return;
    }

    auto* editor = engine.createTrackPluginEditor(trackIndex, slotIndex);
    if (editor == nullptr)
        return;

    auto pluginNames = engine.getTrackPluginNames(trackIndex);
    auto pluginName = juce::isPositiveAndBelow(slotIndex, pluginNames.size()) ? pluginNames[slotIndex]
                                                                              : "Plugin";
    auto window = std::make_unique<ManagedDocumentWindow>("Track " + juce::String(trackIndex + 1) + " - " + pluginName,
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this, windowKey]
                                                          {
                                                              closePluginEditorWindow(windowKey);
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(editor, true);
    window->centreWithSize(900, 650);
    window->setVisible(true);
    pluginEditorWindows.push_back({ windowKey, trackIndex, std::move(window) });

    engine.reapplyTrackPluginState(trackIndex, slotIndex);
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::Timer::callAfterDelay(450, [safeThis, trackIndex, slotIndex]
    {
        if (safeThis != nullptr)
            safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
    });

    // Don't engage the live audio path (and start calling processBlock from the audio thread)
    // until the editor is actually showing with a real size, not just after a guessed delay - a
    // fixed delay that's fine for a light editor (e.g. TAL-NoiseMaker) can still be too short for
    // a heavier one (e.g. a sampler loading kit graphics), and the resulting race is intermittent
    // rather than a hard crash every time, which matches what was observed.
    pollPluginEditorReady(windowKey, trackIndex, juce::Component::SafePointer<juce::Component>(editor), 30);
}

void MainComponent::pollPluginEditorReady(const juce::String& windowKey,
                                          int trackIndex,
                                          juce::Component::SafePointer<juce::Component> editorPointer,
                                          int attemptsRemaining)
{
    if (findPluginEditorWindow(windowKey) == nullptr)
        return; // Editor was closed or replaced before it became ready - nothing to engage.

    auto isReady = editorPointer != nullptr && editorPointer->isShowing()
                  && editorPointer->getWidth() > 0 && editorPointer->getHeight() > 0;

    if (isReady || attemptsRemaining <= 0)
    {
        engine.setTrackHasOpenEditor(trackIndex, true);

        auto slotText = windowKey.fromLastOccurrenceOf("-", false, false);
        auto slotIndex = slotText.getIntValue();
        if (slotIndex >= 0)
        {
            engine.reapplyTrackPluginState(trackIndex, slotIndex);

            auto safeThis = juce::Component::SafePointer<MainComponent>(this);
            juce::Timer::callAfterDelay(900, [safeThis, trackIndex, slotIndex]
            {
                if (safeThis != nullptr)
                    safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
            });

            juce::Timer::callAfterDelay(2200, [safeThis, trackIndex, slotIndex]
            {
                if (safeThis != nullptr)
                    safeThis->engine.reapplyTrackPluginState(trackIndex, slotIndex);
            });
        }

        return;
    }

    juce::Component::SafePointer<MainComponent> safeThis(this);
    juce::Timer::callAfterDelay(150, [safeThis, windowKey, trackIndex, editorPointer, attemptsRemaining]
    {
        if (safeThis != nullptr)
            safeThis->pollPluginEditorReady(windowKey, trackIndex, editorPointer, attemptsRemaining - 1);
    });
}

void MainComponent::configureVstSearchPaths()
{
    if (! ensureStorageRootConfigured())
        return;

    auto currentPaths = vstPluginCatalog.getSearchPaths();
    pluginChooser = std::make_unique<juce::FileChooser>("Choose a VST folder",
                                                        currentPaths.isEmpty() ? juce::File{} : juce::File(currentPaths[0]),
                                                        juce::String{},
                                                        true);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    pluginChooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                               [safeThis, currentPaths](const juce::FileChooser& chooser) mutable
                               {
                                   if (safeThis == nullptr)
                                       return;

                                   auto selectedDirectory = chooser.getResult();
                                   safeThis->pluginChooser.reset();

                                   if (! selectedDirectory.isDirectory())
                                       return;

                                   auto updatedPaths = currentPaths;
                                   updatedPaths.addIfNotAlreadyThere(selectedDirectory.getFullPathName());
                                   updatedPaths.trim();
                                   updatedPaths.removeEmptyStrings();
                                   updatedPaths.removeDuplicates(false);

                                   safeThis->vstPluginCatalog.setSearchPaths(updatedPaths);
                                   safeThis->saveAppSettings();
                                   safeThis->rescanVstCatalog();
                                   safeThis->transportBar.setStatusText("Added VST folder: " + selectedDirectory.getFileName());
                               });
}

juce::StringArray MainComponent::parseVstPathList(const juce::String& rawList)
{
    juce::StringArray result;
    auto tokens = juce::StringArray::fromTokens(rawList, ";", "");

    for (auto token : tokens)
    {
        token = token.trim();
        if (token.isEmpty())
            continue;

        // Expand %ENV_VAR% style references (e.g. %LOCALAPPDATA%, %PROGRAMFILES%).
        for (int guard = 0; guard < 8 && token.contains("%"); ++guard)
        {
            auto start = token.indexOfChar('%');
            auto end = token.indexOfChar(start + 1, '%');
            if (end < 0)
                break;

            auto varName = token.substring(start + 1, end);
            auto varValue = juce::SystemStats::getEnvironmentVariable(varName, {});
            if (varValue.isEmpty())
                break;

            token = token.substring(0, start) + varValue + token.substring(end + 1);
        }

        // Don't require the folder to exist yet - a dev project's VST3 output folder (e.g.
        // "...\Builds\VisualStudio2022\x64\Debug\VST3") may not exist until it's been built once,
        // and the scanner already skips missing search paths gracefully every rescan, so
        // registering it now means it starts working the moment it does exist.
        result.add(juce::File(token).getFullPathName());
    }

    return result;
}

void MainComponent::importVstPathList()
{
    if (! ensureStorageRootConfigured())
        return;

    auto* alertWindow = new juce::AlertWindow("Import VST Path List",
                                              "Paste a semicolon-separated list of VST folders (e.g. copied from Reaper's VST path setting). "
                                              "Windows %ENV_VAR% references are expanded automatically.",
                                              juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("pathList", "", "Path list");
    alertWindow->addButton("Import", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alertWindow](int result)
    {
        if (safeThis == nullptr || result != 1)
            return;

        auto rawList = alertWindow->getTextEditorContents("pathList");
        auto parsedPaths = parseVstPathList(rawList);

        if (parsedPaths.isEmpty())
        {
            safeThis->transportBar.setStatusText("No valid folders found in the pasted list.");
            return;
        }

        auto updatedPaths = safeThis->vstPluginCatalog.getSearchPaths();
        for (const auto& path : parsedPaths)
            updatedPaths.addIfNotAlreadyThere(path);
        updatedPaths.trim();
        updatedPaths.removeEmptyStrings();
        updatedPaths.removeDuplicates(false);

        safeThis->vstPluginCatalog.setSearchPaths(updatedPaths);
        safeThis->saveAppSettings();
        safeThis->rescanVstCatalog();
        safeThis->transportBar.setStatusText("Imported " + juce::String(parsedPaths.size()) + " VST folder(s).");
    }), true);
}

void MainComponent::editControlSurfaceMappings()
{
    if (! ensureStorageRootConfigured())
        return;

    // Was previously "write control-surface-mappings.json to disk, then reveal/open it in an
    // external editor" -- that only worked because the mappings lived in a raw OS file. Now that
    // they live in the VFS (no OS-visible file to open), the equivalent hand-edit escape hatch is
    // a paste-back text dialog, the same pattern importVstPathList() already established for VST
    // search paths: show the current JSON, let the user edit it, re-import on OK.
    auto* alertWindow = new juce::AlertWindow("Edit Control Surface Mappings",
                                              "Edit the mapping JSON directly, then Save to re-import it.",
                                              juce::MessageBoxIconType::NoIcon);
    alertWindow->addTextEditor("mappingsJson", juce::JSON::toString(controlSurfaceMappings.toVar(), true), "Mappings JSON");
    alertWindow->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    alertWindow->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    alertWindow->enterModalState(true, juce::ModalCallbackFunction::create([safeThis, alertWindow](int result)
    {
        if (safeThis == nullptr || result != 1)
            return;

        auto editedJson = alertWindow->getTextEditorContents("mappingsJson");
        ControlSurfaceMappingStore edited;
        juce::String errorMessage;
        if (! edited.loadFromVar(juce::JSON::parse(editedJson), errorMessage))
        {
            safeThis->transportBar.setStatusText("Could not parse mappings: " + errorMessage);
            return;
        }

        safeThis->controlSurfaceMappings = edited;
        if (! creation::services::SuiteVfsJsonStore::saveJson("station-control-surface-mappings.json", edited.toVar(), errorMessage))
        {
            safeThis->transportBar.setStatusText(errorMessage);
            return;
        }

        safeThis->transportBar.setStatusText("Saved control surface mappings.");
    }), true);
}

void MainComponent::showMidiLearnDialog(const juce::String& targetId, const juce::String& displayLabel)
{
    if (midiLearnWindow != nullptr)
    {
        midiLearnWindow->toFront(true);
        return;
    }

    MidiLearnPanel::ExistingBinding existing;
    for (const auto& profile : controlSurfaceMappings.getProfiles())
    {
        auto found = false;
        for (const auto& binding : profile.bindings)
        {
            if (binding.targetId != targetId)
                continue;

            existing.found = true;
            existing.deviceLabel = profile.devicePattern.isNotEmpty() ? profile.devicePattern : "any device";
            existing.channel = binding.channel;
            existing.number = binding.number;
            existing.isController = binding.isController;
            found = true;
            break;
        }
        if (found)
            break;
    }

    auto panel = std::make_unique<MidiLearnPanel>(engine, targetId, displayLabel, existing);
    auto* panelPtr = panel.get();

    auto window = std::make_unique<ManagedDocumentWindow>("Learn MIDI Binding",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              midiLearnWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(false, false);

    // The panel stays open after a capture so the user can see what was saved - only Close (or
    // the window's own close control) dismisses it. Deferred via callAsync since onCancelled
    // fires from inside a MidiLearnPanel button click, and resetting midiLearnWindow
    // synchronously would destroy the panel - and this lambda - while still on the call stack.
    panelPtr->onLearned = [this, targetId](juce::String deviceId, int channel, int number, bool isCC)
    {
        applyLearnedMidiBinding(targetId, deviceId, channel, number, isCC);
    };
    panelPtr->onCancelled = [this]
    {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->midiLearnWindow.reset();
        });
    };

    window->setContentOwned(panel.release(), true);
    window->centreWithSize(460, 400);
    window->setVisible(true);
    midiLearnWindow = std::move(window);
}

void MainComponent::requestGenericMidiLearn(const juce::String& displayLabel,
                                            std::function<void(juce::String deviceId, int channel, int number, bool isCC)> onLearned,
                                            WorkstationAudioEngine::MidiLearnKind expectedKind)
{
    // Shares the single app-wide learn dialog with showMidiLearnDialog above (only one binding
    // can sensibly be learned at a time) - but the result goes straight back to the caller instead
    // of being saved into ControlSurfaceMappingStore under a transport targetId. Callers like
    // Signal Lab's MIDI Control nodes keep their own binding on their own model instead.
    if (midiLearnWindow != nullptr)
    {
        midiLearnWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<MidiLearnPanel>(engine, juce::String(), displayLabel, MidiLearnPanel::ExistingBinding {}, expectedKind);
    auto* panelPtr = panel.get();

    auto window = std::make_unique<ManagedDocumentWindow>("Learn MIDI Binding",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              midiLearnWindow.reset();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(false, false);

    // Auto-close after a successful capture -- once it's learned there's
    // nothing left to do in this dialog, so don't make the user close it
    // by hand. Deferred via callAsync: onLearned fires from inside this
    // same MidiLearnPanel's own timerCallback(), so resetting
    // midiLearnWindow (which destroys that panel) must not happen as a
    // continuation of that call, same reasoning as the Signal Lab scope
    // tool-window close fix.
    panelPtr->onLearned = [this, onLearned = std::move(onLearned)](juce::String deviceId, int channel, int number, bool isCC)
    {
        if (onLearned)
            onLearned(deviceId, channel, number, isCC);

        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->midiLearnWindow.reset();
        });
    };
    panelPtr->onCancelled = [this]
    {
        juce::Component::SafePointer<MainComponent> safeThis(this);
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->midiLearnWindow.reset();
        });
    };

    window->setContentOwned(panel.release(), true);
    window->centreWithSize(460, 400);
    window->setVisible(true);
    midiLearnWindow = std::move(window);
}

void MainComponent::applyLearnedMidiBinding(const juce::String& targetId, const juce::String& deviceId, int channel, int number, bool isCC)
{
    juce::String deviceName = "*";
    if (deviceId.isNotEmpty())
    {
        for (const auto& device : juce::MidiInput::getAvailableDevices())
        {
            if (device.identifier == deviceId)
            {
                deviceName = device.name;
                break;
            }
        }
    }

    auto* profile = controlSurfaceMappings.findProfileById("custom-bindings");
    if (profile == nullptr)
    {
        ControlSurfaceMappingStore::Profile newProfile;
        newProfile.id = "custom-bindings";
        newProfile.displayName = "Custom Bindings";
        newProfile.devicePattern = deviceName;
        newProfile.usage = "*";
        newProfile.description = "Bindings created via right-click -> Learn MIDI Binding.";
        controlSurfaceMappings.addProfile(std::move(newProfile));
        profile = controlSurfaceMappings.findProfileById("custom-bindings");
    }
    else if (deviceName != "*" && ! profile->matchesDevice(deviceName))
    {
        profile->devicePattern += "," + deviceName;
    }

    if (profile == nullptr)
        return;

    // Only replace an exact duplicate (same target + same channel/number, i.e. re-learning the
    // identical control) - a DIFFERENT device or control bound to the same targetId is meant to
    // coexist, so more than one piece of hardware can trigger the same action.
    for (int i = profile->bindings.size(); --i >= 0;)
    {
        const auto& existing = profile->bindings.getReference(i);
        if (existing.targetId == targetId && existing.channel == channel && existing.number == number)
            profile->bindings.remove(i);
    }

    ControlSurfaceMappingStore::Binding binding;
    binding.triggerType = "transport";
    binding.actionId = targetId.fromFirstOccurrenceOf("transport_", false, false);
    binding.targetId = targetId;
    binding.behavior = "momentary";
    binding.channel = channel;
    binding.number = number;
    binding.isController = isCC;
    profile->bindings.add(binding);

    juce::String saveError;
    if (! creation::services::SuiteVfsJsonStore::saveJson("station-control-surface-mappings.json", controlSurfaceMappings.toVar(), saveError))
    {
        transportBar.setStatusText("Could not save MIDI binding: " + saveError);
        return;
    }

    midiSurface.setControlSurfaceMappings(controlSurfaceMappings);
    transportBar.setStatusText("Learned MIDI binding for " + targetId
                               + (deviceName != "*" ? (" on " + deviceName) : "") + ".");
}

void MainComponent::rescanVstCatalog()
{
    vstPluginCatalog.rescan();
    pluginRackBar.setCatalogSummary(vstPluginCatalog.describeSummary());
    refreshPluginsPanel();

    if (fxStackPanel != nullptr)
        fxStackPanel->setCatalog(vstPluginCatalog.getEntries());
}

void MainComponent::showPluginLoadMenu(const std::function<void(const juce::File&)>& onPluginChosen)
{
    // Plugin selection is framework-mediated, never an OS file pick: the app scans VST3 search
    // paths at startup (and on demand via "Rescan") into vstPluginCatalog, and this menu offers
    // only what that scan found. No juce::FileChooser here -- reaching out to the OS to browse for
    // a plugin DLL directly is out of scope for this menu; the OS file dialog is reserved for
    // actual asset import/export, not plugin discovery.
    auto entries = vstPluginCatalog.getEntries();
    if (entries.isEmpty())
    {
        juce::PopupMenu emptyMenu;
        emptyMenu.addItem(1, "Rescan VST folders");
        emptyMenu.addItem(2, "Manage VST folders...");
        emptyMenu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pluginRackBar),
                                [this](int result)
                                {
                                    if (result == 1)
                                    {
                                        rescanVstCatalog();
                                        transportBar.setStatusText(vstPluginCatalog.describeSummary());
                                    }
                                    else if (result == 2)
                                    {
                                        configureVstSearchPaths();
                                    }
                                });
        return;
    }

    juce::PopupMenu menu;
    menu.addItem(1, "Rescan VST folders");
    menu.addItem(2, "Manage VST folders...");
    menu.addSeparator();

    for (int index = 0; index < entries.size(); ++index)
        menu.addItem(100 + index, entries.getReference(index).name);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&pluginRackBar),
                       [this, onPluginChosen, entries](int result)
                       {
                           if (result == 1)
                           {
                               rescanVstCatalog();
                               transportBar.setStatusText(vstPluginCatalog.describeSummary());
                               return;
                           }

                           if (result == 2)
                           {
                               configureVstSearchPaths();
                               return;
                           }

                           if (result >= 100 && result < 100 + entries.size())
                               onPluginChosen(entries.getReference(result - 100).file);
                       });
}

void MainComponent::refreshPluginsPanel()
{
    pluginsPanel.setSearchPaths(vstPluginCatalog.getSearchPaths());
    pluginsPanel.setPlugins(vstPluginCatalog.getEntries());
    pluginsPanel.setStatusText(vstPluginCatalog.describeSummary());

    if (pluginRackBar.isTrackContext())
        pluginsPanel.setInsertTargetDescription("Current insert target: Track " + juce::String(pluginRackBar.getTrackIndex() + 1)
                                                + " - " + engine.getTrackName(pluginRackBar.getTrackIndex()));
    else
        pluginsPanel.setInsertTargetDescription("Current insert target: Master");
}

void MainComponent::loadPluginIntoCurrentInsert(const juce::File& file)
{
    if (! file.existsAsFile() && ! file.isDirectory())
        return;

    if (pluginRackBar.isTrackContext())
        closePluginEditorWindowsForTrack(pluginRackBar.getTrackIndex());
    else
        closePluginEditorWindow("master-plugin");

    juce::String errorMessage;
    auto loaded = pluginRackBar.isTrackContext()
        ? engine.loadTrackPlugin(pluginRackBar.getTrackIndex(), file, errorMessage)
        : engine.loadMasterPlugin(file, errorMessage);

    if (loaded)
    {
        refreshInsertRack();
        refreshPluginsPanel();
        transportBar.setStatusText("Loaded plugin: " + file.getFileNameWithoutExtension());
    }
    else
    {
        transportBar.setStatusText("Plugin load failed: " + errorMessage);
        pluginsPanel.setStatusText("Plugin load failed: " + errorMessage);
    }
}

void MainComponent::showTour()
{
    tourOverlay.start();
    tourOverlay.toFront(true);
}

void MainComponent::importProjectSounds()
{
    if (! ensureStorageRootConfigured())
        return;

    if (! projectSession.isValid())
    {
        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::createProject(suiteSettings, creation::assets::SuiteAppDomain::station, "New Project", "1.0.0", "1.0.0", projectSession, errorMessage))
        {
            transportBar.setStatusText("Could not create a project for imported sounds.");
            return;
        }

        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
    }

    assetChooser = std::make_unique<juce::FileChooser>("Import project sounds",
                                                       juce::File{},
                                                       "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");

    assetChooser->launchAsync(juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::canSelectMultipleItems,
                              [this](const juce::FileChooser& result)
                              {
                                  auto selectedFiles = result.getResults();
                                  assetChooser.reset();

                                  if (selectedFiles.isEmpty())
                                      return;

                                  auto importedCount = 0;
                                  juce::String lastError;

                                  for (const auto& sourceFile : selectedFiles)
                                  {
                                      juce::String errorMessage;
                                      auto imported = juce::File();
                                      if (imported.existsAsFile())
                                          ++importedCount;
                                      else
                                          lastError = errorMessage;
                                  }

                                  refreshProjectAssets();
                                  saveSessionToDisk();

                                  if (importedCount > 0)
                                      transportBar.setStatusText("Imported " + juce::String(importedCount) + " sound(s) into this project.");
                                  else if (lastError.isNotEmpty())
                                      transportBar.setStatusText(lastError);
                              });
}

void MainComponent::refreshProjectAssets()
{
    if (! projectSession.isValid())
    {
        contentPanel.setProjectAssets({});
        refreshAiContextStore();
        return;
    }

    auto projectAssets = projectSession.getManifest().assetCatalog.assets;
    contentPanel.setProjectAssets(projectAssets);
    refreshAiContextStore();
}

void MainComponent::refreshContentLibrary()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setStoragePath({});
        contentPanel.setItems({});
        contentPanel.setTutorialItems({});
        contentPanel.setStatusText("Choose a local storage location to initialize the content library.");
        refreshAiContextStore();
        return;
    }

    contentPanel.setStoragePath(creation::suite::getContentDirectory(suiteSettings).getFullPathName());

    juce::String errorMessage;
    if (! contentLibrary.loadFromStorage(creation::suite::getContentDirectory(suiteSettings).getChildFile("BuiltIn"),
                                         creation::suite::getContentDirectory(suiteSettings).getChildFile("Downloaded"),
                                         creation::suite::getContentDirectory(suiteSettings).getChildFile("User"),
                                         errorMessage))
    {
        contentPanel.setItems({});
        contentPanel.setTutorialItems({});
        contentPanel.setStatusText(errorMessage);
        refreshAiContextStore();
        return;
    }

    auto combinedItems = contentLibrary.getItems();
    contentPanel.setItems(combinedItems);
    refreshTutorialLibrary();
    contentPanel.setStatusText(contentLibrary.createSummaryText());
    refreshAiContextStore();

    if (! authenticated)
        return;

    contentPanel.setStatusText(contentLibrary.createSummaryText() + "  |  Syncing LagDaemon...");
    auto token = authSession.getSession().token;

    std::thread([this, token, combinedItems]()
    {
        juce::Array<ContentApiClient::LibraryItem> remoteLibrary;
        juce::String remoteError;
        if (! contentApiClient.fetchLibrary(token, "creation-station", remoteLibrary, remoteError))
        {
            juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), remoteError]
            {
                if (safeThis != nullptr)
                    safeThis->contentPanel.setStatusText(remoteError);
            });
            return;
        }

        auto mergedItems = combinedItems;
        juce::StringArray installedIds;
        for (const auto& localItem : combinedItems)
            installedIds.addIfNotAlreadyThere(localItem.id);

        for (const auto& remoteItem : remoteLibrary)
        {
            if (installedIds.contains(remoteItem.id))
                continue;

            ContentLibrary::Item item;
            item.id = remoteItem.id;
            item.name = remoteItem.name;
            item.type = remoteItem.itemType;
            item.category = remoteItem.tags.isEmpty() ? "LagDaemon Content" : remoteItem.tags.joinIntoString(", ");
            item.description = remoteItem.description.isNotEmpty() ? remoteItem.description : ("Remote " + remoteItem.itemType + " from LagDaemon.");
            item.requiredTier = remoteItem.requiredTier;
            item.version = remoteItem.version;
            item.origin = ContentLibrary::Origin::remote;
            item.accessState = remoteItem.accessState == "locked" ? ContentLibrary::AccessState::locked
                                                                  : ContentLibrary::AccessState::available;
            item.fileSizeBytes = remoteItem.sizeBytes;
            mergedItems.add(item);
        }

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), mergedItems]
        {
            if (safeThis != nullptr)
            {
                safeThis->contentPanel.setItems(mergedItems);
                safeThis->refreshTutorialLibrary();
                safeThis->contentPanel.setStatusText("Library ready: " + juce::String(mergedItems.size()) + " local + remote items.");
                safeThis->refreshAiContextStore();
            }
        });
    }).detach();
}

void MainComponent::refreshTutorialLibrary()
{
    juce::Array<ContentPanel::TutorialItem> tutorials;

    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setTutorialItems(tutorials);
        return;
    }

    auto collect = [&tutorials](const juce::File& directory, bool builtIn)
    {
        juce::Array<juce::File> files;
        directory.findChildFiles(files, juce::File::findFiles, false, "*.nalm");

        for (const auto& file : files)
        {
            ContentPanel::TutorialItem item;
            item.file = file;
            item.builtIn = builtIn;
            item.name = file.getFileNameWithoutExtension().replace("-", " ");
            item.description = builtIn ? "Bundled guided demo/tutorial." : "User-authored guided demo/tutorial.";

            auto firstLine = file.loadFileAsString().upToFirstOccurrenceOf("\n", false, false).trim();
            if (firstLine.startsWithIgnoreCase("tutorial "))
            {
                auto quotedName = firstLine.fromFirstOccurrenceOf("\"", false, false);
                if (quotedName.isNotEmpty())
                    item.name = quotedName.upToLastOccurrenceOf("\"", false, false);
            }

            tutorials.add(item);
        }
    };

    collect(creation::suite::getTutorialsDirectory(suiteSettings).getChildFile("BuiltIn"), true);
    collect(creation::suite::getTutorialsDirectory(suiteSettings).getChildFile("User"), false);
    contentPanel.setTutorialItems(tutorials);
}

void MainComponent::launchTutorialItem(const ContentPanel::TutorialItem& item)
{
    if (! item.file.existsAsFile())
    {
        transportBar.setStatusText("That tutorial file is not available.");
        return;
    }

    cw::tutorial::ScriptCompiler compiler;
    cw::tutorial::Script script;
    juce::String errorMessage;
    if (! compiler.compile(item.file.loadFileAsString(), script, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    tourOverlay.setSteps(buildTutorialSteps(script));
    showTour();
    transportBar.setStatusText("Started tutorial: " + script.name);
}

void MainComponent::refreshAiContextStore()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contextEngine.clearDocuments();
        return;
    }

    juce::String errorMessage;
    if (! contextStore.rebuild(projectSession,
                               suiteSettings,
                               contentLibrary,
                               workspaceModeName(activeMode),
                               dslPanel.getSourceText(),
                               errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    contextEngine.replaceDocuments(contextStore.getDocuments());
}

MainComponent::WorkspaceMode MainComponent::workspaceModeFromString(const juce::String& modeName) const
{
    auto normalized = modeName.trim().toLowerCase();

    if (normalized == "tracker")
        return WorkspaceMode::tracker;
    if (normalized == "sampler")
        return WorkspaceMode::sampler;
    // "arrange" named the now-retired standalone Foley Stage; that manual sound-placement
    // workflow lives in Tracker now (video track + clip editing). "foley" now names the NEW
    // node-graph sequencing workspace instead - a different, later addition to this same file.
    if (normalized == "arrange")
        return WorkspaceMode::tracker;
    if (normalized == "foley")
        return WorkspaceMode::foley;
    if (normalized == "signal")
        return WorkspaceMode::signal;
    if (normalized == "library" || normalized == "assets")
        return WorkspaceMode::tracker;
    if (normalized == "mix" || normalized == "layers")
        return WorkspaceMode::mix;
    if (normalized == "plugins" || normalized == "plugin")
        return WorkspaceMode::plugins;
    if (normalized == "code" || normalized == "script")
        return WorkspaceMode::code;
    if (normalized == "record" || normalized == "capture")
        return WorkspaceMode::record;
    if (normalized == "score" || normalized == "song" || normalized == "notation")
        return WorkspaceMode::score;
    if (normalized == "settings" || normalized == "options")
        return WorkspaceMode::settings;

    return activeMode;
}

void MainComponent::configureTutorialOverlay()
{
    cw::tutorial::Script script;

    auto loadFromFile = [this](const juce::File& file, cw::tutorial::Script& loadedScript, juce::String& errorMessage) -> bool
    {
        if (! file.existsAsFile())
            return false;

        cw::tutorial::ScriptCompiler compiler;
        return compiler.compile(file.loadFileAsString(), loadedScript, errorMessage);
    };

    juce::String errorMessage;
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        auto builtInDirectory = creation::suite::getTutorialsDirectory(suiteSettings).getChildFile("BuiltIn");
        auto sampleFile = builtInDirectory.getChildFile("getting-started-demo.nalm");
        auto builtInSource = cw::tutorial::getBuiltInGettingStartedTutorialSource();

        if (! sampleFile.existsAsFile() || sampleFile.loadFileAsString() != builtInSource)
            sampleFile.replaceWithText(builtInSource);

        auto userFile = creation::suite::getTutorialsDirectory(suiteSettings).getChildFile("User").getChildFile("getting-started-demo.nalm");

        if (! loadFromFile(userFile, script, errorMessage))
        {
            errorMessage.clear();
            loadFromFile(sampleFile, script, errorMessage);
        }
    }

    if (script.scenes.isEmpty())
        script = cw::tutorial::makeGettingStartedTutorial();

    tourOverlay.setSteps(buildTutorialSteps(script));
}

std::vector<TourGuideOverlay::Step> MainComponent::buildTutorialSteps(const cw::tutorial::Script& script)
{
    std::vector<TourGuideOverlay::Step> steps;
    steps.reserve((size_t) script.scenes.size());

    for (const auto& scene : script.scenes)
    {
        TourGuideOverlay::Step step;
        step.title = scene.title;
        step.body = scene.body;
        step.advanceOnTargetClick = scene.advanceOnTargetClick;
        step.drawConnector = scene.drawConnector;
        step.nextButtonText = scene.nextButtonText;
        step.targetBounds = [this, targetId = scene.targetId]()
        {
            return tutorialTargetBoundsForId(targetId);
        };
        step.onStepEntered = [this, actions = scene.actions]()
        {
            executeTutorialActions(actions);
        };
        steps.push_back(std::move(step));
    }

    return steps;
}

void MainComponent::executeTutorialActions(const juce::Array<cw::tutorial::Action>& actions)
{
    for (const auto& action : actions)
    {
        switch (action.type)
        {
            case cw::tutorial::ActionType::switchWorkspace:
                setWorkspaceMode(workspaceModeFromString(action.value));
                break;

            case cw::tutorial::ActionType::applySignalTemplate:
                signalLabPanel.applyAiTemplate(action.value);
                break;

        }
    }
}

juce::Rectangle<int> MainComponent::tutorialTargetBoundsForId(const juce::String& targetId) const
{
    auto id = targetId.trim().toLowerCase();

    if (id == "transport")
        return transportBar.getBounds();
    if (id == "modes")
        return menuBar != nullptr ? menuBar->getBounds() : juce::Rectangle<int>();
    if (id == "signal")
        return signalLabPanel.getBounds();
    if (id == "library")
        return contentPanel.getBounds();
    if (id == "mix" || id == "layers")
        return mixerPanel.getBounds();
    if (id == "plugins" || id == "plugin")
        return pluginsPanel.getBounds();
    if (id == "code" || id == "script")
        return dslPanel.getBounds();
    if (id == "record" || id == "capture")
        return recordView.getBounds();
    if (id == "ai")
        return aiPanel.getBounds();

    return {};
}

void MainComponent::executeAiTaskStep(const CreationStationTaskPlanner::TaskStep& step)
{
    juce::StringArray actionNotes;

    for (const auto& action : step.actions)
    {
        switch (action.target)
        {
            case CreationStationTaskPlanner::ActionTarget::workspace:
                if (action.command == "switch-mode")
                {
                    auto mode = workspaceModeFromString(action.stringValue);
                    setWorkspaceMode(mode);
                    actionNotes.add("opened " + workspaceModeName(mode));
                }
                break;

            case CreationStationTaskPlanner::ActionTarget::signalLab:
                if (action.command == "apply-template")
                {
                    signalLabPanel.applyAiTemplate(action.stringValue);
                    actionNotes.add("seeded Signal Lab with " + action.stringValue);
                }
                else if (action.command == "preview-signal")
                {
                    if (signalLabPanel.previewCurrentSignal())
                        actionNotes.add("previewed the current signal");
                    else
                        actionNotes.add("could not preview because no signal is ready yet");
                }
                break;

            case CreationStationTaskPlanner::ActionTarget::transport:
            case CreationStationTaskPlanner::ActionTarget::context:
                break;
        }
    }

    if (actionNotes.isEmpty())
        transportBar.setStatusText("AI step complete: " + step.title);
    else
        transportBar.setStatusText("AI step complete: " + step.title + " - " + actionNotes.joinIntoString(", "));
}

void MainComponent::setAiSidebarCollapsed(bool shouldCollapse)
{
    aiSidebarCollapsed = shouldCollapse;
    aiPanel.setCollapsed(shouldCollapse);
    resized();
    markLayoutDirty();
}

void MainComponent::showAiSidebar()
{
    setAiSidebarCollapsed(false);
}

void MainComponent::launchAiCompletion(const CreationStationContextEngine::ContextPacket& packet)
{
    if (aiCompletionInFlight)
        return;

    const auto profile = creation::services::SuiteAiProviderRuntime::resolveProfile(aiProviderSettings.providerId);
    if (creation::services::SuiteAiProviderRuntime::requiresApiKey(profile, aiProviderSettings.apiKey))
    {
        aiPanel.setAssistantResponse("Enter your provider API key in Settings first.");
        transportBar.setStatusText("AI provider key is missing.");
        return;
    }

    if (pendingAiPrompt.trim().isEmpty())
    {
        aiPanel.setAssistantResponse("Type a prompt first.");
        return;
    }

    aiCompletionInFlight = true;

    auto systemPrompt = appManifest.instructions;
    auto userPrompt = pendingAiPrompt;

    // Only pass along project context that scored as genuinely relevant to this prompt.
    // (packet.summary carries internal ISD tuning metrics for the app's own debugging UI —
    // it is not useful grounding for the model and was previously drowning out the user's
    // actual question, causing generic app-orientation answers regardless of what was asked.)
    juce::String contextBlock;
    if (! packet.snippets.isEmpty())
    {
        contextBlock << "Project context (use only if directly relevant to the request below):\n";
        for (const auto& snippet : packet.snippets)
            contextBlock << "- " << snippet.title << " (" << snippet.category << "): " << snippet.excerpt << "\n";
        contextBlock << "\n";
    }

    // Station's own help, chosen for this question and the panel the user is in. The same topics are what the Help window
    // shows, so the assistant and the manual never disagree.
    juce::String helpBlock;
    // Help excerpts answer how-to questions about Station; the Producer is talking about the music, so it gets none.
    const bool producerRole = aiPanel.getRole() == AiPanel::Role::producer;
    const auto helpExcerpts = producerRole ? juce::String() : helpLibrary.buildPromptContext(pendingAiQuestion, currentHelpId(), 7000);
    juce::String suppliedHelp;
    for (const auto* topic : (producerRole ? std::vector<const cs::help::Topic*>() : helpLibrary.topicsForPrompt(pendingAiQuestion, currentHelpId())))
        suppliedHelp << (suppliedHelp.isEmpty() ? "" : "; ") << topic->title;
    if (helpExcerpts.isNotEmpty())
    {
        systemPrompt << "\n\nStation help: the user's message begins with excerpts from Station's own help. When they ask how "
                        "Station works or how to do something, answer from those excerpts: follow their steps and menu names "
                        "exactly, and name the help topic (its title) you used. The excerpts are the best information available, "
                        "even where a topic is marked draft, so use them. Only if none of the excerpts relates to the question, "
                        "say so plainly and point to Help Topics (F1). The Station help overrides any older description of the "
                        "app elsewhere in these instructions.";
        helpBlock << "Station help (relevant excerpts; each is labelled with its topic):\n\n" << helpExcerpts << "\n";
    }

    userPrompt = helpBlock + contextBlock + userPrompt;

    // Providers that speak the OpenAI chat protocol go through the assistant: it can write and run FRust against
    // Station, see the result, and keep going. Other providers keep the single answer for now.
    if (StationAssistant::supportsProvider(aiProviderSettings.providerId))
    {
        launchAssistantRun(systemPrompt, userPrompt, suppliedHelp);
        return;
    }

    std::thread([safeThis = juce::Component::SafePointer<MainComponent>(this),
                 systemPrompt = std::move(systemPrompt),
                 userPrompt = std::move(userPrompt),
                 suppliedHelp]() mutable
    {
        if (safeThis == nullptr)
            return;

        creation::services::SuiteAiChatClient::ChatResult result;
        auto ok = safeThis->openAiChatClient.sendChatCompletion(safeThis->aiProviderSettings,
                                                                systemPrompt,
                                                                userPrompt,
                                                                result);

        juce::MessageManager::callAsync([safeThis,
                                         ok,
                                         suppliedHelp,
                                         result = std::move(result)]() mutable
        {
            if (safeThis == nullptr)
                return;

            safeThis->aiCompletionInFlight = false;

            if (ok)
            {
                // Show which help topics the assistant was given, so an answer can be checked against its sources.
                const auto shown = suppliedHelp.isNotEmpty() ? result.text + "\n\n_Help topics supplied: " + suppliedHelp + "_" : result.text;
                safeThis->aiPanel.setAssistantResponse(shown);
                safeThis->recordConversationTurn("assistant", shown);
                safeThis->transportBar.setStatusText("AI response ready.");
            }
            else
            {
                safeThis->aiPanel.setAssistantResponse(result.errorMessage);
                safeThis->transportBar.setStatusText(result.errorMessage);
            }
        });
    }).detach();
}

StationAssistant& MainComponent::ensureAssistant()
{
    if (assistant == nullptr)
    {
        auto embedded = [](const char* resource)
        {
            int size = 0;
            const char* data = BuiltInFrustData::getNamedResource(resource, size);
            return data != nullptr ? std::string(data, (size_t) size) : std::string();
        };
        assistant = std::make_unique<StationAssistant>(
            getAgentHost(), embedded("StationAgentApi_frust"), embedded("StationScriptGuide_md"),
            [this](const std::string& query)
            {
                // Runs on the assistant's thread; the help library is read-only data, so this is safe.
                return helpLibrary.buildPromptContext(juce::String(query), {}, 6000).toStdString();
            });
    }
    return *assistant;
}

void MainComponent::launchAssistantRun(const juce::String& systemPrompt, const juce::String& userPrompt, const juce::String& suppliedHelp)
{
    ensureAssistant();

    creation::services::SuiteAiResolvedRuntimeSettings account;
    account.providerId = aiProviderSettings.providerId;
    account.providerDisplayName = aiProviderSettings.providerDisplayName;
    account.baseUrl = aiProviderSettings.baseUrl;
    account.modelName = aiProviderSettings.modelName;
    account.apiKey = aiProviderSettings.apiKey;

    aiPanel.onStopRequested = [this]
    {
        if (assistant != nullptr)
            assistant->stop();
        transportBar.setStatusText("Stopping the assistant...");
    };

    // Who it works as this time: the Engineer (changes the project) or the Producer (talks about the music, measures).
    assistant->setRole(aiPanel.getRole() == AiPanel::Role::producer ? StationAssistant::Role::producer : StationAssistant::Role::engineer);

    aiPanel.setRunning(true);
    transportBar.setStatusText("The assistant is working...");

    const bool started = assistant->start(
        account, systemPrompt, userPrompt, pendingAiQuestion,
        [this](const juce::String& status)
        {
            aiPanel.setAssistantResponse("_" + status + "_");
        },
        [this, suppliedHelp](const StationAssistant::Outcome& outcome)
        {
            aiCompletionInFlight = false;
            aiPanel.setRunning(false);

            auto text = outcome.text;
            if (suppliedHelp.isNotEmpty() && outcome.finished)
                text << "\n\n_Help topics supplied: " << suppliedHelp << "_";
            aiPanel.setAssistantResponse(text);
            if (outcome.error.isEmpty())
                recordConversationTurn("assistant", text);
            transportBar.setStatusText(outcome.error.isNotEmpty() ? outcome.error
                                       : outcome.finished ? juce::String("AI response ready.") : juce::String("The assistant stopped."));
        });

    if (! started)
    {
        aiCompletionInFlight = false;
        aiPanel.setRunning(false);
        aiPanel.setAssistantResponse("The assistant is still working on the previous request.");
    }
}

void MainComponent::refreshAiPanelAccountsAndModels()
{
    juce::Array<AiPanel::AccountEntry> accountEntries;
    for (const auto& account : suiteAiSettings.accounts)
    {
        AiPanel::AccountEntry entry;
        entry.accountId = account.accountId;
        entry.displayName = account.accountLabel.isNotEmpty() ? account.accountLabel : account.accountId;
        accountEntries.add(entry);
    }

    const auto* selectedAccount = creation::services::SuiteAiSettingsResolver::resolveAccountForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    aiPanel.setAvailableAccounts(accountEntries, selectedAccount != nullptr ? selectedAccount->accountId : juce::String());

    if (selectedAccount == nullptr)
    {
        aiPanel.setAvailableModels({}, "Add a suite AI account in Settings -> Suite AI Accounts to use the Virtual Engineer.");
        return;
    }

    const auto modelName = creation::services::SuiteAiSettingsResolver::resolveModelNameForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    const auto statusText = selectedAccount->cachedModelIds.isEmpty()
        ? juce::String("No cached models for this account yet - refresh it from Settings -> Suite AI Accounts.")
        : juce::String(selectedAccount->cachedModelIds.size()) + " model(s) available.";
    aiPanel.setAvailableModels(selectedAccount->cachedModelIds, statusText);
    aiPanel.setSelectedModel(modelName);
}

bool MainComponent::loadSuiteAiProviderSettings()
{
    creation::services::SuiteAiSettingsStore store;
    juce::String errorMessage;
    suiteAiSettings = store.load(errorMessage);

    const auto runtimeSettings = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    aiProviderSettings = makeAiProviderSettings(runtimeSettings);

    refreshAiPanelAccountsAndModels();
    return runtimeSettings.isValid();
}

void MainComponent::selectAiAccountForStation(const juce::String& accountId, const juce::String& modelNameOverride)
{
    if (accountId.isEmpty())
        return;

    creation::services::SuiteAiSettingsStore store;
    juce::String errorMessage;
    suiteAiSettings = store.load(errorMessage);

    creation::services::SuiteAiSettingsResolver::selectAccountForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station, accountId, modelNameOverride);

    if (! store.save(suiteAiSettings, errorMessage))
        transportBar.setStatusText(errorMessage);

    const auto runtimeSettings = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
        suiteAiSettings, creation::assets::SuiteAppDomain::station);
    aiProviderSettings = makeAiProviderSettings(runtimeSettings);

    refreshAiPanelAccountsAndModels();
}

void MainComponent::refreshAiAccountModelCachesAtStartup()
{
    // Reconnects every configured account once at launch to keep its cached model list current -
    // not continuous polling. Runs off the message thread since it's real (blocking) HTTP calls
    // per account; the UI already shows whatever was cached from the last run in the meantime.
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    std::thread([safeThis]() mutable
    {
        creation::services::SuiteAiSettingsStore store;
        juce::String errorMessage;
        auto refreshedSettings = store.refreshAllAccountModelCaches(errorMessage);

        juce::MessageManager::callAsync([safeThis, refreshedSettings]
        {
            if (safeThis == nullptr)
                return;

            safeThis->suiteAiSettings = refreshedSettings;

            const auto runtimeSettings = creation::services::SuiteAiSettingsResolver::resolveRuntimeSettingsForApp(
                safeThis->suiteAiSettings, creation::assets::SuiteAppDomain::station);
            safeThis->aiProviderSettings = makeAiProviderSettings(runtimeSettings);

            safeThis->refreshAiPanelAccountsAndModels();
        });
    }).detach();
}

void MainComponent::syncSemanticAppContext()
{
    if (! authenticated || appContextSyncInProgress)
        return;

    appContextSyncInProgress = true;
    auto token = authSession.getSession().token;
    auto manifest = appManifest;
    auto appName = juce::String("creation-station");
    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    std::thread([safeThis, token, manifest, appName]() mutable
    {
        LiteSemRagApiClient client;
        LiteSemRagApiClient::AppContextInfo info;
        juce::String errorMessage;
        auto published = client.syncAppContext(token, appName, manifest, info, errorMessage);

        juce::MessageManager::callAsync([safeThis, published, errorMessage, info, checksum = manifest.checksum()]
        {
            if (safeThis == nullptr)
                return;

            safeThis->appContextSyncInProgress = false;

            if (published)
            {
                safeThis->appContextLastPublishedChecksum = checksum;
                safeThis->transportBar.setStatusText("LiteSemRAG app context synced.");
                return;
            }

            // A background sync at launch that fails is not something the user can act on, so it is logged, not
            // shown in a dialog on every launch.
            if (errorMessage.isNotEmpty())
                DBG("LiteSemRAG app-context sync failed: " + errorMessage);
        });
    }).detach();
}

void MainComponent::downloadContentItem(const ContentLibrary::Item& item)
{
    if (! authenticated)
    {
        contentPanel.setStatusText("Sign in to download LagDaemon content.");
        return;
    }

    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
    {
        contentPanel.setStatusText("Choose a local storage location before downloading content.");
        return;
    }

    if (item.origin != ContentLibrary::Origin::remote || item.id.isEmpty())
    {
        contentPanel.setStatusText("That item is already local.");
        return;
    }

    auto slug = item.name.trim().toLowerCase().retainCharacters("abcdefghijklmnopqrstuvwxyz0123456789-_ ");
    slug = slug.replace(" ", "-");
    while (slug.contains("--"))
        slug = slug.replace("--", "-");
    slug = slug.trimCharactersAtStart("-");
    slug = slug.trimCharactersAtEnd("-");
    if (slug.isEmpty())
        slug = "content";

    juce::String extension;
    if (item.type == "patch")
        extension = ".cspatch";
    else if (item.type == "pack" || item.type == "sample-pack")
        extension = ".cspack";
    else if (item.type == "audio")
        extension = ".wav";
    else
        extension = ".bin";

    auto destination = creation::suite::getContentDirectory(suiteSettings).getChildFile("Downloaded")
                           .getChildFile(item.id + "__" + slug + extension);
    auto token = authSession.getSession().token;

    contentPanel.setStatusText("Downloading " + item.name + "...");
    std::thread([this, token, item, destination]()
    {
        juce::String errorMessage;
        auto success = contentApiClient.downloadContentItem(token, item.id, destination, errorMessage);

        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), success, errorMessage, item]
        {
            if (safeThis == nullptr)
                return;

            if (! success)
            {
                safeThis->contentPanel.setStatusText(errorMessage);
                return;
            }

            safeThis->contentPanel.setStatusText("Downloaded " + item.name + " from LagDaemon.");
            safeThis->refreshContentLibrary();
        });
    }).detach();
}

void MainComponent::activateContentItem(const ContentLibrary::Item& item)
{
    if (! item.file.existsAsFile())
    {
        contentPanel.setStatusText("That content item is not available on disk.");
        return;
    }

    if (item.type == "patch")
    {
        juce::String errorMessage;
        cw::PatchDocument document;
        if (! cw::parsePatchDocumentJson(item.file.loadFileAsString(), document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        if (! signalLabPanel.loadPatchDocument(document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        transportBar.setStatusText("Opened patch from library: " + item.file.getFileName());
        setWorkspaceMode(WorkspaceMode::signal);
        return;
    }

    if (item.type == "audio")
    {
        if (! projectSession.isValid())
        {
            contentPanel.setStatusText("Open or create a project before importing library audio into Foley.");
            return;
        }

        juce::String errorMessage;
        auto logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot
                         + item.file.getFileName();

        juce::MemoryBlock fileData;
        if (! item.file.loadFileAsData(fileData))
        {
            contentPanel.setStatusText("Could not read: " + item.file.getFileName());
            return;
        }

        if (! projectSession.writeEntry(logicalPath, fileData, juce::Time::getCurrentTime()))
        {
            contentPanel.setStatusText("Could not import: " + item.file.getFileName());
            return;
        }

        creation::assets::AssetDescriptor importedAsset;
        importedAsset.id = "asset:" + juce::Uuid().toString();
        importedAsset.version = "1";
        importedAsset.versionId = importedAsset.id + "@1";
        importedAsset.displayName = item.file.getFileNameWithoutExtension();
        importedAsset.logicalPath = logicalPath;
        importedAsset.kind = creation::assets::AssetKind::audio;
        importedAsset.mediaType = "audio/wav";
        importedAsset.fileSizeBytes = (int64) fileData.getSize();
        importedAsset.createdAt = importedAsset.modifiedAt = juce::Time::getCurrentTime();
        importedAsset.sourceApp = "Djehuti Station";
        projectSession.upsertAssetDescriptor(importedAsset);

        if (! projectSession.commit(errorMessage))
        {
            contentPanel.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not save imported library audio.");
            return;
        }

        refreshProjectAssets();
        placeProjectAssetOnTracker(importedAsset);
        return;
    }

    item.file.revealToUser();
    transportBar.setStatusText("Revealed content item: " + item.file.getFileName());
}

void MainComponent::openProjectAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::String errorMessage;

    if (asset.kind == creation::assets::AssetKind::patch)
    {
        juce::MemoryBlock patchData;
        if (! projectSession.readEntry(asset.logicalPath, patchData))
        {
            contentPanel.setStatusText("Could not read that project asset.");
            return;
        }

        cw::PatchDocument document;
        if (! cw::parsePatchDocumentJson(patchData.toString(), document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        pushSignalUndoState(signalLabPanel.createState(), "Load sound");

        if (! signalLabPanel.loadPatchDocument(document, errorMessage))
        {
            contentPanel.setStatusText(errorMessage);
            return;
        }

        currentSignalLabAssetId = asset.id;
        setWorkspaceMode(WorkspaceMode::signal);
        transportBar.setStatusText("Opened project sound: " + asset.displayName);
        return;
    }

    if (asset.kind == creation::assets::AssetKind::audio
        || asset.kind == creation::assets::AssetKind::render)
    {
        const auto bytes = readAssetBytes(asset.logicalPath, asset.versionId);
        if (bytes == nullptr || ! engine.previewAssetData(bytes, {}, errorMessage))
        {
            contentPanel.setStatusText(errorMessage.isNotEmpty() ? errorMessage : juce::String("Could not read that project asset."));
            return;
        }

        transportBar.setStatusText("Previewing project asset: " + asset.displayName);
        return;
    }

    if (asset.kind == creation::assets::AssetKind::trackerArrangement)
    {
        if (! restoreArrangementAsset(asset))
        {
            contentPanel.setStatusText("Could not open that arrangement: " + asset.displayName);
            return;
        }

        setWorkspaceMode(WorkspaceMode::tracker);
        transportBar.setStatusText("Opened arrangement: " + asset.displayName);
        return;
    }

    if (asset.kind == creation::assets::AssetKind::foleyPatch)
    {
        if (! restoreFoleyAsset(asset))
        {
            contentPanel.setStatusText("Could not open that Foley setup: " + asset.displayName);
            return;
        }

        setWorkspaceMode(WorkspaceMode::foley);
        transportBar.setStatusText("Opened Foley setup: " + asset.displayName);
        return;
    }

    transportBar.setStatusText("There is nothing to open for " + asset.displayName + " here.");
}

bool MainComponent::restoreArrangementAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::MemoryBlock data;
    if (! projectSession.readEntry(asset.logicalPath, data))
        return false;

    auto xmlString = juce::String::createStringFromData(data.getData(), (int) data.getSize());
    auto state = juce::ValueTree::fromXml(xmlString);
    if (! state.isValid())
        return false;

    timelineModel.restoreState(state);
    currentArrangementAssetId = asset.id;
    trackerPanel.setCurrentArrangementName(asset.displayName);
    trackerPanel.refreshTimelineView();
    markArrangementClean();
    return true;
}

juce::String MainComponent::arrangementFingerprint() const
{
    // The playhead position and zoom are not edits, so they are left out of the comparison.
    auto state = timelineModel.createState();
    state.removeProperty("transportSeconds", nullptr);
    state.removeProperty("pixelsPerSecond", nullptr);
    if (auto xml = state.createXml())
        return xml->toString(juce::XmlElement::TextFormat().singleLine());
    return {};
}

bool MainComponent::arrangementIsDirty() const
{
    if (timelineModel.getTrackCount() == 0 && timelineModel.getClips().empty())
        return false;

    return arrangementFingerprint() != arrangementBaseline;
}

void MainComponent::markArrangementClean()
{
    arrangementBaseline = arrangementFingerprint();
    refreshArrangementTitle();
}

void MainComponent::refreshArrangementTitle()
{
    if (dockManager == nullptr || ! dockManager->isRegistered(trackerPanelId))
        return;

    const auto name = trackerPanel.getCurrentArrangementName();
    const auto title = "Tracker - " + (name.isNotEmpty() ? name : juce::String("Untitled")) + (arrangementIsDirty() ? " *" : "");
    if (title == shownArrangementTitle)
        return;

    shownArrangementTitle = title;
    dockManager->setPanelTitle(trackerPanelId, title);
}

void MainComponent::newArrangement()
{
    if (! arrangementIsDirty())
    {
        performNewArrangement();
        return;
    }

    const auto currentName = trackerPanel.getCurrentArrangementName();
    auto* prompt = new juce::AlertWindow("Start a new arrangement",
                                         "This arrangement has unsaved changes. Save it before starting a new one?",
                                         juce::MessageBoxIconType::QuestionIcon);
    prompt->addTextEditor("name", currentName.isNotEmpty() ? currentName : juce::String("Arrangement"), "Save as:");
    prompt->addButton("Save", 1);
    prompt->addButton("Don't Save", 2);
    prompt->addButton("Cancel", 0);

    prompt->enterModalState(true, juce::ModalCallbackFunction::create([safe = juce::Component::SafePointer<MainComponent>(this), prompt](int result)
    {
        std::unique_ptr<juce::AlertWindow> dialog(prompt);
        if (safe == nullptr || result == 0)
            return;

        if (result == 1)
        {
            const auto name = dialog->getTextEditorContents("name").trim();
            if (name.isEmpty() || ! safe->trackerPanel.onArrangementSaveRequested)
                return;

            safe->trackerPanel.onArrangementSaveRequested(name);

            // If the save did not go through (for example no project is open) the arrangement is still unsaved,
            // so it is kept rather than thrown away.
            if (safe->arrangementIsDirty())
                return;
        }

        safe->performNewArrangement();
    }), true);
}

void MainComponent::performNewArrangement()
{
    // Stop whatever is playing, then remove every track the same way removing one track does, so Ctrl+Z brings the
    // old arrangement back.
    if (transportBar.onStop)
        transportBar.onStop();

    juce::ValueTree undoSnapshot("UndoSnapshot");
    undoSnapshot.addChild(timelineModel.createState(), -1, nullptr);
    undoSnapshot.addChild(engine.createSessionState(), -1, nullptr);

    for (int trackIndex = engine.getTrackCount() - 1; trackIndex >= 0; --trackIndex)
        engine.removeTrack(trackIndex);

    pushTimelineUndoState(undoSnapshot);

    timelineModel.clear();
    armedTracks.clear();
    monitoredTracks.clear();
    currentArrangementAssetId = {};
    trackerPanel.setCurrentArrangementName({});
    selectedClipIndex = -1;

    syncTrackViews();
    trackerPanel.setSelectedClip(-1);
    trackerPanel.setSelectedTrack(-1);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();

    pluginRackBar.setContextMaster();
    mixerPanel.setSelectedChannel(-1);
    mixerPanel.setBankOffset(0);
    midiSurface.setBankOffset(0);
    midiSurface.refreshVisibleWindow();
    refreshInsertRack();

    projectDirty = true;
    saveSessionToDisk();
    markArrangementClean();
    transportBar.setStatusText("New arrangement.");
}


bool MainComponent::restoreSignalLabAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::String errorMessage;
    juce::MemoryBlock patchData;
    if (! projectSession.readEntry(asset.logicalPath, patchData))
        return false;

    cw::PatchDocument document;
    if (! cw::parsePatchDocumentJson(patchData.toString(), document, errorMessage))
        return false;

    if (! signalLabPanel.loadPatchDocument(document, errorMessage))
        return false;

    currentSignalLabAssetId = asset.id;
    return true;
}

bool MainComponent::restoreFoleyAsset(const creation::assets::AssetDescriptor& asset)
{
    juce::MemoryBlock data;
    if (! projectSession.readEntry(asset.logicalPath, data))
        return false;

    auto frgraphText = juce::String::createStringFromData(data.getData(), (int) data.getSize());
    juce::String errorMessage;
    if (! foleyPanel.loadGraph(frgraphText, errorMessage))
        return false;

    currentFoleyAssetId = asset.id;
    return true;
}

void MainComponent::restoreLastActiveAssets(const juce::ValueTree& lastActiveAssetsState)
{
    if (! lastActiveAssetsState.isValid())
        return;

    const auto& catalog = projectSession.getManifest().assetCatalog;

    if (auto id = lastActiveAssetsState.getProperty("trackerAssetId").toString(); id.isNotEmpty())
        if (auto* asset = catalog.findById(id))
            restoreArrangementAsset(*asset);

    if (auto id = lastActiveAssetsState.getProperty("signalLabAssetId").toString(); id.isNotEmpty())
        if (auto* asset = catalog.findById(id))
            restoreSignalLabAsset(*asset);

    if (auto id = lastActiveAssetsState.getProperty("foleyAssetId").toString(); id.isNotEmpty())
        if (auto* asset = catalog.findById(id))
            restoreFoleyAsset(*asset);
}

void MainComponent::placeProjectAssetOnTracker(const creation::assets::AssetDescriptor& asset, double startSeconds)
{
    if (asset.kind != creation::assets::AssetKind::audio
        && asset.kind != creation::assets::AssetKind::render
        && asset.kind != creation::assets::AssetKind::patch
        && asset.kind != creation::assets::AssetKind::video)
    {
        contentPanel.setStatusText("Only audio, video, and signal patches can be placed on the Tracker right now.");
        return;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    auto targetTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
        targetTrack = 0;

    if (asset.kind == creation::assets::AssetKind::patch)
    {
        if (timelineModel.getTrackKind(targetTrack) != cs::TrackKind::signal)
        {
            addTrack();
            targetTrack = engine.getTrackCount() - 1;
            timelineModel.setTrackKind(targetTrack, cs::TrackKind::signal);
            trackerPanel.setTrackKind(targetTrack, cs::TrackKind::signal);
            engine.setTrackIsMidiKind(targetTrack, false);
            engine.setTrackIsAutomationKind(targetTrack, false);
        }
    }

    juce::String errorMessage;
    int clipIndex = -1;

    if (asset.kind == creation::assets::AssetKind::video)
    {
        if (timelineModel.getTrackKind(targetTrack) != cs::TrackKind::video)
        {
            addTrack();
            targetTrack = engine.getTrackCount() - 1;
            timelineModel.setTrackKind(targetTrack, cs::TrackKind::video);
            trackerPanel.setTrackKind(targetTrack, cs::TrackKind::video);
            engine.setTrackIsMidiKind(targetTrack, false);
            engine.setTrackIsAutomationKind(targetTrack, false);
        }

        // The video stays in the project: it is opened as a stream when it plays, so nothing is copied out.
        double videoSeconds = asset.details["durationSeconds"].getDoubleValue();
        if (videoSeconds <= 0.0)
        {
            const auto source = makeVideoSource(asset.id, asset.logicalPath);
            auto stream = source.makeStream();
            cs::VideoDecodeService probe;
            const auto info = stream != nullptr ? probe.open(std::move(stream), source.nameHint) : cs::VideoStreamInfo {};
            videoSeconds = info.valid ? info.durationSeconds : 0.0;
        }

        cs::AssetRef videoRef;
        videoRef.id = asset.id;
        videoRef.versionId = asset.versionId;
        videoRef.mode = creation::assets::AssetReferenceMode::exact;

        const auto videoClipIndex = timelineModel.addClipFromData(cs::ClipKind::video, targetTrack, asset.displayName, asset.id, "project-video",
                                                                  nullptr, startSeconds, videoSeconds > 0.0 ? videoSeconds : 10.0, errorMessage);
        if (videoClipIndex < 0)
        {
            reportError("Could not add " + asset.displayName + " to the Tracker" + (errorMessage.isNotEmpty() ? ": " + errorMessage : juce::String()));
            return;
        }

        timelineModel.setClipAssetReference(videoClipIndex, videoRef);
        trackerPanel.setSelectedTrack(targetTrack);
        trackerPanel.refreshTimelineView();
        setWorkspaceMode(WorkspaceMode::tracker);
        saveSessionToDisk();
        showToast("Added " + asset.displayName + " to the Tracker.");
        return;
    }

    if (asset.kind == creation::assets::AssetKind::patch)
    {
        juce::MemoryBlock patchData;
        if (! projectSession.readEntry(asset.logicalPath, patchData))
        {
            contentPanel.setStatusText("Could not read the patch asset from the project.");
            return;
        }

        cw::PatchDocument doc;
        if (! cw::parsePatchDocumentJson(patchData.toString(), doc, errorMessage))
        {
            contentPanel.setStatusText("Could not parse patch asset: " + errorMessage);
            return;
        }

        cs::AssetRef assetRef;
        assetRef.id = asset.id;
        assetRef.versionId = asset.versionId;
        assetRef.mode = creation::assets::AssetReferenceMode::exact;

        clipIndex = timelineModel.addClip(cs::ClipKind::signal,
                                          targetTrack,
                                          asset.displayName,
                                          asset.id,
                                          "signal-lab",
                                          juce::File(),
                                          timelineModel.getTransportSeconds(),
                                          doc.durationSeconds > 0.0 ? doc.durationSeconds : 5.0,
                                          errorMessage);

        if (clipIndex >= 0)
        {
            timelineModel.setClipAssetReference(clipIndex, assetRef);
        }
    }
    else
    {
        auto sourceTool = asset.kind == creation::assets::AssetKind::render ? "render" : "project-audio";
        clipIndex = placeAudioAssetOnTracker(asset,
                                             targetTrack,
                                             timelineModel.getTransportSeconds(),
                                             sourceTool,
                                             errorMessage);
    }

    if (clipIndex < 0)
    {
        contentPanel.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not place that asset on the Tracker.");
        return;
    }

    trackerPanel.setSelectedTrack(targetTrack);
    trackerPanel.refreshTimelineView();
    setWorkspaceMode(WorkspaceMode::tracker);
    saveSessionToDisk();
    transportBar.setStatusText("Placed project asset on Tracker: " + asset.displayName);
}

int MainComponent::placeAudioAssetOnTracker(const creation::assets::AssetDescriptor& asset,
                                            int targetTrack,
                                            double startSeconds,
                                            const juce::String& sourceTool,
                                            juce::String& errorMessage)
{
    // The sound is read into memory through the VFS service; nothing is copied out to a file.
    const auto bytes = readAssetBytes(asset.logicalPath, asset.versionId);
    if (bytes == nullptr)
    {
        errorMessage = "the sound could not be read from the project";
        return -1;
    }

    cs::AssetRef assetRef;
    assetRef.id = asset.id;
    assetRef.versionId = asset.versionId;
    assetRef.mode = creation::assets::AssetReferenceMode::exact;

    auto clipIndex = timelineModel.addClipFromData(cs::ClipKind::audio,
                                                   targetTrack,
                                                   asset.displayName,
                                                   asset.id,
                                                   sourceTool,
                                                   bytes.get(),
                                                   startSeconds,
                                                   0.0,
                                                   errorMessage);

    if (clipIndex >= 0)
        timelineModel.setClipAssetReference(clipIndex, assetRef);

    if (clipIndex >= 0 && juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
    {
        engine.setTrackStereoEnabled(targetTrack, true);
        timelineModel.setTrackChannelMode(targetTrack, cs::TrackChannelMode::stereo);
        trackerPanel.setTrackStereo(targetTrack, true);
    }

    return clipIndex;
}

bool MainComponent::importAudioFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds)
{
    if (filePaths.isEmpty())
        return false;

    if (! ensureStorageRootConfigured())
        return false;

    juce::String projectError;
    if (! ensureProjectSessionActive(projectError))
    {
        transportBar.setStatusText(projectError.isNotEmpty() ? projectError : "Could not initialize project for imported audio.");
        return false;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    auto targetTrack = preferredTrack;
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
        targetTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()))
    {
        addTrack();
        targetTrack = engine.getTrackCount() - 1;
    }

    auto placedCount = 0;
    auto nextStartSeconds = juce::jmax(0.0, startSeconds);
    juce::String lastError;
    auto insertionTrack = targetTrack;

    for (const auto& filePath : filePaths)
    {
        auto sourceFile = juce::File(filePath);
        if (! sourceFile.existsAsFile())
            continue;

        if (! juce::isPositiveAndBelow(insertionTrack, engine.getTrackCount()))
        {
            addTrack();
            insertionTrack = engine.getTrackCount() - 1;
        }

        // Import the external audio file into the VFS container
        juce::String importError;
        auto logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot
                         + sourceFile.getFileName();

        juce::MemoryBlock fileData;
        if (! sourceFile.loadFileAsData(fileData))
        {
            lastError = "Could not read: " + sourceFile.getFileName();
            continue;
        }

        if (! projectSession.writeEntry(logicalPath, fileData, juce::Time::getCurrentTime()))
        {
            lastError = "Could not import: " + sourceFile.getFileName();
            continue;
        }

        creation::assets::AssetDescriptor importedAsset;
        importedAsset.id = "asset:" + juce::Uuid().toString();
        importedAsset.version = "1";
        importedAsset.versionId = importedAsset.id + "@1";
        importedAsset.displayName = sourceFile.getFileNameWithoutExtension();
        importedAsset.logicalPath = logicalPath;
        importedAsset.kind = creation::assets::AssetKind::audio;
        importedAsset.mediaType = "audio/wav";
        importedAsset.fileSizeBytes = (int64) fileData.getSize();
        importedAsset.createdAt = importedAsset.modifiedAt = juce::Time::getCurrentTime();
        importedAsset.sourceApp = "Djehuti Station";
        projectSession.upsertAssetDescriptor(importedAsset);

        if (! projectSession.commit(importError))
        {
            lastError = importError;
            continue;
        }

        juce::String clipError;
        auto clipIndex = placeAudioAssetOnTracker(importedAsset, insertionTrack, nextStartSeconds, "import", clipError);
        if (clipIndex < 0)
        {
            lastError = clipError;
            continue;
        }

        const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
        ++placedCount;

        if (filePaths.size() > 1)
        {
            nextStartSeconds = juce::jmax(0.0, startSeconds);
            ++insertionTrack;
        }
        else
        {
            nextStartSeconds = clip.startSeconds + clip.durationSeconds;
        }
    }

    if (placedCount <= 0)
    {
        if (lastError.isNotEmpty())
            transportBar.setStatusText(lastError);
        return false;
    }

    refreshProjectAssets();
    trackerPanel.setSelectedTrack(targetTrack);
    trackerPanel.refreshTimelineView();
    setWorkspaceMode(WorkspaceMode::tracker);
    saveSessionToDisk(true);
    transportBar.setStatusText("Imported " + juce::String(placedCount) + " audio file(s) onto the Tracker.");
    return true;
}

int MainComponent::addImportedVideoToTracker(const juce::File& sourceFile, const juce::String& assetId, const juce::String& logicalPath, juce::int64 fileSize,
                                               const cs::VideoStreamInfo& info, const juce::MemoryBlock& thumbnailJpeg, int targetTrack, double startSeconds,
                                               juce::String& errorMessage)
{
    creation::assets::AssetDescriptor importedAsset;
    importedAsset.id = assetId;
    importedAsset.version = "1";
    importedAsset.versionId = importedAsset.id + "@1";
    importedAsset.displayName = sourceFile.getFileNameWithoutExtension();
    importedAsset.logicalPath = logicalPath;
    importedAsset.kind = creation::assets::AssetKind::video;
    importedAsset.mediaType = "video/" + sourceFile.getFileExtension().trimCharactersAtStart(".").toLowerCase();
    importedAsset.fileSizeBytes = (int64) fileSize;
    importedAsset.createdAt = importedAsset.modifiedAt = juce::Time::getCurrentTime();
    importedAsset.sourceApp = "Djehuti Station";

    // What the video is, recorded now so the Add Clip picker can show it.
    importedAsset.details.set("durationSeconds", juce::String(info.durationSeconds, 3));
    importedAsset.details.set("width", juce::String(info.width));
    importedAsset.details.set("height", juce::String(info.height));
    importedAsset.details.set("frameRate", juce::String(info.frameRate, 3));
    importedAsset.details.set("hasAudio", info.hasAudio ? "1" : "0");
    if (info.hasAudio)
    {
        importedAsset.details.set("channels", juce::String(info.audioNumChannels));
        importedAsset.details.set("sampleRate", juce::String(info.audioSampleRate, 0));
    }
    if (thumbnailJpeg.getSize() > 0)
    {
        const auto thumbnailPath = "Assets/Thumbnails/" + assetId.replaceCharacters(":\\/ ", "____") + ".jpg";
        if (projectSession.writeEntry(thumbnailPath, thumbnailJpeg, juce::Time::getCurrentTime()))
            importedAsset.details.set("thumbnail", thumbnailPath);
    }

    projectSession.upsertAssetDescriptor(importedAsset);

    if (! projectSession.commit(errorMessage))
        return -1;

    // A track of -1 means the video only goes into the project's asset library (File > Import), no clip.
    if (targetTrack < 0)
        return 0;

    // The clip plays from the file that was just imported, so a big video is not copied back out of the
    // project the moment it goes in. durationSeconds comes from the probed VideoStreamInfo -- addClip()
    // deliberately never runs waveform/duration analysis for ClipKind::video (it can't open a video
    // container as audio), so this is the only source of truth for how long the clip is.
    return timelineModel.addClipFromData(cs::ClipKind::video,
                                         targetTrack,
                                         importedAsset.displayName,
                                         importedAsset.id,
                                         "import",
                                         nullptr,
                                         startSeconds,
                                         info.durationSeconds,
                                         errorMessage);
}

void MainComponent::importVideoFilesToTracker(const juce::StringArray& filePaths, int preferredTrack, double startSeconds)
{
    if (filePaths.isEmpty())
        return;

    if (! ensureStorageRootConfigured())
        return;

    juce::String projectError;
    if (! ensureProjectSessionActive(projectError))
    {
        transportBar.setStatusText(projectError.isNotEmpty() ? projectError : "Could not initialize project for imported video.");
        return;
    }

    // Video clips require a video-kind track (see canTrackContainClip) -- reuse the preferred/
    // selected track only if it's already video-kind, otherwise add a fresh track and switch it,
    // the same "make a sensible home for what was dropped" behavior importAudioFilesToTracker
    // already has for audio.
    auto targetTrack = preferredTrack;
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()) || timelineModel.getTrackKind(targetTrack) != cs::TrackKind::video)
        targetTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(targetTrack, engine.getTrackCount()) || timelineModel.getTrackKind(targetTrack) != cs::TrackKind::video)
    {
        addTrack();
        targetTrack = engine.getTrackCount() - 1;
        timelineModel.setTrackKind(targetTrack, cs::TrackKind::video);
        trackerPanel.setTrackKind(targetTrack, cs::TrackKind::video);
        engine.setTrackIsMidiKind(targetTrack, false);
        engine.setTrackIsAutomationKind(targetTrack, false);
    }

    runVideoImport(filePaths, targetTrack, juce::jmax(0.0, startSeconds));
}

namespace
{
struct VideoImportItem
{
    juce::File file;
    cs::VideoStreamInfo info;
    juce::String logicalPath;
    juce::String assetId;
    juce::MemoryBlock thumbnailJpeg; // a small picture from near the start of the video, for the Add Clip picker
    juce::String error; // why this file could not be imported ("" when it went in)
    bool uploaded = false;
    std::shared_ptr<juce::MemoryBlock> audioBytes; // the video's own sound as a WAV, in memory
    bool hasAudio = false;
};

// Decodes a video's own sound track to a 16-bit WAV, in memory. hasAudio comes back false (and the call still succeeds)
// for a video with no sound. Nothing is written to the disk.
bool renderVideoAudioWav(const cs::VideoSource& source, juce::MemoryBlock& wavData, bool& hasAudio, juce::String& error)
{
    hasAudio = false;
    cs::VideoDecodeService decoder;
    cs::VideoStreamInfo info;
    if (source.makeStream)
    {
        auto stream = source.makeStream();
        info = stream != nullptr ? decoder.open(std::move(stream), source.nameHint) : cs::VideoStreamInfo {};
    }
    else
    {
        info = decoder.open(source.file);
    }

    if (! info.valid)
    {
        error = "the video could not be opened to read its sound" + (decoder.getLastError().isNotEmpty() ? ": " + decoder.getLastError() : juce::String());
        return false;
    }

    if (! info.hasAudio)
        return true;

    juce::AudioBuffer<float> pcm;
    if (! decoder.decodeAudioToFloatPCM(pcm) || pcm.getNumSamples() == 0)
    {
        error = "the video's sound could not be decoded";
        return false;
    }

    wavData.reset();
    auto* wavStream = new juce::MemoryOutputStream(wavData, false);

    // The writer takes ownership of (and deletes) the stream.
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatWriter> writer(
        format.createWriterFor(wavStream, info.audioSampleRate > 0.0 ? info.audioSampleRate : 48000.0, (unsigned int) pcm.getNumChannels(), 16, {}, 0));
    if (writer == nullptr)
    {
        delete wavStream;
        error = "the sound could not be encoded";
        return false;
    }

    writer->writeFromAudioSampleBuffer(pcm, 0, pcm.getNumSamples());
    writer.reset(); // flushes the WAV header into wavData
    hasAudio = true;
    return true;
}
}

std::shared_ptr<const juce::MemoryBlock> MainComponent::readAssetBytes(const juce::String& logicalPath, const juce::String& versionKey)
{
    const auto key = projectSession.getManifest().projectId + "|" + logicalPath + "|" + versionKey;
    if (const auto found = assetBytesCache.find(key); found != assetBytesCache.end())
        return found->second;

    auto block = std::make_shared<juce::MemoryBlock>();
    if (! projectSession.readEntry(logicalPath, *block) || block->getSize() == 0)
        return nullptr;

    // Keep what was read for reuse, but never more than about 1.5 GB of it.
    constexpr juce::int64 kMaxCachedBytes = 1500LL * 1024 * 1024;
    if (assetBytesCacheSize + (juce::int64) block->getSize() > kMaxCachedBytes)
    {
        assetBytesCache.clear();
        assetBytesCacheSize = 0;
    }

    assetBytesCacheSize += (juce::int64) block->getSize();
    assetBytesCache[key] = block;
    return block;
}

cs::VideoSource MainComponent::makeVideoSource(const juce::String& assetId, const juce::String& logicalPath) const
{
    cs::VideoSource source;
    source.key = assetId;
    source.nameHint = juce::File(logicalPath).getFileName();
    const auto projectId = projectSession.getManifest().projectId;
    source.makeStream = [projectId, logicalPath] { return creation::assets::openVfsEntryStream(projectId, logicalPath); };
    return source;
}

juce::String MainComponent::videoAudioCachePath(const juce::String& assetId)
{
    return "cache/video_audio_" + assetId.replaceCharacters(":\\/ ", "____") + ".wav";
}

bool MainComponent::videoClipsNeedAudio() const
{
    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.kind != cs::ClipKind::video || clip.assetId.isEmpty() || videosWithoutAudio.count(clip.assetId) > 0)
            continue;

        const auto found = videoAudioBytes.find(clip.assetId);
        if (found == videoAudioBytes.end() || found->second == nullptr)
            return true;
    }
    return false;
}

bool MainComponent::prepareVideoAudio(std::function<void()> whenDone)
{
    if (progressTask != nullptr || ! projectSession.isValid())
        return false;

    struct Job
    {
        juce::String assetId;
        cs::VideoSource source;
        std::shared_ptr<juce::MemoryBlock> audio;
        bool hasAudio = false;
        juce::String error;
    };

    auto jobs = std::make_shared<std::vector<Job>>();
    std::set<juce::String> seen;
    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.kind != cs::ClipKind::video || clip.assetId.isEmpty() || videosWithoutAudio.count(clip.assetId) > 0 || ! seen.insert(clip.assetId).second)
            continue;

        if (const auto found = videoAudioBytes.find(clip.assetId); found != videoAudioBytes.end() && found->second != nullptr)
            continue;

        Job job;
        job.assetId = clip.assetId;
        if (const auto asset = resolveTimelineClipAsset(clip); asset.has_value())
            job.source = makeVideoSource(clip.assetId, asset->logicalPath);
        jobs->push_back(std::move(job));
    }

    if (jobs->empty())
        return false;

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    auto work = [safeThis, jobs](ProgressTask& task)
    {
        const auto count = (int) jobs->size();
        for (int i = 0; i < count && ! task.cancelRequested() && safeThis != nullptr; ++i)
        {
            auto& job = (*jobs)[(size_t) i];
            const auto base = (double) i / (double) count;
            const auto share = 1.0 / (double) count;
            auto& session = safeThis->projectSession;
            const auto cachePath = videoAudioCachePath(job.assetId);

            // 1. Already extracted before and saved in the project?
            task.report(base, "Looking for the video's saved sound...");
            {
                juce::MemoryBlock saved;
                if (session.readEntry(cachePath, saved) && saved.getSize() > 0)
                {
                    job.audio = std::make_shared<juce::MemoryBlock>(std::move(saved));
                    job.hasAudio = true;
                    continue;
                }
            }

            // 2. Otherwise read it out of the video, which is streamed from the project (never copied out to a file).
            if (! job.source.isValid())
            {
                job.error = "the video is not in the project any more";
                continue;
            }

            task.report(base + share * 0.1, "Reading the video's sound...");
            auto wav = std::make_shared<juce::MemoryBlock>();
            if (! renderVideoAudioWav(job.source, *wav, job.hasAudio, job.error) || ! job.hasAudio)
                continue;

            job.audio = wav;

            // 3. Keep it in the project, so it does not have to be read out of the video again.
            task.report(base + share * 0.8, "Saving the video's sound into the project...");
            session.writeEntry(cachePath, *wav, juce::Time::getCurrentTime());
        }
    };

    auto finished = [safeThis, jobs, whenDone](bool cancelled)
    {
        if (safeThis == nullptr)
            return;

        auto* self = safeThis.getComponent();
        juce::StringArray problems;
        for (auto& job : *jobs)
        {
            if (job.error.isNotEmpty())
                problems.add("Could not read the sound of a video: " + job.error + ".");
            else if (job.hasAudio && job.audio != nullptr)
                self->videoAudioBytes[job.assetId] = job.audio;
            else if (! cancelled && ! job.hasAudio)
                self->videosWithoutAudio.insert(job.assetId);
        }

        if (! problems.isEmpty())
            self->reportError(problems.joinIntoString("\n\n"));
        else if (! cancelled && whenDone)
            whenDone();

        self->refreshTrackerPlaybackClips();

        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->progressTask.reset();
        });
    };

    progressTask = std::make_unique<ProgressTask>("Preparing the video's sound", std::move(work), std::move(finished));
    progressTask->start();
    return true;
}

void MainComponent::showAddClipPicker(int trackIndex, double startSeconds)
{
    using Kind = creation::assets::AssetKind;
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()) || ! projectSession.isValid())
        return;

    // What this kind of track can hold.
    const auto trackKind = timelineModel.getTrackKind(trackIndex);
    juce::Array<Kind> fits;
    juce::String description;
    switch (trackKind)
    {
        case cs::TrackKind::video: fits = { Kind::video }; description = "video track"; break;
        case cs::TrackKind::signal: fits = { Kind::patch, Kind::audio, Kind::render }; description = "Signal Lab track"; break;
        case cs::TrackKind::audio: fits = { Kind::audio, Kind::render }; description = "audio track"; break;
        case cs::TrackKind::foley: fits = { Kind::audio, Kind::render }; description = "Foley track"; break;
        default: break;
    }

    if (fits.isEmpty())
    {
        showToast("This kind of track cannot hold audio, video or patches.");
        return;
    }

    auto open = [this, trackIndex, startSeconds, fits, description]
    {
        juce::Array<PickerRow> fitting, others;
        for (const auto& asset : projectSession.getManifest().assetCatalog.assets)
        {
            if (asset.kind != Kind::audio && asset.kind != Kind::render && asset.kind != Kind::video && asset.kind != Kind::patch)
                continue;

            PickerRow row;
            row.asset = asset;
            row.facts = describeAsset(asset);

            if (const auto thumbnailPath = asset.details["thumbnail"]; thumbnailPath.isNotEmpty())
            {
                juce::MemoryBlock jpeg;
                if (projectSession.readEntry(thumbnailPath, jpeg))
                    row.thumbnail = juce::ImageFileFormat::loadFrom(jpeg.getData(), jpeg.getSize());
            }

            (fits.contains(asset.kind) ? fitting : others).add(std::move(row));
        }

        new AssetPickerDialog(description, std::move(fitting), std::move(others),
                              [this, trackIndex, startSeconds](const creation::assets::AssetDescriptor& chosen)
                              {
                                  trackerPanel.setSelectedTrack(trackIndex);
                                  placeProjectAssetOnTracker(chosen, startSeconds);
                              });
    };

    open();
}

void MainComponent::handleClipSoundAction(int clipIndex, int action)
{
    const auto& clips = timelineModel.getClips();
    if (! juce::isPositiveAndBelow(clipIndex, (int) clips.size()))
        return;

    if (action == 1)
    {
        splitSoundFromVideo(clipIndex);
        return;
    }

    if (action == 5)
    {
        showVideoClipSettings(clipIndex);
        return;
    }

    auto stateBeforeEdit = timelineModel.createState();
    juce::String done;

    if (action == 2)
    {
        timelineModel.unlinkClip(clipIndex);
        done = "Unlinked. The picture and its sound now move separately.";
    }
    else if (action == 3)
    {
        const auto other = timelineModel.findSoundCounterpart(clipIndex);
        if (other < 0 || ! timelineModel.linkClips(clipIndex, other))
        {
            showToast("There is no picture or sound here to link.");
            return;
        }
        done = "Linked. The picture and its sound move together again.";
    }
    else if (action == 4)
    {
        // The video is either the clicked clip or the one the clicked sound belongs to.
        auto videoIndex = clips[(size_t) clipIndex].kind == cs::ClipKind::video ? clipIndex : -1;
        auto soundIndex = -1;
        if (videoIndex >= 0)
        {
            for (auto partner : timelineModel.getLinkedPartnerIndices(videoIndex))
                if (clips[(size_t) partner].sourceTool == cs::TimelineModel::videoSoundSourceTool(clips[(size_t) videoIndex].assetId))
                    soundIndex = partner;
            if (soundIndex < 0)
                soundIndex = timelineModel.findSoundCounterpart(videoIndex);
        }

        if (videoIndex < 0 || soundIndex < 0)
        {
            showToast("Could not find this video's sound clip to put back.");
            return;
        }

        const auto videoId = clips[(size_t) videoIndex].id;
        timelineModel.unlinkClip(videoIndex);
        timelineModel.deleteClip(soundIndex);
        for (size_t i = 0; i < timelineModel.getClips().size(); ++i)
            if (timelineModel.getClips()[i].id == videoId)
                timelineModel.setClipSoundDetached((int) i, false);
        done = "The sound is back inside the video.";
    }
    else
    {
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    showToast(done);
}

void MainComponent::splitSoundFromVideo(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()))
        return;

    const auto clip = timelineModel.getClips()[(size_t) clipIndex];
    if (clip.kind != cs::ClipKind::video || clip.soundDetached || clip.assetId.isEmpty())
    {
        showToast("The sound of this clip is already split off.");
        return;
    }

    if (videosWithoutAudio.count(clip.assetId) > 0)
    {
        showToast("This video has no sound to split off.");
        return;
    }

    // The video's sound has to be extracted first (a progress window); the split carries on once it is.
    const auto found = videoAudioBytes.find(clip.assetId);
    if (found == videoAudioBytes.end() || found->second == nullptr)
    {
        const auto clipId = clip.id;
        const auto started = prepareVideoAudio([this, clipId]
        {
            for (size_t i = 0; i < timelineModel.getClips().size(); ++i)
                if (timelineModel.getClips()[i].id == clipId)
                {
                    splitSoundFromVideo((int) i);
                    return;
                }
        });

        if (! started)
            reportError("Could not split the sound: another long action is still running. Try again when it has finished.");
        return;
    }

    const auto wavBytes = found->second;

    // Keep the sound in the project's asset list, so it survives closing and reopening the project.
    creation::assets::AssetDescriptor soundAsset;
    const auto soundPath = videoAudioCachePath(clip.assetId);
    for (const auto& existing : projectSession.getManifest().assetCatalog.assets)
        if (existing.logicalPath == soundPath && existing.kind == creation::assets::AssetKind::audio)
            soundAsset = existing;

    if (soundAsset.id.isEmpty())
    {
        soundAsset.id = "asset:" + juce::Uuid().toString();
        soundAsset.version = "1";
        soundAsset.versionId = soundAsset.id + "@1";
        soundAsset.displayName = clip.displayName + " sound";
        soundAsset.logicalPath = soundPath;
        soundAsset.kind = creation::assets::AssetKind::audio;
        soundAsset.mediaType = "audio/wav";
        soundAsset.fileSizeBytes = (int64) wavBytes->getSize();
        soundAsset.createdAt = soundAsset.modifiedAt = juce::Time::getCurrentTime();
        soundAsset.sourceApp = "Djehuti Station";
        projectSession.upsertAssetDescriptor(soundAsset);

        juce::String commitError;
        if (! projectSession.commit(commitError))
        {
            reportError("Could not split the sound: " + commitError);
            return;
        }
    }

    // A new audio track directly under the video's track.
    const auto videoTrackIndex = clip.trackIndex;
    addTrack();
    const auto newTrackIndex = engine.getTrackCount() - 1;
    if (newTrackIndex < 0)
    {
        reportError("Could not split the sound: a new track could not be added.");
        return;
    }

    auto soundTrackIndex = newTrackIndex;
    if (newTrackIndex != videoTrackIndex + 1)
    {
        if (performTrackMove(newTrackIndex, videoTrackIndex + 1))
            soundTrackIndex = videoTrackIndex + 1;
        else
            showToast("The sound is on a new track at the bottom (it could not be placed under the video).");
    }
    engine.setTrackName(soundTrackIndex, clip.displayName + " sound");
    syncTrackViews();

    // Everything from here is one undoable step (adding the empty track was its own).
    auto stateBeforeEdit = timelineModel.createState();

    // Find the video clip again: the track move can renumber tracks.
    auto videoIndex = -1;
    for (size_t i = 0; i < timelineModel.getClips().size(); ++i)
        if (timelineModel.getClips()[i].id == clip.id)
            videoIndex = (int) i;
    if (videoIndex < 0)
        return;

    juce::String clipError;
    const auto soundIndex = timelineModel.addClipFromData(cs::ClipKind::audio, soundTrackIndex, clip.displayName + " sound", soundAsset.id,
                                                          cs::TimelineModel::videoSoundSourceTool(clip.assetId), wavBytes.get(),
                                                          clip.startSeconds, clip.durationSeconds, clipError);
    if (soundIndex < 0)
    {
        reportError("Could not split the sound: " + (clipError.isNotEmpty() ? clipError : juce::String("the sound clip could not be added.")));
        return;
    }

    // The sound shows the same stretch of the video that the picture does.
    timelineModel.setClipSourceRange(soundIndex, clip.sourceStartSeconds, timelineModel.getClips()[(size_t) soundIndex].sourceDurationSeconds);
    timelineModel.setClipDuration(soundIndex, clip.durationSeconds);
    timelineModel.setClipSoundDetached(videoIndex, true);
    timelineModel.linkClips(videoIndex, soundIndex);

    pushTimelineUndoState(stateBeforeEdit);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    showToast("Sound split onto its own track, linked to the picture.");
}

void MainComponent::openVideoViewForPlayback()
{
    if (dockManager == nullptr)
        return;

    auto hasVideo = false;
    for (const auto& clip : timelineModel.getClips())
        if (clip.kind == cs::ClipKind::video)
            hasVideo = true;

    if (! hasVideo)
        return;

    // Already open (docked or floating): leave it exactly where the user put it - but if it is open and hidden
    // (behind another tab), bring it forward so playing actually shows the picture.
    if (dockManager->isRegistered(videoPanelId))
    {
        if (! videoView.isShowing())
            dockManager->activatePanel(videoPanelId);
        return;
    }

    // Not open: show the picture in its own window, which can be docked from there.
    if (auto* panel = registerNamedDockPanel(videoPanelId, CreationDock::DockTargetZone::Right))
        dockManager->floatPanel(panel);

    menuItemsChanged();
}

void MainComponent::updateVideoView(double timelineSeconds)
{
    // Nothing is decoded while the view is not on screen.
    if (! videoView.isShowing())
    {
        videoPanelHost.setExtraInfo("view not on screen: not decoding");
        return;
    }

    // Every video clip under the playhead is a layer. A clip on a track higher in the list is drawn in front of one
    // lower down, so the lowest track is the bottom layer.
    std::vector<const cs::TimelineClip*> active;
    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.kind != cs::ClipKind::video || clip.recording)
            continue;
        if (timelineSeconds < clip.startSeconds || timelineSeconds >= clip.startSeconds + clip.durationSeconds)
            continue;
        active.push_back(&clip);
    }
    std::stable_sort(active.begin(), active.end(), [](const cs::TimelineClip* a, const cs::TimelineClip* b) { return a->trackIndex > b->trackIndex; });

    videoActiveOrder.clear();
    for (const auto* clip : active)
        videoActiveOrder.push_back(clip->id);

    {
        int decoded = 0;
        for (const auto& feed : videoFeeds)
            if (feed.second->frame.isValid())
                ++decoded;
        juce::String decodeProblem;
        for (const auto& feed : videoFeeds)
            if (const auto reason = feed.second->scrub.getLastError(); reason.isNotEmpty())
                decodeProblem = reason;

        videoPanelHost.setExtraInfo("video clips under the playhead " + juce::String((int) active.size()) + ", pictures decoded " + juce::String(decoded)
                                    + ", playhead " + juce::String(timelineSeconds, 1) + " s"
                                    + (decodeProblem.isNotEmpty() ? "  |  DECODE PROBLEM: " + decodeProblem : juce::String()));
    }

    // Decoders for clips that are no longer under the playhead are let go.
    for (auto it = videoFeeds.begin(); it != videoFeeds.end();)
        it = std::find(videoActiveOrder.begin(), videoActiveOrder.end(), it->first) == videoActiveOrder.end() ? videoFeeds.erase(it) : std::next(it);

    if (active.empty())
    {
        videoView.setIdle();
        return;
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    for (const auto* clip : active)
    {
        auto& feedSlot = videoFeeds[clip->id];
        if (feedSlot == nullptr)
            feedSlot = std::make_unique<VideoLayerFeed>();
        auto& feed = *feedSlot;

        if (! feed.source.isValid())
        {
            const auto asset = resolveTimelineClipAsset(*clip);
            if (! asset.has_value())
                continue;
            feed.source = makeVideoSource(clip->assetId, asset->logicalPath);
        }

        // Decode at the size the layer is drawn at, never above it: a small picture-in-picture costs less.
        const auto scale = juce::jlimit(0.05f, 1.0f, cs::videoparams::number(clip->videoParams, cs::videoparams::layoutScale, 1.0f));
        const auto width = juce::jmax(32, juce::roundToInt((float) videoView.getWidth() * scale));
        const auto height = juce::jmax(32, juce::roundToInt((float) videoView.getHeight() * scale));
        const auto sourceSeconds = clip->sourceStartSeconds + (timelineSeconds - clip->startSeconds);

        // Paused and nothing changed: no new decode.
        const auto key = clip->assetId + "|" + juce::String(sourceSeconds, 3) + "|" + juce::String(width) + "x" + juce::String(height);
        if (key == feed.requestKey)
            continue;
        feed.requestKey = key;

        feed.scrub.requestFrame(feed.source, sourceSeconds,
                                [safeThis, clipId = clip->id](juce::Image image)
                                {
                                    if (safeThis == nullptr)
                                        return;

                                    const auto found = safeThis->videoFeeds.find(clipId);
                                    if (found == safeThis->videoFeeds.end())
                                        return;

                                    // A failed decode must not wipe out the last good picture (that would black out the
                                    // layer), and it should be tried again on the next tick.
                                    if (image.isValid())
                                        found->second->frame = std::move(image);
                                    else
                                        found->second->requestKey = {};
                                    safeThis->refreshVideoLayers();
                                },
                                width, height);
    }

    refreshVideoLayers();
}

void MainComponent::refreshVideoLayers()
{
    std::vector<cs::VideoLayer> layers;
    for (const auto& clipId : videoActiveOrder)
    {
        const auto feed = videoFeeds.find(clipId);
        if (feed == videoFeeds.end() || ! feed->second->frame.isValid())
            continue; // its first picture has not arrived yet

        for (const auto& clip : timelineModel.getClips())
            if (clip.id == clipId)
            {
                layers.push_back(cs::videoparams::toLayer(feed->second->frame, clip.videoParams));
                break;
            }
    }

    if (layers.empty())
        videoView.setIdle();
    else
        videoView.setLayers(std::move(layers));
}

void MainComponent::showVideoClipSettings(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, (int) timelineModel.getClips().size()) || timelineModel.getClips()[(size_t) clipIndex].kind != cs::ClipKind::video)
        return;

    if (videoSettingsWindow != nullptr)
    {
        videoSettingsWindow->toFront(true); // one at a time: close it to open another clip's
        return;
    }

    const auto clip = timelineModel.getClips()[(size_t) clipIndex];
    auto stateBeforeEdit = std::make_shared<juce::ValueTree>(timelineModel.createState());
    auto edited = std::make_shared<bool>(false);
    const auto clipId = clip.id;

    auto panel = std::make_unique<VideoClipSettingsPanel>(clip.videoParams);
    panel->onSettingsChanged = [this, clipId, edited](const juce::NamedValueSet& values)
    {
        for (size_t i = 0; i < timelineModel.getClips().size(); ++i)
            if (timelineModel.getClips()[i].id == clipId)
                timelineModel.setClipVideoParams((int) i, values);

        *edited = true;
        refreshVideoLayers(); // the picture updates while a slider moves
    };

    // One undo step for everything done in this window, and the project is saved when it closes.
    auto window = std::make_unique<ManagedDocumentWindow>("Video effects and layout - " + clip.displayName, juce::Colour(0xff141a24), juce::DocumentWindow::closeButton,
                                                          [this, stateBeforeEdit, edited]
                                                          {
                                                              if (*edited)
                                                              {
                                                                  pushTimelineUndoState(*stateBeforeEdit);
                                                                  projectDirty = true;
                                                                  saveSessionToDisk();
                                                              }

                                                              videoSettingsWindow.reset(); // runs after the window has finished closing
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setContentOwned(panel.release(), true);
    window->setAlwaysOnTop(true);
    window->centreAroundComponent(this, window->getWidth(), window->getHeight());
    window->setVisible(true);
    videoSettingsWindow = std::move(window);
}

namespace
{
// Source files keep their own name inside the project; two files with the same name (a folder import can easily have
// them) must not replace each other, so the later one gets " (2)", " (3)" and so on.
template <typename Assets>
std::set<juce::String> collectTakenSourcePaths(const Assets& assets)
{
    std::set<juce::String> taken;
    for (const auto& asset : assets)
        taken.insert(asset.logicalPath);
    return taken;
}

juce::String makeUniqueSourcePath(const juce::File& file, std::set<juce::String>& taken)
{
    const juce::String root = creation::assets::ProjectContainerPaths::sourceAssetRoot;
    auto path = root + file.getFileName();
    for (int n = 2; taken.count(path) > 0; ++n)
        path = root + file.getFileNameWithoutExtension() + " (" + juce::String(n) + ")" + file.getFileExtension();

    taken.insert(path);
    return path;
}
}

void MainComponent::runVideoImport(juce::StringArray filePaths, int trackIndex, double startSeconds)
{
    if (progressTask != nullptr)
    {
        reportError("Could not start the import: another long action is still running. Wait for it to finish, or cancel it.");
        return;
    }

    auto items = std::make_shared<std::vector<VideoImportItem>>();
    auto taken = collectTakenSourcePaths(projectSession.getManifest().assetCatalog.assets);
    for (const auto& path : filePaths)
    {
        VideoImportItem item;
        item.file = juce::File(path);
        item.logicalPath = makeUniqueSourcePath(item.file, taken);
        item.assetId = "asset:" + juce::Uuid().toString();
        items->push_back(std::move(item));
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    // Everything slow happens on the task's thread: opening the video (file I/O and negotiation with the OS
    // decoder -- not safe on the message thread, see VideoDecodeService.h) and streaming it into the project
    // in pieces. Only the quick bookkeeping (asset list, clip) is done afterwards, on the message thread.
    auto work = [safeThis, items](ProgressTask& task)
    {
        const auto count = (int) items->size();
        for (int i = 0; i < count && ! task.cancelRequested(); ++i)
        {
            auto& item = (*items)[(size_t) i];
            const auto name = item.file.getFileName();
            const auto label = count > 1 ? " (" + juce::String(i + 1) + " of " + juce::String(count) + ")" : juce::String();
            const auto share = 1.0 / (double) count;
            const auto base = (double) i * share;

            if (! item.file.existsAsFile())
            {
                item.error = "Could not import " + name + ": the file was not found.";
                continue;
            }

            task.report(base, "Reading " + name + label + "...");
            cs::VideoDecodeService decodeService;
            item.info = decodeService.open(item.file);
            if (! item.info.valid)
            {
                const auto reason = decodeService.getLastError();
                item.error = "Could not open video " + name + (reason.isNotEmpty() ? ": " + reason : juce::String());
                continue;
            }

            encodeThumbnailJpeg(decodeService.decodeFrameAt(juce::jmin(1.0, item.info.durationSeconds * 0.1), 320, 180), item.thumbnailJpeg);

            // Reading the file is a bit over 5% of the bar; the rest is the upload.
            juce::String uploadError;
            const auto sizeText = juce::File::descriptionOfSizeInBytes(item.file.getSize());
            task.report(base + share * 0.05, "Copying " + name + " (" + sizeText + ") into the project" + label + "...");

            if (safeThis == nullptr)
                return;

            // The upload is thread-safe on its own (own client, own connection); only the session's revision
            // counter is touched, which nothing else reads while this window is up.
            const auto ok = safeThis->projectSession.writeEntryFromFile(item.logicalPath, item.file, uploadError, 9,
                [&](double fraction)
                {
                    task.report(base + share * (0.05 + 0.65 * fraction),
                                "Copying " + name + " (" + sizeText + ") into the project" + label + " - " + juce::String((int) std::round(fraction * 100.0)) + "%");
                    return ! task.cancelRequested();
                });

            if (! ok)
            {
                if (! task.cancelRequested() && ! safeThis->projectSession.lastWriteWasCancelled())
                    item.error = uploadError.isNotEmpty() ? uploadError : "Could not import " + name + ".";
                continue;
            }

            item.uploaded = true;

            // The video's own sound: read once here (the file is local), kept in memory, and saved into the project.
            if (task.cancelRequested())
                continue;

            task.report(base + share * 0.7, "Reading " + name + "'s sound" + label + "...");
            cs::VideoSource localSource;
            localSource.key = item.assetId;
            localSource.file = item.file;
            auto wav = std::make_shared<juce::MemoryBlock>();
            juce::String soundError;
            if (! renderVideoAudioWav(localSource, *wav, item.hasAudio, soundError))
            {
                // The video is in; only its sound is missing, and playing will offer to try again.
                DBG("video sound extraction failed: " + soundError);
                item.hasAudio = false;
                continue;
            }

            if (! item.hasAudio)
                continue;

            task.report(base + share * 0.8, "Saving " + name + "'s sound into the project" + label + "...");
            if (safeThis->projectSession.writeEntry(MainComponent::videoAudioCachePath(item.assetId), *wav, juce::Time::getCurrentTime()))
                item.audioBytes = wav;
            else
                item.hasAudio = false;
        }
    };

    auto finished = [safeThis, items, trackIndex, startSeconds](bool cancelled)
    {
        if (safeThis == nullptr)
            return;

        auto* self = safeThis.getComponent();
        auto nextStart = startSeconds;
        juce::StringArray problems;
        int imported = 0;

        for (auto& item : *items)
        {
            if (! item.uploaded)
            {
                if (item.error.isNotEmpty())
                    problems.add(item.error);
                continue;
            }

            juce::String clipError;
            if (item.audioBytes != nullptr)
                self->videoAudioBytes[item.assetId] = item.audioBytes;
            else if (! item.hasAudio && ! item.info.hasAudio)
                self->videosWithoutAudio.insert(item.assetId);

            const auto clipIndex = self->addImportedVideoToTracker(item.file, item.assetId, item.logicalPath, item.file.getSize(), item.info, item.thumbnailJpeg,
                                                                   trackIndex, nextStart, clipError);
            if (clipIndex >= 0)
            {
                if (trackIndex >= 0)
                {
                    const auto& clip = self->timelineModel.getClips()[(size_t) clipIndex];
                    nextStart = clip.startSeconds + clip.durationSeconds;
                }
                ++imported;
            }
            else
            {
                problems.add("Could not import " + item.file.getFileName() + (clipError.isNotEmpty() ? ": " + clipError : juce::String()));
            }
        }

        if (imported > 0)
        {
            self->refreshProjectAssets();
            if (trackIndex >= 0)
            {
                self->trackerPanel.setSelectedTrack(trackIndex);
                self->trackerPanel.refreshTimelineView();
                self->setWorkspaceMode(WorkspaceMode::tracker);
            }
            self->saveSessionToDisk(true);
        }

        if (problems.isEmpty() && cancelled)
            self->showToast(imported > 0 ? "Import cancelled - " + juce::String(imported) + " file(s) were already in." : "Import cancelled.");
        else if (problems.isEmpty() && imported > 0)
            self->showToast("Imported " + juce::String(imported) + " video file(s).");

        if (! problems.isEmpty())
            self->reportError(problems.joinIntoString("\n\n"));

        // The window is done; free it once we are out of its own callback.
        juce::MessageManager::callAsync([safeThis]
        {
            if (safeThis != nullptr)
                safeThis->progressTask.reset();
        });
    };

    progressTask = std::make_unique<ProgressTask>("Importing video", std::move(work), std::move(finished));
    progressTask->start();
}

void MainComponent::showImportWindow()
{
    if (importWindow != nullptr)
    {
        importWindow->toFront(true);
        return;
    }

    auto* panel = new cs::ImportPanel();
    panel->onCancel = [safe = juce::Component::SafePointer<MainComponent>(this)]
    {
        if (safe != nullptr && safe->importWindow != nullptr)
            safe->importWindow->exitModalState(0);
    };
    panel->onImport = [safe = juce::Component::SafePointer<MainComponent>(this)](const juce::StringArray& paths)
    {
        if (safe == nullptr)
            return;

        if (safe->importWindow != nullptr)
            safe->importWindow->exitModalState(0);

        // Start after the dialog has closed, so its own callback is not still on the stack.
        juce::MessageManager::callAsync([safe, paths]
        {
            if (safe != nullptr)
                safe->runLibraryImport(paths);
        });
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(panel);
    options.dialogTitle = "Import";
    options.dialogBackgroundColour = juce::Colour(0xff141a24);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    importWindow = options.launchAsync();
}

namespace
{
struct LibraryAudioItem
{
    juce::File file;
    juce::String logicalPath;
    juce::String assetId;
    juce::String error;
    bool uploaded = false;
};

juce::String audioMediaTypeFor(const juce::File& file)
{
    const auto extension = file.getFileExtension().toLowerCase();
    if (extension == ".mp3") return "audio/mpeg";
    if (extension == ".ogg") return "audio/ogg";
    if (extension == ".flac") return "audio/flac";
    if (extension == ".aif" || extension == ".aiff") return "audio/aiff";
    return "audio/wav";
}
}

// Brings files into the project's asset library without placing them on any track. Audio goes in as it is (the same as
// dropping it on the Tracker); video goes in through the video import, which keeps the original and extracts its
// sound as WAV for the engine. Everything the engine needs is done by those existing paths.
void MainComponent::runLibraryImport(juce::StringArray filePaths)
{
    if (filePaths.isEmpty())
        return;

    if (! ensureStorageRootConfigured())
        return;

    juce::String projectError;
    if (! ensureProjectSessionActive(projectError))
    {
        reportError(projectError.isNotEmpty() ? projectError : "Could not open a project to import into.");
        return;
    }

    if (progressTask != nullptr)
    {
        reportError("Could not start the import: another long action is still running. Wait for it to finish, or cancel it.");
        return;
    }

    juce::StringArray audioFiles, videoFiles, skipped;
    for (const auto& path : filePaths)
    {
        const auto file = juce::File(path);
        const auto extension = file.getFileExtension().toLowerCase();
        if (cs::ImportPanel::isAudioExtension(extension))
            audioFiles.add(path);
        else if (cs::ImportPanel::isVideoExtension(extension))
            videoFiles.add(path);
        else
            skipped.add(file.getFileName());
    }

    if (audioFiles.isEmpty() && videoFiles.isEmpty())
    {
        reportError("None of those files are audio or video files this app can import.");
        return;
    }

    if (audioFiles.isEmpty())
    {
        runVideoImport(videoFiles, -1, 0.0);
        return;
    }

    auto items = std::make_shared<std::vector<LibraryAudioItem>>();
    auto taken = collectTakenSourcePaths(projectSession.getManifest().assetCatalog.assets);
    for (const auto& path : audioFiles)
    {
        LibraryAudioItem item;
        item.file = juce::File(path);
        item.logicalPath = makeUniqueSourcePath(item.file, taken);
        item.assetId = "asset:" + juce::Uuid().toString();
        items->push_back(std::move(item));
    }

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);

    auto work = [safeThis, items](ProgressTask& task)
    {
        const auto count = (int) items->size();
        for (int i = 0; i < count && ! task.cancelRequested(); ++i)
        {
            auto& item = (*items)[(size_t) i];
            const auto name = item.file.getFileName();
            const auto label = count > 1 ? " (" + juce::String(i + 1) + " of " + juce::String(count) + ")" : juce::String();
            const auto share = 1.0 / (double) count;
            const auto base = (double) i * share;

            if (! item.file.existsAsFile())
            {
                item.error = "Could not import " + name + ": the file was not found.";
                continue;
            }

            if (safeThis == nullptr)
                return;

            const auto sizeText = juce::File::descriptionOfSizeInBytes(item.file.getSize());
            task.report(base, "Copying " + name + " (" + sizeText + ") into the project" + label + "...");

            juce::String uploadError;
            const auto ok = safeThis->projectSession.writeEntryFromFile(item.logicalPath, item.file, uploadError, 9,
                [&](double fraction)
                {
                    task.report(base + share * fraction,
                                "Copying " + name + " (" + sizeText + ") into the project" + label + " - " + juce::String((int) std::round(fraction * 100.0)) + "%");
                    return ! task.cancelRequested();
                });

            if (! ok)
            {
                if (! task.cancelRequested() && ! safeThis->projectSession.lastWriteWasCancelled())
                    item.error = uploadError.isNotEmpty() ? uploadError : "Could not import " + name + ".";
                continue;
            }

            item.uploaded = true;
        }
    };

    auto finished = [safeThis, items, videoFiles, skipped](bool cancelled)
    {
        if (safeThis == nullptr)
            return;

        auto* self = safeThis.getComponent();
        juce::StringArray problems;
        int imported = 0;

        for (auto& item : *items)
        {
            if (! item.uploaded)
            {
                if (item.error.isNotEmpty())
                    problems.add(item.error);
                continue;
            }

            creation::assets::AssetDescriptor asset;
            asset.id = item.assetId;
            asset.version = "1";
            asset.versionId = asset.id + "@1";
            asset.displayName = item.file.getFileNameWithoutExtension();
            asset.logicalPath = item.logicalPath;
            asset.kind = creation::assets::AssetKind::audio;
            asset.mediaType = audioMediaTypeFor(item.file);
            asset.fileSizeBytes = (juce::int64) item.file.getSize();
            asset.createdAt = asset.modifiedAt = juce::Time::getCurrentTime();
            asset.sourceApp = "Djehuti Station";
            self->projectSession.upsertAssetDescriptor(asset);
            ++imported;
        }

        if (imported > 0)
        {
            juce::String commitError;
            if (! self->projectSession.commit(commitError))
                problems.add(commitError.isNotEmpty() ? commitError : juce::String("The imported audio could not be saved into the project."));

            self->refreshProjectAssets();
            self->saveSessionToDisk(true);
        }

        if (! skipped.isEmpty())
            problems.add("Not imported (not audio or video this app can import): " + skipped.joinIntoString(", "));

        const auto videosFollow = ! cancelled && ! videoFiles.isEmpty();
        if (! videosFollow)
        {
            if (cancelled)
                self->showToast(imported > 0 ? "Import cancelled - " + juce::String(imported) + " file(s) were already in." : "Import cancelled.");
            else if (imported > 0 && problems.isEmpty())
                self->showToast("Imported " + juce::String(imported) + " audio file(s).");
        }

        if (! problems.isEmpty())
            self->reportError(problems.joinIntoString("\n\n"));

        // The audio window is done; free it once we are out of its own callback, then run the videos (each is its own
        // progress window, with the same Cancel).
        juce::MessageManager::callAsync([safeThis, videosFollow, videoFiles]
        {
            if (safeThis == nullptr)
                return;

            safeThis->progressTask.reset();
            if (videosFollow)
                safeThis->runVideoImport(videoFiles, -1, 0.0);
        });
    };

    progressTask = std::make_unique<ProgressTask>("Importing audio", std::move(work), std::move(finished));
    progressTask->start();
}

std::optional<creation::assets::AssetDescriptor> MainComponent::resolveTimelineClipAsset(const cs::TimelineClip& clip) const
{
    if (! projectSession.isValid() || clip.assetId.isEmpty())
        return std::nullopt;

    const auto* descriptor = projectSession.getManifest().assetCatalog.findById(clip.assetId);
    if (descriptor != nullptr)
        return *descriptor;

    return std::nullopt;
}

void MainComponent::exportProjectAssetRaw(const creation::assets::AssetDescriptor& asset)
{
    if (asset.kind != creation::assets::AssetKind::audio
        && asset.kind != creation::assets::AssetKind::render)
    {
        contentPanel.setStatusText("Only project WAV/render audio can be exported raw right now.");
        return;
    }

    const auto exportBytes = readAssetBytes(asset.logicalPath, asset.versionId);
    if (exportBytes == nullptr)
    {
        contentPanel.setStatusText("Could not read that asset for export.");
        return;
    }

    auto fileName = asset.logicalPath.fromLastOccurrenceOf("/", false, false);
    rawAssetExportChooser = std::make_unique<juce::FileChooser>("Export raw project audio",
                                                                juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                    .getChildFile(fileName),
                                                                "*.wav",
                                                                true);
    auto chooser = rawAssetExportChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, asset, exportBytes](const juce::FileChooser& result)
                         {
                             auto destination = result.getResult();
                             if (chooser == rawAssetExportChooser.get())
                                 rawAssetExportChooser.reset();

                             if (destination.getFullPathName().isEmpty())
                                 return;

                             if (destination.getFileExtension().isEmpty())
                                 destination = destination.withFileExtension(juce::File(asset.logicalPath).getFileExtension());

                             if (destination.existsAsFile() && ! destination.deleteFile())
                             {
                                 contentPanel.setStatusText("Could not replace the existing export file.");
                                 return;
                             }

                             if (! destination.replaceWithData(exportBytes->getData(), exportBytes->getSize()))
                             {
                                 contentPanel.setStatusText("Could not export the raw audio file.");
                                 return;
                             }

                             contentPanel.setStatusText("Exported raw audio: " + destination.getFileName());
                         });
}

bool MainComponent::saveRenderToProject(const juce::AudioBuffer<float>& buffer, double sampleRate, int bitsPerSample, bool dither,
                                        const juce::String& displayName, creation::assets::AssetDescriptor& savedAsset,
                                        juce::String& errorMessage)
{
    juce::String projectError;
    if (! ensureProjectSessionActive(projectError))
    {
        errorMessage = projectError.isNotEmpty() ? projectError : "Could not initialize the project for a render.";
        return false;
    }

    juce::MemoryBlock encoded;
    if (! encodeWavToMemory(buffer, sampleRate, bitsPerSample, dither, encoded, errorMessage))
        return false;

    const auto logicalPath = creation::assets::ProjectContainerPaths::derivedAssetRoot
                           + slugForProjectAssetName(displayName) + "-" + makeRecordingTimestamp() + ".wav";
    if (! projectSession.writeEntry(logicalPath, encoded, juce::Time::getCurrentTime()))
    {
        errorMessage = "Could not write the render into the project.";
        return false;
    }

    savedAsset = {};
    savedAsset.id = "asset:" + juce::Uuid().toString();
    savedAsset.version = "1";
    savedAsset.versionId = savedAsset.id + "@1";
    savedAsset.displayName = displayName;
    savedAsset.logicalPath = logicalPath;
    savedAsset.kind = creation::assets::AssetKind::render;
    savedAsset.mediaType = "audio/wav";
    savedAsset.fileSizeBytes = (int64) encoded.getSize();
    savedAsset.createdAt = savedAsset.modifiedAt = juce::Time::getCurrentTime();
    savedAsset.sourceApp = "Djehuti Station";
    savedAsset.description = "Rendered from the Tracker.";
    projectSession.upsertAssetDescriptor(savedAsset);

    if (! projectSession.commit(errorMessage))
    {
        if (errorMessage.isEmpty())
            errorMessage = "Could not save the render into the project.";
        return false;
    }

    refreshProjectAssets();
    refreshContentLibrary();
    saveSessionToDisk(true);
    return true;
}

void MainComponent::showToast(const juce::String& message)
{
    if (message.trim().isEmpty())
    {
        toast.dismiss();
        return;
    }

    if (toast.isVisible() && toast.getMessage() == message)
        return;

    const auto width = juce::jmax(200, juce::jmin(720, getWidth() - 40));
    const auto height = ToastMessage::preferredHeight(message, width);
    toast.setBounds((getWidth() - width) / 2, getHeight() - height - 28, width, height);
    toast.show(message);
}

void MainComponent::reportError(const juce::String& message)
{
    if (message.trim().isEmpty())
        return;

    pendingErrors.addIfNotAlreadyThere(message.trim());
    if (! errorDialogShowing)
        showPendingErrors();
}

void MainComponent::showPendingErrors()
{
    if (pendingErrors.isEmpty())
        return;

    errorDialogShowing = true;
    const auto count = pendingErrors.size();
    const auto text = pendingErrors.joinIntoString("\n\n");
    pendingErrors.clear();

    // JUCE numbers a two-button box's results 1 (first) and 0 (second, also bound to Escape).
    auto options = juce::MessageBoxOptions()
                       .withIconType(juce::MessageBoxIconType::WarningIcon)
                       .withTitle(count > 1 ? "Something went wrong (" + juce::String(count) + " problems)" : juce::String("Something went wrong"))
                       .withMessage(text)
                       .withButton("Copy details")
                       .withButton("Close");

    juce::AlertWindow::showAsync(options, [safeThis = juce::Component::SafePointer<MainComponent>(this), text](int result)
    {
        if (result == 1)
            juce::SystemClipboard::copyTextToClipboard(text);

        if (safeThis != nullptr)
        {
            safeThis->errorDialogShowing = false;
            safeThis->showPendingErrors(); // anything that arrived while this was open
        }
    });
}

void MainComponent::toggleProjectAssetPreview(const creation::assets::AssetDescriptor& asset)
{
    if (previewingProjectAssetId == asset.id && engine.isPreviewingAsset())
    {
        engine.stopAssetPreview();
        previewingProjectAssetId = {};
        contentPanel.setPreviewingAssetId({});
        return;
    }

    juce::String error;
    const auto previewBytes = readAssetBytes(asset.logicalPath, asset.versionId);
    if (previewBytes == nullptr)
    {
        transportBar.setStatusText("Could not open that asset to play it.");
        return;
    }

    engine.stopAssetPreview();
    if (! engine.previewAssetData(previewBytes, {}, error))
    {
        transportBar.setStatusText(error.isNotEmpty() ? error : "Could not play that asset.");
        previewingProjectAssetId = {};
        contentPanel.setPreviewingAssetId({});
        return;
    }

    previewingProjectAssetId = asset.id;
    contentPanel.setPreviewingAssetId(asset.id);
}

namespace
{
// Runs the offline render on a worker thread behind a modal progress window with a Cancel button, so the UI
// stays alive and the user can see how far along it is.
class RenderJob final : public juce::ThreadWithProgressWindow
{
public:
    RenderJob(WorkstationAudioEngine& engineToUse,
              juce::Array<WorkstationAudioEngine::PlaybackClipTarget> clipTargets,
              juce::Array<WorkstationAudioEngine::SignalClipTarget> signalClipTargets,
              double lengthSeconds,
              WorkstationAudioEngine::RenderSettings renderSettings)
        : juce::ThreadWithProgressWindow("Rendering", true, true, 30000, "Cancel"),
          engine(engineToUse),
          targets(std::move(clipTargets)),
          signalTargets(std::move(signalClipTargets)),
          durationSeconds(lengthSeconds),
          settings(std::move(renderSettings))
    {
        setStatusMessage("Rendering the master mix...");
    }

    void run() override
    {
        const auto startedAt = juce::Time::getMillisecondCounterHiRes();
        settings.onProgress = [this, startedAt](float fraction)
        {
            const auto elapsed = (juce::Time::getMillisecondCounterHiRes() - startedAt) * 0.001;
            const auto remaining = fraction > 0.02f ? elapsed * (1.0 - fraction) / fraction : 0.0;
            setProgress((double) fraction);
            setStatusMessage("Rendering the master mix\n" + formatClock(durationSeconds * fraction) + " of " + formatClock(durationSeconds)
                             + "   |   elapsed " + formatClock(elapsed)
                             + (fraction > 0.02f ? "   |   about " + juce::String((int) std::ceil(remaining)) + " s left" : juce::String()));
            return ! threadShouldExit();
        };

        succeeded = engine.renderTrackerMixToBuffer(targets, durationSeconds, settings, rendered, errorMessage, signalTargets);
    }

    // Called on the message thread when the render has finished or the user cancelled it.
    void threadComplete(bool userPressedCancel) override
    {
        if (onFinished)
            onFinished(*this, userPressedCancel);
    }

    static juce::String formatClock(double seconds)
    {
        const auto total = (int) std::floor(juce::jmax(0.0, seconds));
        return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2);
    }

    std::function<void(RenderJob&, bool userPressedCancel)> onFinished;
    bool succeeded = false;
    juce::AudioBuffer<float> rendered;
    juce::String errorMessage;

private:
    WorkstationAudioEngine& engine;
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    juce::Array<WorkstationAudioEngine::SignalClipTarget> signalTargets;
    double durationSeconds = 0.0;
    WorkstationAudioEngine::RenderSettings settings;
};

juce::String expandRenderName(const juce::String& pattern, const juce::String& projectName)
{
    return pattern.replace("$project", projectName.isNotEmpty() ? projectName : "Untitled")
                  .replace("$date", juce::Time::getCurrentTime().formatted("%Y-%m-%d"));
}
}

void MainComponent::showRenderDialog(RenderRequest::Destination preferredDestination)
{
    if (renderDialogWindow != nullptr)
        return;

    if (engine.isRecording() || engine.isPlaying())
    {
        transportBar.setStatusText("Stop playback or recording before rendering.");
        return;
    }

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lengthSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(targets, lengthSeconds, errorMessage, false))
    {
        transportBar.setStatusText(errorMessage);
        return;
    }

    auto hasMidiClips = false;
    for (const auto& clip : timelineModel.getClips())
        if (clip.kind == cs::ClipKind::midi && ! clip.recording)
            hasMidiClips = true;

    auto initial = lastRenderRequest;
    initial.destination = preferredDestination;
    if (initial.name.isEmpty())
        initial.name = "$project-mix-$date";
    if (initial.customEndSeconds <= initial.customStartSeconds)
        initial.customEndSeconds = lengthSeconds;

    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    auto* dialog = new RenderDialog(initial, lengthSeconds, currentDevice != nullptr ? currentDevice->getCurrentSampleRate() : 48000.0, hasMidiClips);
    dialog->onCancel = [this]
    {
        if (renderDialogWindow != nullptr)
            renderDialogWindow->exitModalState(0);
    };
    dialog->onRender = [this](const RenderRequest& request)
    {
        if (renderDialogWindow != nullptr)
            renderDialogWindow->exitModalState(1);

        // Start once the dialog has closed, from a clean message-loop turn.
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this), request]
        {
            if (safeThis != nullptr)
                safeThis->beginRender(request);
        });
    };

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned(dialog);
    options.dialogTitle = "Render";
    options.dialogBackgroundColour = juce::Colour(0xff10151d);
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = false;
    renderDialogWindow = options.launchAsync();
}

void MainComponent::beginRender(const RenderRequest& request)
{
    lastRenderRequest = request;

    if (request.destination == RenderRequest::Destination::project)
    {
        runRenderJob(request, {});
        return;
    }

    const auto projectName = projectSession.isValid() ? projectSession.getManifest().projectName : juce::String();
    const auto defaultName = slugForProjectAssetName(expandRenderName(request.name, projectName)) + ".wav";
    renderExportChooser = std::make_unique<juce::FileChooser>("Save the render as a WAV file",
                                                              juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
                                                                  .getChildFile(defaultName),
                                                              "*.wav",
                                                              true);
    auto chooser = renderExportChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser, request](const juce::FileChooser& result)
                         {
                             auto destination = result.getResult();
                             if (chooser == renderExportChooser.get())
                                 renderExportChooser.reset();

                             if (destination.getFullPathName().isEmpty())
                                 return;

                             if (destination.getFileExtension().isEmpty())
                                 destination = destination.withFileExtension(".wav");

                             runRenderJob(request, destination);
                         });
}

void MainComponent::runRenderJob(const RenderRequest& request, const juce::File& destinationFile)
{
    const auto projectName = projectSession.isValid() ? projectSession.getManifest().projectName : juce::String();
    const auto displayName = expandRenderName(request.name, projectName);

    // A fresh look at the timeline: what is on it now is what gets rendered.
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lengthSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(targets, lengthSeconds, errorMessage, false))
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Nothing to render", errorMessage);
        return;
    }

    juce::Array<WorkstationAudioEngine::SignalClipTarget> signalTargets;
    juce::String signalError;
    buildSignalClipTargets(signalTargets, signalError);
    if (signalError.isNotEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Could not render", signalError);
        return;
    }

    auto startSeconds = 0.0;
    auto rangeSeconds = lengthSeconds;
    if (request.range == RenderRequest::Range::custom)
    {
        startSeconds = request.customStartSeconds;
        rangeSeconds = request.customEndSeconds - request.customStartSeconds;
    }
    const auto renderSeconds = rangeSeconds + request.tailSeconds;

    auto* currentDevice = deviceManager.getCurrentAudioDevice();
    WorkstationAudioEngine::RenderSettings settings;
    settings.sampleRate = request.sampleRate > 0.0 ? request.sampleRate
                                                   : (currentDevice != nullptr ? currentDevice->getCurrentSampleRate() : 48000.0);
    settings.blockSize = currentDevice != nullptr ? currentDevice->getCurrentBufferSizeSamples() : 512;
    settings.startSeconds = startSeconds;
    settings.normalizePeak = request.normalize != RenderRequest::Normalize::off;
    settings.peakTargetDecibels = request.normalize == RenderRequest::Normalize::peakMinus03 ? -0.3f : -1.0f;

    engine.stopAssetPreview();
    previewingProjectAssetId = {};
    contentPanel.setPreviewingAssetId({});

    // The render runs on a worker thread, so the live audio callback is taken off the engine for its duration.
    renderJob = std::make_unique<RenderJob>(engine, targets, signalTargets, renderSeconds, settings);
    auto* job = static_cast<RenderJob*>(renderJob.get());
    job->onFinished = [this, request, destinationFile, settings, displayName, renderSeconds, startSeconds]
                      (RenderJob& job, bool userPressedCancel)
    {
        engine.attachToDevice(deviceManager);

        // The job object is finished with once this handler returns; release it from a clean message-loop turn.
        juce::MessageManager::callAsync([safeThis = juce::Component::SafePointer<MainComponent>(this)]
        {
            if (safeThis != nullptr)
                safeThis->renderJob.reset();
        });

        juce::String errorMessage;
        if (! job.succeeded)
        {
            if (userPressedCancel || job.errorMessage == "Render cancelled.")
                transportBar.setStatusText("Render cancelled.");
            else
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Render failed", job.errorMessage);
            return;
        }

        const auto peak = job.rendered.getMagnitude(0, job.rendered.getNumSamples());
        const auto peakDb = peak > 0.0f ? juce::Decibels::gainToDecibels(peak) : -100.0f;
        const auto clips = request.bitsPerSample < 32 && peak > 1.0f;
        const auto ditherOn = request.dither && request.bitsPerSample == 16;

        juce::String where;
        creation::assets::AssetDescriptor savedAsset;
        auto savedToProject = false;

        if (request.destination == RenderRequest::Destination::project)
        {
            if (! saveRenderToProject(job.rendered, settings.sampleRate, request.bitsPerSample, ditherOn, displayName, savedAsset, errorMessage))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Could not save the render", errorMessage);
                return;
            }

            savedToProject = true;
            where = "Saved to the project as \"" + savedAsset.displayName + "\".";

            if (request.placeOnNewTrack)
            {
                addTrack();
                trackerPanel.setSelectedTrack(engine.getTrackCount() - 1);
                placeProjectAssetOnTracker(savedAsset, startSeconds);
            }
        }
        else
        {
            juce::MemoryBlock encoded;
            if (! encodeWavToMemory(job.rendered, settings.sampleRate, request.bitsPerSample, ditherOn, encoded, errorMessage)
                || (destinationFile.existsAsFile() && ! destinationFile.deleteFile())
                || ! destinationFile.getParentDirectory().createDirectory()
                || ! destinationFile.replaceWithData(encoded.getData(), encoded.getSize()))
            {
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Could not save the render",
                                                       errorMessage.isNotEmpty() ? errorMessage : "The file could not be written.");
                return;
            }

            where = "Saved to " + destinationFile.getFullPathName();
        }

        auto hasMidiClips = false;
        for (const auto& clip : timelineModel.getClips())
            if (clip.kind == cs::ClipKind::midi && ! clip.recording)
                hasMidiClips = true;

        const auto bitLabel = request.bitsPerSample == 32 ? juce::String("32-bit float") : juce::String(request.bitsPerSample) + "-bit";
        juce::String summary = where + "\n\n"
            + "Length: " + RenderJob::formatClock(renderSeconds) + "  (" + bitLabel + ", " + juce::String((int) settings.sampleRate) + " Hz)\n"
            + "Peak: " + juce::String(peakDb, 1) + " dBFS\n"
            + (clips ? "Warning: the peak goes above 0 dBFS, so this " + bitLabel + " file clips. Try Normalize: Peak to -1 dB.\n"
                     : juce::String("No clipping.\n"));
        if (hasMidiClips)
            summary += "\nMIDI instrument tracks were not included in this render.";

        auto options = juce::MessageBoxOptions()
                           .withIconType(clips ? juce::MessageBoxIconType::WarningIcon : juce::MessageBoxIconType::InfoIcon)
                           .withTitle("Render finished")
                           .withMessage(summary)
                           .withButton(savedToProject ? "Play" : "OK");
        if (savedToProject)
            options = options.withButton("Close");

        juce::AlertWindow::showAsync(options, [this, savedAsset, savedToProject](int result)
        {
            if (savedToProject && result == 1)
                toggleProjectAssetPreview(savedAsset);
        });
    };

    engine.detachFromDevice(deviceManager);
    job->launchThread();
}

void MainComponent::showProjectMenu()
{
    suiteShellController.showProjectBrowser();
}

void MainComponent::showSuiteSettingsWindow()
{
    if (suiteSettingsWindow != nullptr)
    {
        suiteSettingsWindow->toFront(true);
        return;
    }

    auto panel = std::make_unique<SuiteSettingsPanel>();
    panel->setSettings(suiteSettings);
    panel->onBrowseRequested = [this](const juce::String& fieldId)
    {
        chooseSuiteDirectory(fieldId);
    };
    panel->onApplyRequested = [this](const SuiteSettings& settings)
    {
        applySuiteSettings(settings);
    };
    panel->onReadEulaRequested = [this]
    {
        suiteShellController.showSuiteEula();
    };

    auto* panelRaw = panel.get();
    auto window = std::make_unique<ManagedDocumentWindow>("Djehuti Suite Control",
                                                          juce::Colour(0xff11151c),
                                                          juce::DocumentWindow::allButtons,
                                                          [this]
                                                          {
                                                              closeSuiteSettingsWindow();
                                                          });
    window->setUsingNativeTitleBar(true);
    window->setResizable(true, true);
    window->setContentOwned(panel.release(), true);
    window->centreWithSize(940, 560);
    window->setVisible(true);

    suiteSettingsPanel = panelRaw;
    suiteSettingsWindow = std::move(window);
}

void MainComponent::closeSuiteSettingsWindow()
{
    suiteSettingsPanel = nullptr;
    suiteSettingsWindow.reset();
}

void MainComponent::chooseSuiteDirectory(const juce::String& fieldId)
{
    juce::String currentPath = suiteSettings.suiteVfsRoot;

    suiteDirectoryChooser = std::make_unique<juce::FileChooser>("Choose a folder for the Djehuti Suite",
                                                                currentPath.isNotEmpty() ? juce::File(currentPath) : juce::File(),
                                                                "*",
                                                                true);
    auto chooser = suiteDirectoryChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                         [this, chooser, fieldId](const juce::FileChooser& result)
                         {
                             auto selected = result.getResult();
                             if (chooser == suiteDirectoryChooser.get())
                                 suiteDirectoryChooser.reset();

                             if (selected == juce::File())
                                 return;

                             auto selectedPath = selected.getFullPathName();
                             if (fieldId == "suite_vfs_root")
                                 suiteSettings.suiteVfsRoot = selectedPath;

                             if (suiteSettingsPanel != nullptr)
                                 suiteSettingsPanel->setSettings(suiteSettings);
                         });
}

void MainComponent::applySuiteSettings(const SuiteSettings& settings)
{
    juce::String errorMessage;
    if (! suiteSettingsStore.save(settings, errorMessage))
    {
        transportBar.setStatusText(errorMessage);
        if (suiteSettingsPanel != nullptr)
            suiteSettingsPanel->setStatusText(errorMessage);
        return;
    }

    suiteSettings = settings;
    transportBar.setStatusText("Saved Djehuti Suite settings.");
    if (suiteSettingsPanel != nullptr)
        suiteSettingsPanel->setStatusText("Saved suite-wide settings for all Creation apps.");
}

void MainComponent::createNewProject()
{
    guardUnsavedProjectChange("creating a new project", [this] { beginCreateNewProject(); });
}

void MainComponent::beginCreateNewProject()
{
    if (! ensureStorageRootConfigured())
        return;

    auto* nameEditor = new juce::AlertWindow("Create New Project",
                                             "Enter a name for your new project container:",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", "");
    nameEditor->addButton("Create Project", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto name = dialog->getTextEditorContents("projectName").trim();
        if (name.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   "Project name cannot be empty. Please enter a name.");
            return;
        }

        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings, creation::assets::SuiteAppDomain::station, name, "1.0.0", "1.0.0", options->projectSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
        options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
        options->refreshProjectAssets();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Created project: " + options->projectSession.getManifest().projectName);
    }), true);
}

void MainComponent::openProject()
{
    if (! ensureStorageRootConfigured())
        return;

    suiteShellController.showProjectBrowser();
}

void MainComponent::openProject(const juce::String& projectId)
{
    guardUnsavedProjectChange("opening another project", [this, projectId]
    {
        juce::String errorMessage;
        if (! creation::assets::ProjectWorkspaceService::openProject(suiteSettings, projectId, projectSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
        settingsPanel.setProjectMetadata(projectSession.getManifest());
        refreshProjectAssets();
        loadSessionFromDisk();
        saveAppSettings();
    });
}

void MainComponent::saveProject()
{
    if (! projectSession.isValid())
    {
        createNewProject();
        return;
    }

    saveSessionToDisk(true);
    if (! projectDirty)
        transportBar.setStatusText("Project saved: " + projectSession.getManifest().projectName);
}

void MainComponent::saveProjectAs()
{
    if (! projectSession.isValid())
    {
        createNewProject();
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As",
                                             "Give the copied project a new name.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("projectName", projectSession.getManifest().projectName + " Copy");
    nameEditor->addButton("Save As", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto projectName = dialog->getTextEditorContents("projectName").trim();
        if (projectName.isEmpty())
            projectName = options->projectSession.getManifest().projectName + " Copy";

        juce::String errorMessage;
        creation::assets::ProjectSession newSession;
        if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings,
                                                                        creation::assets::SuiteAppDomain::station,
                                                                        projectName, "1.0.0", "1.0.0",
                                                                        newSession, errorMessage))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Project Error",
                                                   errorMessage);
            return;
        }

        // Transfer current session XML entry
        juce::MemoryBlock sessionData;
        if (options->projectSession.readEntry("session.xml", sessionData))
            newSession.writeEntry("session.xml", sessionData);
        newSession.commit(errorMessage);

        options->projectSession = std::move(newSession);
        options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
        options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
        options->refreshProjectAssets();
        options->loadSessionFromDisk();
        options->saveSessionToDisk(true);
        options->transportBar.setStatusText("Saved project as: " + options->projectSession.getManifest().projectName);
    }), true);
}

void MainComponent::createProjectFromTemplate()
{
    guardUnsavedProjectChange("creating a project from a template", [this] { beginCreateProjectFromTemplate(); });
}

void MainComponent::beginCreateProjectFromTemplate()
{
    if (! ensureStorageRootConfigured())
        return;

    // Templates are just the reusable session.xml starting point (tracks, etc.) a project would
    // otherwise start empty with -- never a full packed project copy, so a plain XML file is the
    // honest format now that there's no single container file to snapshot. Browsing a real
    // template file via a native picker is a legitimate asset-import case, not a "pick a thing
    // the framework already catalogs" case.
    projectChooser = std::make_unique<juce::FileChooser>("Create a project from a Djehuti Station template",
                                                         creation::suite::getTemplatesDirectory(suiteSettings),
                                                         "*.xml",
                                                         true);

    auto chooser = projectChooser.get();
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this, chooser](const juce::FileChooser& result)
                         {
                             auto templateFile = result.getResult();
                             if (! templateFile.existsAsFile())
                                 return;

                             auto* nameEditor = new juce::AlertWindow("New Project From Template",
                                                                      "Give the new project a name.",
                                                                      juce::MessageBoxIconType::QuestionIcon);
                             nameEditor->addTextEditor("projectName", templateFile.getFileNameWithoutExtension());
                             nameEditor->addButton("Create", 1);
                             nameEditor->addButton("Cancel", 0);

                             auto options = juce::Component::SafePointer<MainComponent>(this);
                             nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor, templateFile](int modalResult) mutable
                             {
                                 std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
                                 if (modalResult != 1 || options == nullptr)
                                     return;

                                 auto projectName = dialog->getTextEditorContents("projectName").trim();
                                 juce::String errorMessage;
                                 if (! creation::assets::ProjectWorkspaceService::createProject(options->suiteSettings,
                                                                                                 creation::assets::SuiteAppDomain::station,
                                                                                                 projectName, "1.0.0", "1.0.0",
                                                                                                 options->projectSession, errorMessage))
                                 {
                                     juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                                            "Template Error",
                                                                            errorMessage);
                                     return;
                                 }

                                 // The template file IS the session.xml payload directly (see
                                 // beginCreateProjectFromTemplate's comment above).
                                 auto templateXml = templateFile.loadFileAsString();
                                 if (templateXml.isNotEmpty())
                                 {
                                     juce::MemoryBlock sessionData(templateXml.toRawUTF8(), templateXml.getNumBytesAsUTF8());
                                     options->projectSession.writeEntry("session.xml", sessionData);
                                     options->projectSession.commit(errorMessage);
                                 }

                                 options->transportBar.setProjectLabel("Project: " + options->projectSession.getManifest().projectName);
                                 options->settingsPanel.setProjectMetadata(options->projectSession.getManifest());
                                 options->refreshProjectAssets();
                                 options->loadSessionFromDisk();
                                 options->saveSessionToDisk(true);
                                 options->transportBar.setStatusText("Project created from template.");
                             }), true);
                         });
}

void MainComponent::saveProjectAsTemplate()
{
    if (! projectSession.isValid())
    {
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::InfoIcon,
                                               "Template Needs A Project",
                                               "Create or open a project first, then save it as a template.");
        return;
    }

    auto* nameEditor = new juce::AlertWindow("Save Project As Template",
                                             "Name this reusable studio setup.",
                                             juce::MessageBoxIconType::QuestionIcon);
    nameEditor->addTextEditor("templateName", projectSession.getManifest().projectName + " Template");
    nameEditor->addButton("Save Template", 1);
    nameEditor->addButton("Cancel", 0);

    auto options = juce::Component::SafePointer<MainComponent>(this);
    nameEditor->enterModalState(true, juce::ModalCallbackFunction::create([options, nameEditor](int result) mutable
    {
        std::unique_ptr<juce::AlertWindow> dialog(nameEditor);
        if (result != 1 || options == nullptr)
            return;

        auto templateName = dialog->getTextEditorContents("templateName").trim();
        juce::File templatesDir = creation::suite::getTemplatesDirectory(options->suiteSettings);
        templatesDir.createDirectory();
        juce::File templateFile = templatesDir.getChildFile(templateName + ".xml");

        juce::MemoryBlock sessionData;
        if (! options->projectSession.readEntry("session.xml", sessionData)
            || ! templateFile.replaceWithData(sessionData.getData(), sessionData.getSize()))
        {
            juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                   "Template Error",
                                                   "Could not create template file.");
            return;
        }

        options->transportBar.setStatusText("Saved template: " + templateFile.getFileName());
    }), true);
}

void MainComponent::guardUnsavedProjectChange(const juce::String& actionName, const std::function<void()>& action)
{
    juce::ignoreUnused(actionName);

    if (! projectDirty)
    {
        if (action)
            action();
        return;
    }

    saveSessionToDisk(true);

    if (! projectDirty && action)
        action();
}

juce::String MainComponent::createRecordingTakeName() const
{
    return "Take-" + makeRecordingTimestamp() + ".wav";
}

void MainComponent::refreshRecentTakes()
{
    if (! projectSession.isValid())
    {
        recordView.setRecentTakes({});
        refreshProjectAssets();
        return;
    }

    juce::StringArray names;
    auto projectAssets = projectSession.getManifest().assetCatalog.query({});
    for (const auto& asset : projectAssets)
    {
        if (asset.kind != creation::assets::AssetKind::audio
            && asset.kind != creation::assets::AssetKind::render)
            continue;

        auto fileName = asset.logicalPath.fromLastOccurrenceOf("/", false, false);
        if (fileName.isNotEmpty())
            names.add(fileName);
        if (names.size() >= 10)
            break;
    }

    recordView.setRecentTakes(names);
    contentPanel.setProjectAssets(projectAssets);
}

bool MainComponent::startRecordingSession()
{
    if (! ensureStorageRootConfigured())
        return false;

    juce::String recProjectError;
    if (! ensureProjectSessionActive(recProjectError))
    {
        transportBar.setStatusText(recProjectError.isNotEmpty() ? recProjectError : "Could not initialize project for recording.");
        return false;
    }

    if (engine.getTrackCount() == 0)
        addTrack();

    activeRecordingTrack = trackerPanel.getSelectedTrack();
    if (! juce::isPositiveAndBelow(activeRecordingTrack, engine.getTrackCount()))
        activeRecordingTrack = 0;

    auto hasArmedTrack = false;
    for (auto armed : armedTracks)
        hasArmedTrack = hasArmedTrack || armed;

    if (! hasArmedTrack && juce::isPositiveAndBelow(activeRecordingTrack, (int) armedTracks.size()))
    {
        armedTracks[(size_t) activeRecordingTrack] = true;
        engine.setTrackRecordingArmed(activeRecordingTrack, true);
        trackerPanel.setTrackArmed(activeRecordingTrack, true);
    }

    juce::Array<WorkstationAudioEngine::RecordingTarget> recordingTargets;
    juce::Array<int> midiRecordingTracks;
    juce::StringArray tracksRejectedForRecording;
    const auto timestamp = makeRecordingTimestamp();

    for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
    {
        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) trackIndex])
            continue;

        if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::midi)
        {
            midiRecordingTracks.add(trackIndex);
            continue;
        }

        if (! cs::canTrackContainClip(timelineModel.getTrackKind(trackIndex), cs::ClipKind::audio))
        {
            tracksRejectedForRecording.add(engine.getTrackName(trackIndex));
            continue;
        }

        auto trackName = engine.getTrackName(trackIndex).retainCharacters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        trackName = trackName.trim().replace(" ", "-");

        if (trackName.isEmpty())
            trackName = "Track-" + juce::String(trackIndex + 1).paddedLeft('0', 2);

        WorkstationAudioEngine::RecordingTarget target;
        target.trackIndex = trackIndex;
        target.file = juce::File::getCurrentWorkingDirectory().getChildFile("Take-" + timestamp
                                                                                    + "-T" + juce::String(trackIndex + 1).paddedLeft('0', 2)
                                                                                    + "-" + trackName
                                                                                    + ".wav");
        recordingTargets.add(target);
    }

    if (! recordingTargets.isEmpty())
    {
        juce::String errorMessage;
        if (! engine.startRecordingToFiles(recordingTargets, errorMessage))
        {
            transportBar.setStatusText("Record failed: " + errorMessage);
            return false;
        }

        for (const auto& target : recordingTargets)
            timelineModel.beginRecordingClip(target.trackIndex, target.file);
    }

    if (! midiRecordingTracks.isEmpty())
    {
        engine.startMidiRecording();
        for (auto trackIndex : midiRecordingTracks)
            timelineModel.beginRecordingMidiClip(trackIndex);
    }

    const auto totalArmedCount = recordingTargets.size() + midiRecordingTracks.size();
    if (totalArmedCount == 0 && !videoCaptureService.isRecording())
    {
        transportBar.setStatusText(tracksRejectedForRecording.isEmpty()
            ? "Record failed: no armed tracks were available for recording."
            : "Record failed: " + tracksRejectedForRecording.joinIntoString(", ")
                  + " cannot hold an audio recording (wrong track type).");
        return false;
    }

    trackerPanel.refreshTimelineView();
    transportBar.setStatusText(tracksRejectedForRecording.isEmpty()
        ? "Recording " + juce::String(totalArmedCount) + " track(s)."
        : "Recording " + juce::String(totalArmedCount) + " track(s) -- skipped "
              + tracksRejectedForRecording.joinIntoString(", ") + " (wrong track type for audio).");
    recordView.setRecordingState(true, totalArmedCount == 1
                                            ? (recordingTargets.size() == 1 ? recordingTargets[0].file.getFileName()
                                                                             : juce::String("MIDI"))
                                            : juce::String(totalArmedCount) + " tracks");
    refreshRecentTakes();
    return true;
}

void MainComponent::stopRecordingSession()
{
    const auto wasMidiRecording = engine.isMidiRecording();
    if (! engine.isRecording() && ! wasMidiRecording)
        return;

    auto takeFiles = engine.getRecordingFiles();
    engine.stopRecording();

    auto midiTrackCount = 0;

    if (wasMidiRecording)
    {
        engine.stopMidiRecording();
        auto recordedEvents = engine.takeRecordedMidiEvents();
        const auto engineSampleRate = engine.getSampleRate();

        juce::Array<int> recordedTrackIndices;
        for (const auto& event : recordedEvents)
            recordedTrackIndices.addIfNotAlreadyThere(event.trackIndex);

        for (auto trackIndex : recordedTrackIndices)
        {
            std::vector<WorkstationAudioEngine::RecordedMidiEvent> trackEvents;
            for (const auto& event : recordedEvents)
                if (event.trackIndex == trackIndex)
                    trackEvents.push_back(event);

            std::sort(trackEvents.begin(), trackEvents.end(),
                      [](const auto& a, const auto& b) { return a.samplePosition < b.samplePosition; });

            // Pair each note-on with the next note-off on the same channel/pitch, converting the
            // engine's absolute recording-clock sample position into clip-relative beats.
            std::vector<cs::MidiNoteEvent> notes;
            struct OpenNote { int channel; int pitch; size_t noteIndex; };
            std::vector<OpenNote> openNotes;

            for (const auto& event : trackEvents)
            {
                const auto& message = event.message;
                const auto eventSeconds = engineSampleRate > 0.0 ? (double) event.samplePosition / engineSampleRate : 0.0;
                const auto elapsedBeats = timelineModel.secondsToBeat(eventSeconds - transportStartTimelineSeconds);

                if (message.isNoteOn())
                {
                    cs::MidiNoteEvent note;
                    note.id = juce::Uuid().toString();
                    note.pitch = message.getNoteNumber();
                    note.velocity = message.getVelocity();
                    note.channel = message.getChannel();
                    note.startBeats = juce::jmax(0.0, elapsedBeats);
                    note.lengthBeats = 0.25;
                    notes.push_back(note);
                    openNotes.push_back({ note.channel, note.pitch, notes.size() - 1 });
                }
                else if (message.isNoteOff())
                {
                    for (auto it = openNotes.begin(); it != openNotes.end(); ++it)
                    {
                        if (it->channel == message.getChannel() && it->pitch == message.getNoteNumber())
                        {
                            auto& note = notes[it->noteIndex];
                            note.lengthBeats = juce::jmax(0.05, elapsedBeats - note.startBeats);
                            openNotes.erase(it);
                            break;
                        }
                    }
                }
            }

            timelineModel.setRecordingClipMidiNotes(trackIndex, std::move(notes));
        }

        midiTrackCount = recordedTrackIndices.size();
    }

    juce::StringArray recordingSaveErrors;

    for (const auto& takeFile : takeFiles)
    {
        // A take is only a name here; its audio was recorded into memory and is handed over now.
        const auto takeData = engine.takeFinishedRecording(takeFile);
        if (takeData == nullptr)
            continue;

        juce::String importError;
        if (! ensureProjectSessionActive(importError))
        {
            recordingSaveErrors.add((importError.isNotEmpty() ? importError
                                                                : "Could not initialize project to save the recorded take.")
                                     + " (" + takeFile.getFileName() + ")");
            continue;
        }

        auto logicalPath = creation::assets::ProjectContainerPaths::sourceAssetRoot
                         + takeFile.getFileName();

        const juce::MemoryBlock& fileData = *takeData;

        if (! projectSession.writeEntry(logicalPath, fileData, juce::Time::getCurrentTime()))
        {
            recordingSaveErrors.add("Recorded take could not be registered in the project library: " + takeFile.getFileName());
            continue;
        }

        creation::assets::AssetDescriptor importedAsset;
        importedAsset.id = "asset:" + juce::Uuid().toString();
        importedAsset.version = "1";
        importedAsset.versionId = importedAsset.id + "@1";
        importedAsset.displayName = takeFile.getFileNameWithoutExtension().replace("-", " ");
        importedAsset.logicalPath = logicalPath;
        importedAsset.kind = creation::assets::AssetKind::audio;
        importedAsset.mediaType = "audio/wav";
        importedAsset.fileSizeBytes = (int64) fileData.getSize();
        importedAsset.createdAt = importedAsset.modifiedAt = juce::Time::getCurrentTime();
        importedAsset.sourceApp = "Djehuti Station";
        projectSession.upsertAssetDescriptor(importedAsset);

        if (! projectSession.commit(importError))
        {
            recordingSaveErrors.add((importError.isNotEmpty() ? importError : "Could not save the recorded take.")
                                     + " (" + takeFile.getFileName() + ")");
            continue;
        }

        cs::AssetRef assetRef;
        assetRef.id = importedAsset.id;
        assetRef.versionId = importedAsset.versionId;
        assetRef.mode = creation::assets::AssetReferenceMode::exact;
        assetRef.displayName = importedAsset.displayName;

        const auto& clips = timelineModel.getClips();
        for (int clipIndex = 0; clipIndex < static_cast<int>(clips.size()); ++clipIndex)
        {
            if (clips[(size_t) clipIndex].file != takeFile)
                continue;

            timelineModel.setClipAssetReference(clipIndex, assetRef);
            timelineModel.setClipFile(clipIndex, juce::File());
            juce::String waveformError;
            timelineModel.analyzeClipWaveformFromData(clipIndex, fileData, waveformError);
        }

    }

    timelineModel.finishRecordingClip(timelineModel.getTransportSeconds());
    activeRecordingTrack = -1;
    trackerPanel.refreshTimelineView();
    midiSurface.setTransportState(false, false);

    const auto totalTrackCount = takeFiles.size() + midiTrackCount;
    if (! recordingSaveErrors.isEmpty())
        transportBar.setStatusText("Recording stopped, but " + juce::String(recordingSaveErrors.size())
            + " take(s) failed to save to the project: " + recordingSaveErrors.joinIntoString(" | "));
    else
        transportBar.setStatusText("Recording stopped: " + juce::String(totalTrackCount) + " track(s).");
    recordView.setRecordingState(false, totalTrackCount == 1
                                             ? (takeFiles.size() == 1 ? takeFiles[0].getFileName() : juce::String("MIDI"))
                                             : juce::String(totalTrackCount) + " tracks");
    refreshRecentTakes();
    saveSessionToDisk();
}

void MainComponent::revealProjectFolder()
{
    if (! projectSession.isValid())
        return;

    // Projects live inside the suite VFS service's storage now, not a file this process can
    // point Explorer at directly -- the client intentionally never holds a real path (see
    // docs/architecture/Suite-Shared-Project-Model.md). Use the VFS Browser debug tool (suite
    // Settings -> VFS Browser, Debug builds only) to inspect what's on disk instead.
    transportBar.setStatusText("Projects are stored via the suite VFS service now, not a local folder to open directly.");
}

bool MainComponent::chooseStorageRoot(bool promptWhenAlreadyConfigured)
{
    juce::ignoreUnused(promptWhenAlreadyConfigured);
    suiteShellController.showSuiteSettings();
    transportBar.setStatusText("Configure Djehuti Station project storage in Djehuti Suite settings.");
    return false;
}

bool MainComponent::ensureStorageRootConfigured()
{
    if (suiteSettings.suiteVfsRoot.isNotEmpty())
        return true;

    transportBar.setStatusText("Configure Djehuti Station project storage in Djehuti Suite settings.");
    if (! chooseStorageRoot())
    {
        transportBar.setStatusText("Djehuti Station project storage is required before the studio can save projects or content.");
        return false;
    }

    return true;
}

bool MainComponent::ensureProjectSessionActive(juce::String& errorMessage)
{
    if (projectSession.isValid())
        return true;

    if (! ensureStorageRootConfigured())
    {
        errorMessage = "Djehuti Station project storage is not configured.";
        return false;
    }

    juce::String settingsError;
    auto settings = creation::services::SuiteVfsJsonStore::loadJson("station-settings.json", settingsError);
    if (auto* settingsObject = settings.getDynamicObject())
    {
        auto lastProjectId = settingsObject->getProperty("lastOpenedProjectId").toString();
        if (lastProjectId.isNotEmpty())
        {
            if (creation::assets::ProjectWorkspaceService::openProject(suiteSettings, lastProjectId, projectSession, errorMessage))
            {
                transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
                settingsPanel.setProjectMetadata(projectSession.getManifest());
                refreshProjectAssets();
                loadSessionFromDisk();
                return true;
            }
        }
    }

    juce::String listError;
    auto availableProjects = creation::assets::ProjectContainerService::listProjects(
        suiteSettings, listError);

    if (listError == "Could not reach the suite VFS service.")
    {
        errorMessage = listError;
        transportBar.setStatusText(listError);
        juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                               "Service Error",
                                               "Could not connect to the VFS service. Check if DjehutiSuiteVfsService is installed correctly.");
        return false;
    }

    if (! availableProjects.isEmpty())
    {
        if (creation::assets::ProjectWorkspaceService::openProject(
                suiteSettings, availableProjects.getFirst().projectId, projectSession, errorMessage))
        {
            transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
            settingsPanel.setProjectMetadata(projectSession.getManifest());
            refreshProjectAssets();
            loadSessionFromDisk();
            return true;
        }
    }

    beginCreateNewProject();
    return false;
}



void MainComponent::saveAppSettings()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    auto* object = new juce::DynamicObject();
    object->setProperty("formatVersion", 1);
    object->setProperty("autoloadLastProject", autoloadLastProject);
    if (projectSession.isValid())
        object->setProperty("lastOpenedProjectId", projectSession.getProjectId());
    object->setProperty("audioSystem", selectedStudioAudioSystem);
    object->setProperty("audioInputDevice", selectedStudioInputDevice);
    object->setProperty("audioOutputDevice", selectedStudioOutputDevice);

    // StudioIOModel's own state shape is a ValueTree (createState()/restoreState(), also used
    // elsewhere for undo snapshots) -- bridged here as an XML string rather than rewriting that
    // model's serialization, since the fix this migration makes is *where* the bytes live (VFS,
    // not a raw OS file), not what format each nested piece of state already uses.
    if (auto studioIoXml = studioIOModel.createState().createXml())
        object->setProperty("studioIO", studioIoXml->toString());

    juce::Array<juce::var> vstPaths;
    for (const auto& path : vstPluginCatalog.getSearchPaths())
        vstPaths.add(path);
    object->setProperty("vstSearchPaths", vstPaths);

    juce::Array<juce::var> disabledMidi;
    for (const auto& deviceId : disabledMidiInputDeviceIds)
        disabledMidi.add(deviceId);
    object->setProperty("disabledMidiInputDevices", disabledMidi);

    juce::String errorMessage;
    creation::services::SuiteVfsJsonStore::saveJson("station-settings.json", juce::var(object), errorMessage);
}

void MainComponent::loadAppSettings()
{
    juce::String errorMessage;
    auto parsed = creation::services::SuiteVfsJsonStore::loadJson("station-settings.json", errorMessage);
    auto* state = parsed.getDynamicObject();
    if (state == nullptr)
    {
        autoloadLastProject = true;
        loadSuiteAiProviderSettings();
        vstPluginCatalog.setSearchPaths(juce::StringArray());
        engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
        return;
    }

    selectedStudioAudioSystem = state->getProperty("audioSystem").toString();
    selectedStudioInputDevice = state->getProperty("audioInputDevice").toString();
    selectedStudioOutputDevice = state->getProperty("audioOutputDevice").toString();
    autoloadLastProject = true;

    if (auto studioIoXmlText = state->getProperty("studioIO").toString(); studioIoXmlText.isNotEmpty())
        if (auto studioIoXml = juce::parseXML(studioIoXmlText))
            studioIOModel.restoreState(juce::ValueTree::fromXml(*studioIoXml));

    juce::StringArray vstPaths;
    if (const auto* vstPathsArray = state->getProperty("vstSearchPaths").getArray())
        for (const auto& path : *vstPathsArray)
            vstPaths.add(path.toString());
    vstPaths.trim();
    vstPaths.removeEmptyStrings();
    vstPaths.removeDuplicates(false);
    vstPluginCatalog.setSearchPaths(vstPaths);

    disabledMidiInputDeviceIds.clear();
    if (const auto* disabledMidiArray = state->getProperty("disabledMidiInputDevices").getArray())
        for (const auto& deviceId : *disabledMidiArray)
            disabledMidiInputDeviceIds.add(deviceId.toString());

    loadSuiteAiProviderSettings();
    engine.setMetronomeTempo(timelineModel.getTempoBpm(), timelineModel.getTimeSignatureNumerator());
}

void MainComponent::applySelectedAudioDeviceSettings()
{
    if (selectedStudioAudioSystem.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType(selectedStudioAudioSystem, true);

    // engine.attachToDevice() auto-enables every non-control-surface MIDI input device as a
    // friendly default (so a freshly plugged-in keyboard just works) - apply the user's saved
    // exceptions on top of that default rather than replacing it, so newly connected devices
    // keep working out of the box while explicit opt-outs still stick across restarts.
    for (const auto& deviceId : disabledMidiInputDeviceIds)
        deviceManager.setMidiInputDeviceEnabled(deviceId, false);
    refreshMidiDeviceSettings();

    if (selectedStudioInputDevice.isEmpty() && selectedStudioOutputDevice.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (selectedStudioInputDevice.isNotEmpty())
    {
        setup.inputDeviceName = selectedStudioInputDevice;
        setup.useDefaultInputChannels = false;
        setup.inputChannels.clear();
        for (int channel = 0; channel < 32; ++channel)
            setup.inputChannels.setBit(channel);
    }

    if (selectedStudioOutputDevice.isNotEmpty())
    {
        setup.outputDeviceName = selectedStudioOutputDevice;
        setup.useDefaultOutputChannels = true;
    }

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
        transportBar.setStatusText("Audio restore: " + error);
}

void MainComponent::refreshMidiDeviceSettings()
{
    juce::Array<SettingsPanel::MidiDeviceInfo> devices;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        SettingsPanel::MidiDeviceInfo info;
        info.id = device.identifier;
        info.name = device.name;
        info.enabled = deviceManager.isMidiInputDeviceEnabled(device.identifier);
        info.routedTrackIndex = -1;

        for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
        {
            if (engine.getTrackMidiInputDeviceId(trackIndex) == device.identifier)
            {
                info.routedTrackIndex = trackIndex;
                break;
            }
        }

        devices.add(info);
    }

    juce::StringArray trackNames;
    for (int trackIndex = 0; trackIndex < engine.getTrackCount(); ++trackIndex)
        trackNames.add(engine.getTrackName(trackIndex));

    settingsPanel.setMidiInputDevices(devices, trackNames);
}

juce::ValueTree MainComponent::createProjectStateForSave()
{
    auto state = engine.createSessionState();
    state.setProperty("bankOffset", mixerPanel.getBankOffset(), nullptr);
    state.setProperty("insertContext", pluginRackBar.isTrackContext() ? "track" : "master", nullptr);
    state.setProperty("insertTrackIndex", pluginRackBar.getTrackIndex(), nullptr);
    state.setProperty("workspaceMode", static_cast<int>(activeMode), nullptr);
    state.setProperty("dslSource", dslPanel.getSourceText(), nullptr);
    state.setProperty("selectedClipIndex", selectedClipIndex, nullptr);
    state.addChild(scorePanel.createState(), -1, nullptr);
    state.addChild(timelineModel.createState(), -1, nullptr);
    auto& timelineUndoContext = undoService.getOrCreateContext(timelineUndoContextId, 100);
    state.addChild(timelineUndoContext.serialise("TimelineUndoContext"), -1, nullptr);
    auto& signalUndoContext = undoService.getOrCreateContext(signalUndoContextId, 100);
    state.addChild(signalUndoContext.serialise("SignalUndoContext"), -1, nullptr);

    juce::ValueTree lastActiveAssets("LastActiveAssets");
    lastActiveAssets.setProperty("trackerAssetId", currentArrangementAssetId, nullptr);
    lastActiveAssets.setProperty("signalLabAssetId", currentSignalLabAssetId, nullptr);
    lastActiveAssets.setProperty("foleyAssetId", currentFoleyAssetId, nullptr);
    state.addChild(lastActiveAssets, -1, nullptr);

    return state;
}

void MainComponent::saveSessionToDisk(bool userInitiated)
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    if (! projectSession.isValid())
    {
        if (userInitiated)
            transportBar.setStatusText("There is no active project to save yet.");
        return;
    }

    auto state = createProjectStateForSave();
    if (auto xml = state.createXml())
    {
        auto xmlString = xml->toString();
        juce::MemoryBlock xmlBlock(xmlString.toRawUTF8(), xmlString.getNumBytesAsUTF8());
        projectSession.writeEntry("session.xml", xmlBlock);
    }
    lastObservedPluginStateSignature = engine.createHostedPluginStateSignature();
    pluginStateAutosavePending = false;

    // Startup restore now uses the active project's session.xml. Remove legacy config snapshot.
    auto legacySessionFile = suiteSettingsStore.getSuiteConfigDirectory().getChildFile("session.xml");
    if (legacySessionFile.existsAsFile())
        legacySessionFile.deleteFile();

    juce::String packageError;
    if (! projectSession.commit(packageError))
    {
        projectDirty = true;
        transportBar.setStatusText("Project package save failed: " + packageError);
        return;
    }

    projectDirty = false;
}

bool MainComponent::prepareTrackerPlayback()
{
    // A video whose sound has not been extracted yet (imported before this existed, or on another machine):
    // do that first, in a progress window, and let the user press Play again once it is done.
    if (videoClipsNeedAudio() && prepareVideoAudio([this]
        {
            refreshTrackerPlaybackClips();
            showToast("The video's sound is ready. Press Play.");
        }))
    {
        return false;
    }

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lastClipEnd = 0.0;
    juce::String errorMessage;

    if (! buildTrackerPlaybackTargets(targets, lastClipEnd, errorMessage, false))
    {
        transportBar.setStatusText(errorMessage);
        return false;
    }

    if (! engine.setTrackerPlaybackClips(targets, errorMessage))
    {
        transportBar.setStatusText(errorMessage.isNotEmpty() ? errorMessage : "Could not prepare tracker playback.");
        return false;
    }

    juce::Array<WorkstationAudioEngine::SignalClipTarget> signalTargets;
    juce::String signalError;
    buildSignalClipTargets(signalTargets, signalError);
    engine.setTrackerSignalClips(signalTargets, signalError);
    if (signalError.isNotEmpty())
        transportBar.setStatusText(signalError);

    refreshMidiPlaybackClips();

    // Play always starts from wherever the playhead currently is - no auto-snap to the
    // first clip. (Previously this reset the transport position whenever it was at or past
    // the last clip's end, which silently discarded the user's chosen playhead position -
    // including immediately before recording, since onRecord also flows through here.)
    if (signalError.isEmpty())
        transportBar.setStatusText("Tracker playback ready: " + juce::String(targets.size() + signalTargets.size()) + " clip(s).");
    return true;
}

void MainComponent::refreshTrackerPlaybackClips()
{
    // Keeps the engine's cached tracker playback clip list in sync with timelineModel
    // immediately after any edit (delete/split/duplicate/move/undo/redo), not just when
    // Play is next pressed. Without this, a deleted clip's audio keeps being cached by the
    // engine and can still be heard - and captured into a new take - the next time
    // anything triggers playback, including pressing Record.
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> targets;
    double lastClipEnd = 0.0;
    juce::String errorMessage;
    buildTrackerPlaybackTargets(targets, lastClipEnd, errorMessage, false);

    juce::String engineError;
    engine.setTrackerPlaybackClips(targets, engineError);

    juce::Array<WorkstationAudioEngine::SignalClipTarget> signalTargets;
    juce::String signalError;
    buildSignalClipTargets(signalTargets, signalError);
    engine.setTrackerSignalClips(signalTargets, signalError);

    refreshMidiPlaybackClips();
}

void MainComponent::refreshMidiPlaybackClips()
{
    juce::Array<WorkstationAudioEngine::MidiPlaybackClip> midiClips;

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.kind != cs::ClipKind::midi || clip.midiNotes.empty())
            continue;

        WorkstationAudioEngine::MidiPlaybackClip midiClip;
        midiClip.trackIndex = clip.trackIndex;
        midiClip.startSeconds = clip.startSeconds;
        midiClip.durationSeconds = clip.durationSeconds;
        midiClip.notes = clip.midiNotes;
        midiClips.add(std::move(midiClip));
    }

    engine.setTrackerMidiClips(midiClips);
}

bool MainComponent::buildTrackerPlaybackTargets(juce::Array<WorkstationAudioEngine::PlaybackClipTarget>& targets,
                                                double& durationSeconds,
                                                juce::String& errorMessage,
                                                bool includeSignalClips)
{
    targets.clear();
    durationSeconds = 0.0;

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.recording)
            continue;

        if (clip.kind == cs::ClipKind::midi)
        {
            // Offline-rendering a MIDI clip means driving the instrument plugin through
            // hundreds of processBlock calls back-to-back with no real-time pacing. At least
            // one real-world plugin (a sample-streaming drum sampler) crashes reliably under
            // that load - confirmed by an identical crash signature to the earlier idle-audio
            // instability. Disabled until a safer rendering approach exists; see task #7.
            continue;
        }

        if (clip.kind == cs::ClipKind::signal && ! includeSignalClips)
        {
            // Run live by the engine instead (buildSignalClipTargets); it still sets how long the timeline is.
            durationSeconds = juce::jmax(durationSeconds, clip.startSeconds + clip.durationSeconds);
            continue;
        }

        std::shared_ptr<const juce::MemoryBlock> clipBytes;

        if (clip.kind == cs::ClipKind::video)
        {
            // The picture is not audio. The clip's sound plays from the WAV extracted from the video (see
            // prepareVideoAudio); until that exists the clip is simply silent.
            durationSeconds = juce::jmax(durationSeconds, clip.startSeconds + clip.durationSeconds);
            if (clip.soundDetached)
                continue; // its sound is a clip of its own now; playing it here too would double it

            const auto extracted = videoAudioBytes.find(clip.assetId);
            if (extracted == videoAudioBytes.end() || extracted->second == nullptr)
                continue;

            clipBytes = extracted->second;
        }
        else if (clip.kind == cs::ClipKind::signal)
        {
            // A Signal clip's source is a patch document (.cspatch), never playable audio. It is rendered to a WAV, kept
            // in the project VFS (never as a file on the disk) and played from memory.
            const auto cached = clip.assetId.isNotEmpty() ? signalRenderBytes.find(clip.assetId) : signalRenderBytes.end();
            if (cached != signalRenderBytes.end() && cached->second != nullptr)
            {
                clipBytes = cached->second;
            }
            else
            {
                // Always re-read the patch from the project: it may have been re-saved since the last render.
                juce::String patchText, matError;
                if (clip.assetId.isNotEmpty())
                {
                    const auto assetOpt = resolveTimelineClipAsset(clip);
                    juce::MemoryBlock patchData;
                    if (assetOpt.has_value() && projectSession.readEntry(assetOpt->logicalPath, patchData))
                        patchText = patchData.toString();
                }

                if (patchText.isEmpty())
                    continue;

                // Key the cache on the patch content, so editing the patch invalidates the cached render.
                const auto cacheKey = clip.assetId.isNotEmpty() ? clip.assetId : clip.displayName;
                const auto cachePath = "cache/signal_render_" + cacheKey.replaceCharacters(":\\/ ", "____")
                                     + "_" + juce::String::toHexString(patchText.hashCode64()) + ".wav";

                juce::MemoryBlock saved;
                if (projectSession.readEntry(cachePath, saved) && saved.getSize() > 0)
                {
                    clipBytes = std::make_shared<const juce::MemoryBlock>(std::move(saved));
                }
                else
                {
                    cw::PatchDocument doc;
                    if (! cw::parsePatchDocumentJson(patchText, doc, matError))
                    {
                        errorMessage = "Signal track render failed (parse): " + matError;
                        return false;
                    }

                    PatchRuntimePlayer player;
                    player.prepare(48000.0, 512);
                    juce::AudioBuffer<float> buffer;
                    if (! player.renderPatchToBuffer(doc, doc.durationSeconds > 0.0 ? doc.durationSeconds : 5.0, buffer, matError, nullptr))
                    {
                        errorMessage = "Signal track render failed (render): " + matError;
                        return false;
                    }

                    auto wavData = std::make_shared<juce::MemoryBlock>();
                    juce::WavAudioFormat wavFormat;
                    // The writer takes ownership of (and deletes) the stream, so it must be heap-allocated.
                    auto* wavStream = new juce::MemoryOutputStream(*wavData, false);
                    std::unique_ptr<juce::AudioFormatWriter> writer(
                        wavFormat.createWriterFor(wavStream, 48000.0, (unsigned int) buffer.getNumChannels(), 24, {}, 0));
                    if (writer == nullptr)
                    {
                        delete wavStream;
                        errorMessage = "Could not create a WAV writer for the rendered Signal clip.";
                        return false;
                    }
                    writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
                    writer.reset(); // Flushes the WAV header and finalizes wavData

                    if (! projectSession.writeEntry(cachePath, *wavData, juce::Time::getCurrentTime()))
                    {
                        errorMessage = "Could not write rendered WAV into VFS.";
                        return false;
                    }

                    clipBytes = wavData;
                }

                if (clip.assetId.isNotEmpty())
                    signalRenderBytes[clip.assetId] = clipBytes;
            }
        }
        else if (clip.assetId.isNotEmpty())
        {
            // An audio clip: the asset's bytes, read into memory through the VFS service.
            if (const auto assetOpt = resolveTimelineClipAsset(clip); assetOpt.has_value())
                clipBytes = readAssetBytes(assetOpt->logicalPath, assetOpt->versionId);
        }

        if (clipBytes == nullptr && ! clip.file.existsAsFile())
            continue;

        WorkstationAudioEngine::PlaybackClipTarget target;
        target.trackIndex = clip.trackIndex;
        target.file = clip.file;
        target.encodedData = clipBytes;
        target.displayName = clip.displayName;
        target.startSeconds = clip.startSeconds;
        target.sourceStartSeconds = clip.sourceStartSeconds;
        target.durationSeconds = clip.durationSeconds;
        targets.add(target);
        durationSeconds = juce::jmax(durationSeconds, clip.startSeconds + clip.durationSeconds);
    }

    if (targets.isEmpty() && durationSeconds <= 0.0)
    {
        errorMessage = "No recorded or rendered clips are available.";
        return false;
    }

    return true;
}

bool MainComponent::loadSignalClipPatch(const cs::TimelineClip& clip, cw::PatchDocument& patch, juce::String& patchKey, juce::String& errorMessage)
{
    auto cached = clip.assetId.isNotEmpty() ? signalPatchDocs.find(clip.assetId) : signalPatchDocs.end();
    if (cached != signalPatchDocs.end())
    {
        patchKey = cached->second.first;
        patch = cached->second.second;
        return true;
    }

    // The project's asset is the source of truth (clip.file is only a local copy made at
    // load and goes stale once the patch is re-saved); fall back to it for asset-less clips.
    juce::String patchText, matError;
    if (clip.assetId.isNotEmpty())
    {
        const auto assetOpt = resolveTimelineClipAsset(clip);
        juce::MemoryBlock patchData;
        if (assetOpt.has_value() && projectSession.readEntry(assetOpt->logicalPath, patchData))
            patchText = patchData.toString();
    }

    if (patchText.isEmpty() && clip.file.existsAsFile())
        patchText = clip.file.loadFileAsString();

    if (patchText.isEmpty())
        return false;

    if (! cw::parsePatchDocumentJson(patchText, patch, matError))
    {
        errorMessage = "Signal clip \"" + clip.displayName + "\" could not be read: " + matError;
        return false;
    }

    patchKey = juce::String::toHexString(patchText.hashCode64());
    if (clip.assetId.isNotEmpty())
        signalPatchDocs[clip.assetId] = { patchKey, patch };

    return true;
}

bool MainComponent::buildSignalClipTargets(juce::Array<WorkstationAudioEngine::SignalClipTarget>& targets,
                                           juce::String& errorMessage)
{
    targets.clear();

    for (const auto& clip : timelineModel.getClips())
    {
        if (clip.recording || clip.kind != cs::ClipKind::signal)
            continue;

        WorkstationAudioEngine::SignalClipTarget target;
        target.clipId = clip.id;
        target.trackIndex = clip.trackIndex;
        target.startSeconds = clip.startSeconds;
        target.sourceStartSeconds = clip.sourceStartSeconds;
        target.durationSeconds = clip.durationSeconds;

        if (! loadSignalClipPatch(clip, target.patch, target.patchKey, errorMessage))
            continue;

        targets.add(std::move(target));
    }

    return true;
}

void MainComponent::previewScrubAudioAt(double timelineSeconds)
{
    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> fullTargets;
    double fullDurationSeconds = 0.0;
    juce::String errorMessage;
    if (! buildTrackerPlaybackTargets(fullTargets, fullDurationSeconds, errorMessage))
        return;

    constexpr double scrubPreviewLeadSeconds = 0.03;
    constexpr double scrubPreviewLengthSeconds = 0.16;

    auto windowStart = juce::jmax(0.0, timelineSeconds - scrubPreviewLeadSeconds);
    auto windowEnd = windowStart + scrubPreviewLengthSeconds;

    juce::Array<WorkstationAudioEngine::PlaybackClipTarget> scrubTargets;
    for (const auto& target : fullTargets)
    {
        auto clipStart = target.startSeconds;
        auto clipEnd = target.startSeconds + target.durationSeconds;
        auto overlapStart = juce::jmax(clipStart, windowStart);
        auto overlapEnd = juce::jmin(clipEnd, windowEnd);

        if (overlapEnd <= overlapStart)
            continue;

        WorkstationAudioEngine::PlaybackClipTarget scrubTarget = target;
        scrubTarget.startSeconds = overlapStart - windowStart;
        scrubTarget.sourceStartSeconds = target.sourceStartSeconds + (overlapStart - clipStart);
        scrubTarget.durationSeconds = overlapEnd - overlapStart;
        scrubTargets.add(std::move(scrubTarget));
    }

    if (scrubTargets.isEmpty())
    {
        engine.stopAssetPreview();
        return;
    }

    WorkstationAudioEngine::RenderSettings settings;
    settings.sampleRate = engine.getSampleRate() > 0.0 ? engine.getSampleRate() : 48000.0;
    settings.blockSize = 256;
    settings.normalizePeak = false;

    juce::AudioBuffer<float> previewBuffer;
    if (! engine.renderTrackerMixToBuffer(scrubTargets, scrubPreviewLengthSeconds, settings, previewBuffer, errorMessage))
        return;

    engine.previewGeneratedBuffer(previewBuffer, settings.sampleRate, errorMessage);
}

void MainComponent::pushTimelineUndoState()
{
    pushTimelineUndoState(timelineModel.createState());
}

void MainComponent::pushTimelineUndoState(const juce::ValueTree& stateBeforeEdit)
{
    auto& timelineUndoContext = undoService.getOrCreateContext(timelineUndoContextId, 100);
    timelineUndoContext.pushUndoState(stateBeforeEdit, "Timeline edit");
    undoService.setActiveContext(timelineUndoContextId);
}

void MainComponent::restoreTimelineEditState(const juce::ValueTree& state, const juce::String& statusText)
{
    // Most undo entries (clip split/duplicate/delete/move) are a bare "Timeline" tree and only
    // need timelineModel restored, as before. Track-level operations (add/remove) push a wrapping
    // "UndoSnapshot" that also carries the engine's full session state (gain/pan/mute/solo/plugin
    // chains), since removing a track destroys engine-side state that timelineModel never held.
    if (state.hasType(juce::Identifier("UndoSnapshot")))
    {
        if (auto timelineState = state.getChildWithName("Timeline"); timelineState.isValid())
            timelineModel.restoreState(timelineState);

        if (auto engineState = state.getChildWithName("CreationStationSession"); engineState.isValid())
        {
            juce::String engineError;
            engine.restoreSessionState(engineState, engineError);
        }
    }
    else
    {
        timelineModel.restoreState(state);
    }

    transportBar.loopButton.setToggleState(timelineModel.isLoopEnabled(), juce::dontSendNotification);
    transportBar.loopDelaySlider.setValue(timelineModel.getLoopDelaySeconds(), juce::dontSendNotification);

    refreshTrackerPlaybackClips();
    selectedClipIndex = -1;
    syncTrackViews();
    trackerPanel.setSelectedClip(-1);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText(statusText);
}

void MainComponent::pushSignalUndoState(const juce::ValueTree& stateBeforeEdit, const juce::String& label)
{
    auto& signalUndoContext = undoService.getOrCreateContext(signalUndoContextId, 100);
    signalUndoContext.pushUndoState(stateBeforeEdit, label);
    undoService.setActiveContext(signalUndoContextId);
}

void MainComponent::restoreSignalEditState(const juce::ValueTree& state, const juce::String& statusText)
{
    signalLabPanel.restoreState(state);
    setWorkspaceMode(WorkspaceMode::signal);
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText(statusText);
}

void MainComponent::undoSignalEdit()
{
    auto& signalUndoContext = undoService.getOrCreateContext(signalUndoContextId, 100);
    juce::ValueTree stateToRestore;
    juce::String label;
    if (! signalUndoContext.undoTo(signalLabPanel.createState(), stateToRestore, label))
        return;

    undoService.setActiveContext(signalUndoContextId);
    restoreSignalEditState(stateToRestore, label.isNotEmpty() ? (label + " undone.") : "Signal Lab edit undone.");
}

void MainComponent::redoSignalEdit()
{
    auto& signalUndoContext = undoService.getOrCreateContext(signalUndoContextId, 100);
    juce::ValueTree stateToRestore;
    juce::String label;
    if (! signalUndoContext.redoTo(signalLabPanel.createState(), stateToRestore, label))
        return;

    undoService.setActiveContext(signalUndoContextId);
    restoreSignalEditState(stateToRestore, label.isNotEmpty() ? (label + " redone.") : "Signal Lab edit redone.");
}

void MainComponent::undoTimelineEdit()
{
    auto& timelineUndoContext = undoService.getOrCreateContext(timelineUndoContextId, 100);
    juce::ValueTree stateToRestore;
    juce::String label;
    if (! timelineUndoContext.undoTo(timelineModel.createState(), stateToRestore, label))
        return;

    undoService.setActiveContext(timelineUndoContextId);
    restoreTimelineEditState(stateToRestore, label.isNotEmpty() ? (label + " undone.") : "Tracker edit undone.");
}

void MainComponent::redoTimelineEdit()
{
    auto& timelineUndoContext = undoService.getOrCreateContext(timelineUndoContextId, 100);
    juce::ValueTree stateToRestore;
    juce::String label;
    if (! timelineUndoContext.redoTo(timelineModel.createState(), stateToRestore, label))
        return;

    undoService.setActiveContext(timelineUndoContextId);
    restoreTimelineEditState(stateToRestore, label.isNotEmpty() ? (label + " redone.") : "Tracker edit redone.");
}

void MainComponent::splitClipAt(int clipIndex, double splitSeconds)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.splitClip(clipIndex, splitSeconds))
    {
        transportBar.setStatusText("Split needs the playhead inside a clip.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = juce::jmin(clipIndex + 1, static_cast<int>(timelineModel.getClips().size()) - 1);
    trackerPanel.setSelectedClip(selectedClipIndex);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip split.");
}

void MainComponent::duplicateClip(int clipIndex)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.duplicateClip(clipIndex))
    {
        transportBar.setStatusText("No clip selected to duplicate.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = static_cast<int>(timelineModel.getClips().size()) - 1;
    trackerPanel.setSelectedClip(selectedClipIndex);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip duplicated.");
}

void MainComponent::deleteClip(int clipIndex)
{
    auto stateBeforeEdit = timelineModel.createState();
    if (! timelineModel.deleteClip(clipIndex))
    {
        transportBar.setStatusText("No clip selected to delete.");
        return;
    }

    pushTimelineUndoState(stateBeforeEdit);
    selectedClipIndex = -1;
    trackerPanel.setSelectedClip(-1);
    trackerPanel.refreshTimelineView();
    refreshTrackerPlaybackClips();
    projectDirty = true;
    saveSessionToDisk();
    transportBar.setStatusText("Clip deleted.");
}

void MainComponent::renameClip(int clipIndex)
{
    if (! juce::isPositiveAndBelow(clipIndex, static_cast<int>(timelineModel.getClips().size())))
    {
        transportBar.setStatusText("No clip selected to rename.");
        return;
    }

    const auto& clip = timelineModel.getClips()[(size_t) clipIndex];
    auto* renameDialog = new juce::AlertWindow("Rename Clip",
                                               "Give this clip a useful name.",
                                               juce::MessageBoxIconType::QuestionIcon);
    renameDialog->addTextEditor("clipName", clip.displayName);
    renameDialog->addButton("Rename", 1);
    renameDialog->addButton("Cancel", 0);

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    renameDialog->enterModalState(true,
                                  juce::ModalCallbackFunction::create([safeThis, renameDialog, clipIndex](int result) mutable
                                  {
                                      std::unique_ptr<juce::AlertWindow> dialog(renameDialog);
                                      if (result != 1 || safeThis == nullptr)
                                          return;

                                      auto newName = dialog->getTextEditorContents("clipName").trim();
                                      if (newName.isEmpty())
                                          return;

                                      auto stateBeforeEdit = safeThis->timelineModel.createState();
                                      const auto clip = safeThis->timelineModel.getClips()[(size_t) clipIndex];
                                      safeThis->timelineModel.setClipDisplayName(clipIndex, newName);

                                      if (safeThis->projectSession.isValid() && clip.assetId.isNotEmpty())
                                      {
                                          auto& assets = safeThis->projectSession.getManifest().assetCatalog.assets;
                                          for (auto& asset : assets)
                                          {
                                              if (asset.id == clip.assetId)
                                              {
                                                  asset.displayName = newName;
                                                  asset.modifiedAt = juce::Time::getCurrentTime();
                                                  ++asset.revision;
                                                  break;
                                              }
                                          }

                                          juce::String assetError;
                                          if (! safeThis->projectSession.commit(assetError))
                                              safeThis->transportBar.setStatusText(assetError.isNotEmpty() ? assetError : "Clip renamed, but project asset metadata could not be saved.");
                                      }

                                      safeThis->pushTimelineUndoState(stateBeforeEdit);
                                      safeThis->selectedClipIndex = clipIndex;
                                      safeThis->trackerPanel.setSelectedClip(clipIndex);
                                      safeThis->trackerPanel.refreshTimelineView();
                                      safeThis->refreshProjectAssets();
                                      safeThis->projectDirty = true;
                                      safeThis->saveSessionToDisk();
                                      safeThis->transportBar.setStatusText("Clip renamed.");
                                  }),
                                  true);
}

bool MainComponent::keyPressed(const juce::KeyPress& key)
{
    return handleGlobalKeyPress(key);
}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component*)
{
    return handleGlobalKeyPress(key);
}

void MainComponent::parentHierarchyChanged()
{
    // Register as a key listener on the top-level window so app shortcuts (undo/redo/etc.) fire
    // regardless of which child currently holds keyboard focus - or when the previously focused
    // component (e.g. a just-removed track header) has been destroyed and nothing holds focus.
    if (auto* top = getTopLevelComponent(); top != nullptr && top != this)
    {
        top->removeKeyListener(this);
        top->addKeyListener(this);
    }
}

bool MainComponent::handleGlobalKeyPress(const juce::KeyPress& key)
{
    auto mods = key.getModifiers();
    auto code = key.getKeyCode();

    // JUCE returns the *uppercase* letter for alphabetic keys from getKeyCode(); normalise so the
    // shortcuts fire regardless of case (comparing against lowercase 'z' alone silently never matched).
    auto letter = (juce::juce_wchar) juce::CharacterFunctions::toUpperCase((juce::juce_wchar) code);

    if (key == juce::KeyPress::F1Key)
    {
        showHelpWindow();
        return true;
    }

    if (mods.isCommandDown() && ! mods.isShiftDown() && letter == 'Z')
    {
        if (activeMode == WorkspaceMode::signal)
            undoSignalEdit();
        else
            undoTimelineEdit();
        return true;
    }

    if ((mods.isCommandDown() && ! mods.isShiftDown() && letter == 'Y')
        || (mods.isCommandDown() && mods.isShiftDown() && letter == 'Z'))
    {
        if (activeMode == WorkspaceMode::signal)
            redoSignalEdit();
        else
            redoTimelineEdit();
        return true;
    }

    if (mods.isCommandDown() && ! mods.isShiftDown() && letter == 'D' && selectedClipIndex >= 0)
    {
        duplicateClip(selectedClipIndex);
        return true;
    }

    if ((code == juce::KeyPress::deleteKey || code == juce::KeyPress::backspaceKey) && selectedClipIndex >= 0)
    {
        deleteClip(selectedClipIndex);
        return true;
    }

    if (code == juce::KeyPress::F2Key && selectedClipIndex >= 0)
    {
        renameClip(selectedClipIndex);
        return true;
    }

    if (mods.isCommandDown() && mods.isShiftDown() && letter == 'S' && selectedClipIndex >= 0)
    {
        splitClipAt(selectedClipIndex, timelineModel.getTransportSeconds());
        return true;
    }

    return false;
}

void MainComponent::loadSessionFromDisk()
{
    if (! suiteSettings.suiteVfsRoot.isNotEmpty())
        return;

    juce::MemoryBlock sessionData;
    juce::ValueTree state;
    if (projectSession.isValid() && projectSession.readEntry("session.xml", sessionData))
    {
        state = juce::ValueTree::fromXml(juce::String::createStringFromData(sessionData.getData(), (int)sessionData.getSize()));
    }
    if (! state.isValid())
        return;

    loadAppSettings();
    applySelectedAudioDeviceSettings();

    juce::String errorMessage;
    engine.restoreSessionState(state, errorMessage);

    if (auto timelineState = state.getChildWithName("Timeline"); timelineState.isValid())
        timelineModel.restoreState(timelineState);

    markArrangementClean();

    transportBar.loopButton.setToggleState(timelineModel.isLoopEnabled(), juce::dontSendNotification);
    transportBar.loopDelaySlider.setValue(timelineModel.getLoopDelaySeconds(), juce::dontSendNotification);

    auto& timelineUndoContext = undoService.getOrCreateContext(timelineUndoContextId, 100);
    timelineUndoContext.clear();
    if (auto undoState = state.getChildWithName("TimelineUndoContext"); undoState.isValid())
        timelineUndoContext.restore(undoState);
    else
    {
        juce::ValueTree legacyUndoState("TimelineUndoContext");
        legacyUndoState.setProperty("contextId", timelineUndoContextId, nullptr);
        legacyUndoState.setProperty("maxEntries", 100, nullptr);
        juce::ValueTree undoStackState("UndoStack");
        if (auto oldUndoState = state.getChildWithName("TimelineUndoStack"); oldUndoState.isValid())
        {
            for (const auto child : oldUndoState)
            {
                auto entry = child.createCopy();
                entry.setProperty("__undoLabel", "Timeline edit", nullptr);
                undoStackState.addChild(entry, -1, nullptr);
            }
        }
        legacyUndoState.addChild(undoStackState, -1, nullptr);
        juce::ValueTree redoStackState("RedoStack");
        if (auto oldRedoState = state.getChildWithName("TimelineRedoStack"); oldRedoState.isValid())
        {
            for (const auto child : oldRedoState)
            {
                auto entry = child.createCopy();
                entry.setProperty("__undoLabel", "Timeline edit", nullptr);
                redoStackState.addChild(entry, -1, nullptr);
            }
        }
        legacyUndoState.addChild(redoStackState, -1, nullptr);
        timelineUndoContext.restore(legacyUndoState);
    }
    undoService.setActiveContext(timelineUndoContextId);

    auto& signalUndoContext = undoService.getOrCreateContext(signalUndoContextId, 100);
    signalUndoContext.clear();
    if (auto signalUndoState = state.getChildWithName("SignalUndoContext"); signalUndoState.isValid())
        signalUndoContext.restore(signalUndoState);

    auto bankOffset = (int) state.getProperty("bankOffset", 0);
    mixerPanel.setBankOffset(bankOffset);
    midiSurface.setBankOffset(bankOffset);

    auto insertContext = state.getProperty("insertContext").toString();
    auto trackIndex = (int) state.getProperty("insertTrackIndex", -1);

    if (insertContext == "track" && juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
    }
    else
    {
        pluginRackBar.setContextMaster();
        mixerPanel.setSelectedChannel(-1);
    }


    if (auto dslSource = state.getProperty("dslSource").toString(); dslSource.isNotEmpty())
        dslPanel.setSourceText(dslSource);

    refreshProjectAssets();
    restoreLastActiveAssets(state.getChildWithName("LastActiveAssets"));

    if (auto scoreState = state.getChildWithName("ScoreView"); scoreState.isValid())
        scorePanel.restoreState(scoreState);

    syncTrackViews();

    selectedClipIndex = (int) state.getProperty("selectedClipIndex", -1);
    if (! juce::isPositiveAndBelow(selectedClipIndex, static_cast<int>(timelineModel.getClips().size())))
        selectedClipIndex = -1;
    trackerPanel.setSelectedClip(selectedClipIndex);

    armedTracks.resize((size_t) engine.getTrackCount(), false);
    monitoredTracks.resize((size_t) engine.getTrackCount(), false);
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        armedTracks[(size_t) index] = engine.isTrackRecordingArmed(index);
        monitoredTracks[(size_t) index] = engine.isTrackMonitoringEnabled(index);
        trackerPanel.setTrackArmed(index, armedTracks[(size_t) index]);
        trackerPanel.setTrackMonitored(index, monitoredTracks[(size_t) index]);
    }

    recordView.setTrackCount(engine.getTrackCount());
    for (int index = 0; index < engine.getTrackCount(); ++index)
    {
        auto trackName = engine.getTrackName(index);
        if (index < engine.getTrackCount())
            recordView.setTrackName(index, trackName);
    }

    auto savedMode = (int) state.getProperty("workspaceMode", static_cast<int>(WorkspaceMode::tracker));
    if (savedMode > static_cast<int>(WorkspaceMode::sampler))
        savedMode = static_cast<int>(WorkspaceMode::tracker);

    setWorkspaceMode(static_cast<WorkspaceMode>(juce::jlimit(0, static_cast<int>(WorkspaceMode::sampler), savedMode)));

    refreshInsertRack();
    transportBar.setProjectLabel("Project: " + projectSession.getManifest().projectName);
    refreshRecentTakes();

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::Timer::callAfterDelay(300, [safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->engine.reapplyHostedPluginStates();
        safeThis->lastObservedPluginStateSignature = safeThis->engine.createHostedPluginStateSignature();
    });

    lastObservedPluginStateSignature = engine.createHostedPluginStateSignature();
    pluginStateAutosavePending = false;
    pluginStateLastPollWallSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    projectDirty = false;

}

void MainComponent::syncTrackViews()
{
    auto trackCount = engine.getTrackCount();

    refreshTrackInputSources();
    trackerPanel.setTimingInfo(timelineModel.getTempoBpm(),
                               timelineModel.getTimeSignatureNumerator(),
                               timelineModel.getTimeSignatureDenominator(),
                               timelineModel.getMusicalKey());
    timelineModel.setTrackCount(trackCount);
    trackerPanel.setTrackCount(trackCount);
    recordView.setTrackCount(trackCount);
    midiSurface.setTrackCount(trackCount);
    mixerPanel.setChannelCount(trackCount);

    if ((int) armedTracks.size() < trackCount)
        armedTracks.resize((size_t) trackCount, false);
    else if ((int) armedTracks.size() > trackCount)
        armedTracks.resize((size_t) trackCount);
    if ((int) automationLastManualWriteWallSeconds.size() < trackCount)
        automationLastManualWriteWallSeconds.resize((size_t) trackCount, 0.0);
    else if ((int) automationLastManualWriteWallSeconds.size() > trackCount)
        automationLastManualWriteWallSeconds.resize((size_t) trackCount);
    if ((int) monitoredTracks.size() < trackCount)
        monitoredTracks.resize((size_t) trackCount, false);
    else if ((int) monitoredTracks.size() > trackCount)
        monitoredTracks.resize((size_t) trackCount);

    for (int index = 0; index < trackCount; ++index)
    {
        auto trackName = engine.getTrackName(index);
        timelineModel.setTrackName(index, trackName);
        timelineModel.setTrackChannelMode(index, engine.isTrackStereoEnabled(index) ? cs::TrackChannelMode::stereo
                                                                                    : cs::TrackChannelMode::mono);
        trackerPanel.setTrackName(index, trackName);
        trackerPanel.setTrackKind(index, timelineModel.getTrackKind(index));
        engine.setTrackIsMidiKind(index, timelineModel.getTrackKind(index) == cs::TrackKind::midi);
        engine.setTrackIsAutomationKind(index, timelineModel.getTrackKind(index) == cs::TrackKind::automation);
        if (timelineModel.getTrackKind(index) == cs::TrackKind::automation)
            pushAutomationDataToEngine(index);
        engine.setTrackParentIndex(index, timelineModel.getTrackParent(index));
        trackerPanel.setTrackIndented(index, timelineModel.getTrackParent(index) >= 0);
        trackerPanel.setTrackAccentColour(index, computeTrackAccentColour(index));
        trackerPanel.setTrackStereo(index, engine.isTrackStereoEnabled(index));
        recordView.setTrackName(index, trackName);
        mixerPanel.setChannelName(index, trackName);
        midiSurface.setChannelName(index, trackName);
        trackerPanel.setTrackGain(index, engine.getTrackGain(index));
        trackerPanel.setTrackMuted(index, engine.isTrackMuted(index));
        trackerPanel.setTrackSoloed(index, engine.isTrackSoloed(index));
        trackerPanel.setTrackArmed(index, juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index]);
        trackerPanel.setTrackMonitored(index, juce::isPositiveAndBelow(index, (int) monitoredTracks.size()) && monitoredTracks[(size_t) index]);
        trackerPanel.setTrackInput(index, timelineModel.getTrackKind(index) == cs::TrackKind::midi
                                              ? engine.getTrackMidiInputChannel(index)
                                              : studioIOModel.getInputIndexForChannel(engine.getTrackInputChannel(index)));
        trackerPanel.setTrackFxSummary(index, engine.getTrackPluginCount(index));
        engine.setTrackRecordingArmed(index, juce::isPositiveAndBelow(index, (int) armedTracks.size()) && armedTracks[(size_t) index]);
        engine.setTrackMonitoringEnabled(index, juce::isPositiveAndBelow(index, (int) monitoredTracks.size()) && monitoredTracks[(size_t) index]);
        trackerPanel.setTrackLevel(index, engine.getTrackLevel(index));

        if (timelineModel.getTrackKind(index) == cs::TrackKind::automation)
        {
            auto target = timelineModel.getAutomationTarget(index);
            trackerPanel.setAutomationTargetLabel(index, target.displayName);
            trackerPanel.setAutomationRecordMode(index, timelineModel.getAutomationRecordMode(index));
            trackerPanel.setAutomationRecordingRate(index, timelineModel.getAutomationRecordingRate(index));
        }
    }
}

void MainComponent::pushAutomationDataToEngine(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto target = timelineModel.getAutomationTarget(trackIndex);

    std::vector<cs::AutomationPoint> points;
    if (auto* stored = timelineModel.getAutomationPoints(trackIndex))
        points = *stored;

    engine.setTrackAutomationData(trackIndex, target, points);
}

void MainComponent::recordAutomationWriteIfArmed(int targetTrackIndex, cs::AutomationTargetKind kind, float normalizedValue)
{
    if (! engine.isPlaying())
        return;

    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int automationTrackIndex = 0; automationTrackIndex < timelineModel.getTrackCount(); ++automationTrackIndex)
    {
        if (timelineModel.getTrackKind(automationTrackIndex) != cs::TrackKind::automation)
            continue;

        if (! juce::isPositiveAndBelow(automationTrackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) automationTrackIndex])
            continue;

        auto target = timelineModel.getAutomationTarget(automationTrackIndex);
        if (target.kind != kind || target.targetTrackIndex != targetTrackIndex)
            continue;

        // Suspend this lane's own playback of its (about-to-be-overwritten) curve for as long as
        // it's being actively written - otherwise the audio-thread automation pass and this
        // manual write fight over the same value every block.
        engine.setTrackAutomationWriteActive(automationTrackIndex, true);
        if (juce::isPositiveAndBelow(automationTrackIndex, (int) automationLastManualWriteWallSeconds.size()))
            automationLastManualWriteWallSeconds[(size_t) automationTrackIndex] = nowSeconds;

        auto mergeToleranceSeconds = 1.0 / juce::jmax(1, timelineModel.getAutomationRecordingRate(automationTrackIndex));
        timelineModel.addOrUpdateAutomationPoint(automationTrackIndex, timelineModel.getTransportSeconds(),
                                                 juce::jlimit(0.0f, 1.0f, normalizedValue), mergeToleranceSeconds);
        pushAutomationDataToEngine(automationTrackIndex);
        trackerPanel.refreshTimelineView();
    }
}

void MainComponent::updateAutomationRecordModes()
{
    if (! engine.isPlaying())
    {
        // Passes don't carry a write-active lane over into the next Play - Latch/Write both
        // reset naturally on stop, matching how a real DAW ends a recording pass.
        for (int trackIndex = 0; trackIndex < timelineModel.getTrackCount(); ++trackIndex)
            if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::automation)
                engine.setTrackAutomationWriteActive(trackIndex, false);
        return;
    }

    constexpr double touchReleaseTimeoutSeconds = 0.2;
    auto nowSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;

    for (int trackIndex = 0; trackIndex < timelineModel.getTrackCount(); ++trackIndex)
    {
        if (timelineModel.getTrackKind(trackIndex) != cs::TrackKind::automation)
            continue;

        if (! juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()) || ! armedTracks[(size_t) trackIndex])
            continue;

        auto target = timelineModel.getAutomationTarget(trackIndex);
        if (target.kind == cs::AutomationTargetKind::none)
            continue;

        auto mode = timelineModel.getAutomationRecordMode(trackIndex);

        if (mode == cs::AutomationRecordMode::write)
        {
            // Write mode records continuously for the whole pass, touched or not - sample
            // whatever the target's live value currently is (manual writes above may have just
            // set it; otherwise it's whatever it already was) and bake it into the curve.
            engine.setTrackAutomationWriteActive(trackIndex, true);

            float currentValue = 0.5f;
            if (target.kind == cs::AutomationTargetKind::trackVolume)
                currentValue = engine.getTrackGain(target.targetTrackIndex);
            else if (target.kind == cs::AutomationTargetKind::trackPan)
                currentValue = juce::jmap(engine.getTrackPan(target.targetTrackIndex), -1.0f, 1.0f, 0.0f, 1.0f);
            else if (target.kind == cs::AutomationTargetKind::pluginParameter)
                currentValue = engine.getTrackPluginParameterValue(target.targetTrackIndex, target.pluginSlotIndex, target.pluginParameterIndex);

            auto mergeToleranceSeconds = 1.0 / juce::jmax(1, timelineModel.getAutomationRecordingRate(trackIndex));
            timelineModel.addOrUpdateAutomationPoint(trackIndex, timelineModel.getTransportSeconds(),
                                                     juce::jlimit(0.0f, 1.0f, currentValue), mergeToleranceSeconds);
            pushAutomationDataToEngine(trackIndex);
            trackerPanel.refreshTimelineView();
        }
        else if (mode == cs::AutomationRecordMode::touch)
        {
            // Touch releases (resumes normal curve playback) once nothing has manually moved the
            // target for a short idle period - Latch deliberately has no such release here.
            if (juce::isPositiveAndBelow(trackIndex, (int) automationLastManualWriteWallSeconds.size())
                && nowSeconds - automationLastManualWriteWallSeconds[(size_t) trackIndex] > touchReleaseTimeoutSeconds)
                engine.setTrackAutomationWriteActive(trackIndex, false);
        }
    }
}

bool MainComponent::performTrackMove(int trackIndex, int destinationIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return false;

    // Computed BEFORE the move and reused for the engine call below, so both arrays agree on
    // exactly which range moved - TimelineModel::moveTrackGroup decides this the same way
    // internally, but doesn't hand the value back.
    auto blockLength = timelineModel.getTrackKind(trackIndex) == cs::TrackKind::folder
                          ? timelineModel.getFolderBlockLength(trackIndex)
                          : 1;

    if (! timelineModel.moveTrackGroup(trackIndex, destinationIndex))
        return false;

    engine.moveTrackRange(trackIndex, blockLength, destinationIndex);
    return true;
}

juce::Colour MainComponent::computeTrackAccentColour(int trackIndex) const
{
    static const juce::Colour palette[] = {
        juce::Colour(0xff67e8a5), juce::Colour(0xff74caff), juce::Colour(0xffffd166),
        juce::Colour(0xffff9f6e), juce::Colour(0xffb185ff), juce::Colour(0xffff6b6b),
        juce::Colour(0xff5da5ff), juce::Colour(0xffffc857)
    };
    constexpr auto paletteSize = (int) (sizeof(palette) / sizeof(palette[0]));

    auto colourForFolder = [&](int folderIndex)
    {
        auto id = timelineModel.getTrackId(folderIndex);
        auto index = (int) ((juce::uint32) id.hashCode() % (juce::uint32) paletteSize);
        return palette[index];
    };

    if (timelineModel.getTrackKind(trackIndex) == cs::TrackKind::folder)
        return colourForFolder(trackIndex);

    auto parent = timelineModel.getTrackParent(trackIndex);
    if (parent >= 0)
        return colourForFolder(parent);

    return juce::Colour();
}

void MainComponent::showMoveToFolderPicker(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return;

    juce::PopupMenu menu;
    menu.addItem(1, "None (top-level)", true, timelineModel.getTrackParent(trackIndex) < 0);

    juce::Array<int> folderTrackIndices;
    int nextItemId = 2;
    for (int otherIndex = 0; otherIndex < timelineModel.getTrackCount(); ++otherIndex)
    {
        if (otherIndex == trackIndex || timelineModel.getTrackKind(otherIndex) != cs::TrackKind::folder)
            continue;

        auto folderName = timelineModel.getTrackName(otherIndex);
        if (folderName.isEmpty())
            folderName = "Track " + juce::String(otherIndex + 1);

        menu.addItem(nextItemId, folderName, true, timelineModel.getTrackParent(trackIndex) == otherIndex);
        folderTrackIndices.add(otherIndex);
        ++nextItemId;
    }

    auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }), [this, trackIndex, folderTrackIndices](int result)
    {
        if (result <= 0)
            return;

        auto newParent = result == 1 ? -1 : folderTrackIndices[result - 2];

        if (! timelineModel.setTrackParent(trackIndex, newParent))
        {
            transportBar.setStatusText("Couldn't move that track there - it would create a folder loop.");
            return;
        }

        // Assigning into a folder also physically groups it there - directly under the folder
        // and any of its existing members - so the track list and the routing always agree.
        // Detaching back to top-level (newParent < 0) leaves its position alone.
        if (newParent >= 0)
            performTrackMove(trackIndex, newParent + timelineModel.getFolderBlockLength(newParent));

        syncTrackViews();
        saveSessionToDisk();
    });
}

void MainComponent::showAutomationTargetPicker(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, timelineModel.getTrackCount()))
        return;

    struct MenuAction
    {
        cs::AutomationTarget target;
    };

    auto actions = std::make_shared<std::vector<MenuAction>>();
    juce::PopupMenu menu;
    int nextItemId = 1;

    for (int otherIndex = 0; otherIndex < timelineModel.getTrackCount(); ++otherIndex)
    {
        if (otherIndex == trackIndex)
            continue;

        if (timelineModel.getTrackKind(otherIndex) == cs::TrackKind::automation)
            continue;

        auto trackName = timelineModel.getTrackName(otherIndex);
        if (trackName.isEmpty())
            trackName = "Track " + juce::String(otherIndex + 1);

        juce::PopupMenu trackMenu;

        {
            cs::AutomationTarget target;
            target.kind = cs::AutomationTargetKind::trackVolume;
            target.targetTrackIndex = otherIndex;
            target.displayName = trackName + " \xe2\x86\x92 Volume";
            trackMenu.addItem(nextItemId, "Volume");
            actions->push_back({ target });
            ++nextItemId;
        }

        {
            cs::AutomationTarget target;
            target.kind = cs::AutomationTargetKind::trackPan;
            target.targetTrackIndex = otherIndex;
            target.displayName = trackName + " \xe2\x86\x92 Pan";
            trackMenu.addItem(nextItemId, "Pan");
            actions->push_back({ target });
            ++nextItemId;
        }

        auto pluginNames = engine.getTrackPluginNames(otherIndex);
        for (int slotIndex = 0; slotIndex < pluginNames.size(); ++slotIndex)
        {
            juce::PopupMenu pluginMenu;
            {
                cs::AutomationTarget target;
                target.kind = cs::AutomationTargetKind::pluginBypass;
                target.targetTrackIndex = otherIndex;
                target.pluginSlotIndex = slotIndex;
                target.displayName = trackName + " → " + pluginNames[slotIndex] + " → Bypass";
                target.valueMode = cs::AutomationValueMode::toggle;
                target.stepCount = 2;

                pluginMenu.addItem(nextItemId, "Bypass");
                actions->push_back({ target });
                ++nextItemId;
            }

            auto paramCount = engine.getTrackPluginParameterCount(otherIndex, slotIndex);
            for (int paramIndex = 0; paramIndex < paramCount; ++paramIndex)
            {
                auto paramName = engine.getTrackPluginParameterName(otherIndex, slotIndex, paramIndex);
                if (paramName.isEmpty())
                    paramName = "Param " + juce::String(paramIndex + 1);

                cs::AutomationTarget target;
                target.kind = cs::AutomationTargetKind::pluginParameter;
                target.targetTrackIndex = otherIndex;
                target.pluginSlotIndex = slotIndex;
                target.pluginParameterIndex = paramIndex;
                target.parameterId = juce::String(paramIndex);
                target.displayName = trackName + " \xe2\x86\x92 " + pluginNames[slotIndex] + " \xe2\x86\x92 " + paramName;

                pluginMenu.addItem(nextItemId, paramName);
                actions->push_back({ target });
                ++nextItemId;
            }

            trackMenu.addSubMenu(pluginNames[slotIndex], pluginMenu);
        }

        // Each Signal clip on this track offers its Public variables. Only Public ones are settable from
        // outside the graph; Float gets a continuous lane and Bool a toggle lane (Int has no range in the
        // patch yet, so it is not offered).
        for (const auto& clip : timelineModel.getClips())
        {
            if (clip.recording || clip.kind != cs::ClipKind::signal || clip.trackIndex != otherIndex)
                continue;

            cw::PatchDocument clipPatch;
            juce::String clipPatchKey, clipPatchError;
            if (! loadSignalClipPatch(clip, clipPatch, clipPatchKey, clipPatchError))
                continue;

            juce::PopupMenu clipMenu;
            int offered = 0;
            for (const auto& variable : clipPatch.variables)
            {
                if (! variable.isPublic || (variable.valueType != "Float" && variable.valueType != "Bool"))
                    continue;

                cs::AutomationTarget target;
                target.kind = cs::AutomationTargetKind::signalClipInput;
                target.targetTrackIndex = otherIndex;
                target.targetClipId = clip.id;
                target.parameterId = variable.id;
                target.displayName = trackName + " \xe2\x86\x92 " + clip.displayName + " \xe2\x86\x92 " + variable.name;
                if (variable.valueType == "Bool")
                {
                    target.valueMode = cs::AutomationValueMode::toggle;
                    target.stepCount = 2;
                }

                clipMenu.addItem(nextItemId, variable.name);
                actions->push_back({ target });
                ++nextItemId;
                ++offered;
            }

            if (offered > 0)
                trackMenu.addSubMenu("Signal: " + clip.displayName, clipMenu);
            else
                trackMenu.addItem(-1, "Signal: " + clip.displayName + " (no public variables)", false);
        }

        menu.addSubMenu(trackName, trackMenu);
    }

    if (nextItemId == 1)
    {
        menu.addItem(-1, "No other tracks available", false);
    }

    auto clickPoint = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt();
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetScreenArea({ clickPoint.x, clickPoint.y, 1, 1 }), [this, trackIndex, actions](int result)
    {
        if (result <= 0 || (size_t) result > actions->size())
            return;

        const auto& action = (*actions)[(size_t) result - 1];
        timelineModel.setAutomationTarget(trackIndex, action.target);
        pushAutomationDataToEngine(trackIndex);
        trackerPanel.setAutomationTargetLabel(trackIndex, action.target.displayName);
        saveSessionToDisk();
    });
}

void MainComponent::refreshTrackInputSources()
{
    auto inputSources = engine.getInputSources();
    juce::Array<cs::HardwareInputSource> hardwareInputs;
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    auto selectedInputDeviceName = selectedStudioInputDevice.trim();
    if (selectedInputDeviceName.isEmpty())
        selectedInputDeviceName = setup.inputDeviceName.trim();

    for (const auto& source : inputSources)
    {
        cs::HardwareInputSource hardwareInput;
        hardwareInput.channelIndex = source.channelIndex;
        hardwareInput.id = source.id;
        hardwareInput.name = source.name;

        if (selectedInputDeviceName.isNotEmpty())
        {
            auto suffix = source.name.fromLastOccurrenceOf(" / ", false, false).trim();
            if (suffix.isEmpty() || suffix == source.name)
                suffix = "Input channel " + juce::String(source.channelIndex + 1);

            hardwareInput.name = selectedInputDeviceName + " / " + suffix;
        }

        hardwareInputs.add(std::move(hardwareInput));
    }

    studioIOModel.setHardwareInputs(hardwareInputs);
    juce::Array<juce::String> trackerInputNames;
    for (const auto& name : studioIOModel.getNames())
        trackerInputNames.add(name);
    trackerPanel.setInputSources(trackerInputNames);
    settingsPanel.setStudioInputRows(studioIOModel.getNames(),
                                     studioIOModel.getHardwareNames(),
                                     studioIOModel.getAvailability());
}

void MainComponent::refreshAudioDeviceSettingsView()
{
    juce::OwnedArray<juce::AudioIODeviceType> deviceTypes;
    deviceManager.createAudioDeviceTypes(deviceTypes);

    juce::StringArray audioSystems;
    juce::StringArray inputDevices;
    juce::StringArray outputDevices;
    auto selectedSystem = deviceManager.getCurrentAudioDeviceType();
    selectedStudioAudioSystem = selectedSystem;

    for (auto* type : deviceTypes)
    {
        if (type == nullptr)
            continue;

        auto typeName = type->getTypeName();
        audioSystems.addIfNotAlreadyThere(typeName);

        if (typeName == selectedSystem)
        {
            type->scanForDevices();
            inputDevices = type->getDeviceNames(true);
            outputDevices = type->getDeviceNames(false);
        }
    }

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);
    if (selectedStudioInputDevice.isEmpty())
        selectedStudioInputDevice = setup.inputDeviceName;
    if (selectedStudioOutputDevice.isEmpty())
        selectedStudioOutputDevice = setup.outputDeviceName;

    settingsPanel.setAudioDeviceLists(audioSystems,
                                      inputDevices,
                                      outputDevices,
                                      selectedSystem,
                                      selectedStudioInputDevice.isNotEmpty() ? selectedStudioInputDevice : setup.inputDeviceName,
                                      selectedStudioOutputDevice.isNotEmpty() ? selectedStudioOutputDevice : setup.outputDeviceName);

    juce::String diagnostics;
    auto canOpenDriverControlPanel = false;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        auto activeInputs = device->getActiveInputChannels();
        auto activeOutputs = device->getActiveOutputChannels();
        auto inputNames = device->getInputChannelNames();

        diagnostics << "Driver: " << device->getTypeName() << " / " << device->getName() << "\n";
        diagnostics << "Rate: " << juce::String(device->getCurrentSampleRate(), 0)
                    << " Hz   Buffer: " << device->getCurrentBufferSizeSamples() << " samples\n";
        diagnostics << "Inputs: " << activeInputs.countNumberOfSetBits()
                    << " active   Outputs: " << activeOutputs.countNumberOfSetBits() << " active";

        if (! inputNames.isEmpty())
        {
            diagnostics << "\nInput names: ";
            for (int index = 0; index < inputNames.size(); ++index)
            {
                if (index > 0)
                    diagnostics << ", ";
                diagnostics << inputNames[index];
            }
        }

        canOpenDriverControlPanel = device->hasControlPanel();
    }
    else
    {
        diagnostics = "No active audio device. Choose an audio system and device.";
    }

    settingsPanel.setAudioDiagnostics(diagnostics, canOpenDriverControlPanel);
}

void MainComponent::setAudioSystem(const juce::String& audioSystem)
{
    if (audioSystem.isEmpty() || audioSystem == deviceManager.getCurrentAudioDeviceType())
        return;

    selectedStudioAudioSystem = audioSystem;
    selectedStudioInputDevice.clear();
    selectedStudioOutputDevice.clear();
    deviceManager.setCurrentAudioDeviceType(audioSystem, true);
    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    saveAppSettings();
    transportBar.setStatusText("Audio system: " + audioSystem);
}

void MainComponent::setAudioInputDevice(const juce::String& inputDeviceName)
{
    if (inputDeviceName.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.inputDeviceName == inputDeviceName)
    {
        selectedStudioInputDevice = inputDeviceName;
        return;
    }

    selectedStudioInputDevice = inputDeviceName;
    setup.inputDeviceName = inputDeviceName;
    setup.useDefaultInputChannels = false;
    setup.inputChannels.clear();
    for (int channel = 0; channel < 32; ++channel)
        setup.inputChannels.setBit(channel);

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        transportBar.setStatusText(error);
        refreshAudioDeviceSettingsView();
        return;
    }

    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    syncTrackViews();
    saveAppSettings();
    transportBar.setStatusText("Input device: " + inputDeviceName);
}

void MainComponent::setAudioOutputDevice(const juce::String& outputDeviceName)
{
    if (outputDeviceName.isEmpty())
        return;

    juce::AudioDeviceManager::AudioDeviceSetup setup;
    deviceManager.getAudioDeviceSetup(setup);

    if (setup.outputDeviceName == outputDeviceName)
    {
        selectedStudioOutputDevice = outputDeviceName;
        return;
    }

    selectedStudioOutputDevice = outputDeviceName;
    setup.outputDeviceName = outputDeviceName;
    setup.useDefaultOutputChannels = true;

    auto error = deviceManager.setAudioDeviceSetup(setup, true);
    if (error.isNotEmpty())
    {
        transportBar.setStatusText(error);
        refreshAudioDeviceSettingsView();
        return;
    }

    refreshAudioDeviceSettingsView();
    refreshTrackInputSources();
    saveAppSettings();
    transportBar.setStatusText("Output device: " + outputDeviceName);
}

void MainComponent::addTrack()
{
    juce::ValueTree undoSnapshot("UndoSnapshot");
    undoSnapshot.addChild(timelineModel.createState(), -1, nullptr);
    undoSnapshot.addChild(engine.createSessionState(), -1, nullptr);

    auto trackIndex = engine.addTrack();
    if (trackIndex < 0)
        return;

    pushTimelineUndoState(undoSnapshot);

    if ((int) armedTracks.size() <= trackIndex)
        armedTracks.resize((size_t) trackIndex + 1, false);
    if ((int) monitoredTracks.size() <= trackIndex)
        monitoredTracks.resize((size_t) trackIndex + 1, false);

    syncTrackViews();

    auto bankOffset = (trackIndex / mixerPanel.getVisibleChannelCount()) * mixerPanel.getVisibleChannelCount();
    mixerPanel.setBankOffset(bankOffset);
    midiSurface.setBankOffset(bankOffset);

    if (juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
    {
        pluginRackBar.setContextTrack(trackIndex, engine.getTrackName(trackIndex));
        mixerPanel.setSelectedChannel(trackIndex);
        trackerPanel.setSelectedTrack(trackIndex);
    }

    auto visibleStart = mixerPanel.getBankOffset();
    auto bankVisibleCount = mixerPanel.getVisibleChannelCount();

    for (int slot = 0; slot < bankVisibleCount; ++slot)
    {
        auto visibleTrackIndex = visibleStart + slot;
        if (! juce::isPositiveAndBelow(visibleTrackIndex, engine.getTrackCount()))
            continue;

        auto name = engine.getTrackName(visibleTrackIndex);
        auto gain = engine.getTrackGain(visibleTrackIndex);
        auto pan = engine.getTrackPan(visibleTrackIndex);
        auto muted = engine.isTrackMuted(visibleTrackIndex);
        auto soloed = engine.isTrackSoloed(visibleTrackIndex);
        auto pluginName = engine.getTrackPluginName(visibleTrackIndex);
        auto pluginBypassed = engine.isTrackPluginBypassed(visibleTrackIndex);

        mixerPanel.setChannelName(visibleTrackIndex, name);
        mixerPanel.setChannelInsertName(visibleTrackIndex, pluginName.isNotEmpty() ? ("FX: " + pluginName) : "FX: none");
        mixerPanel.setChannelInsertBypassed(visibleTrackIndex, pluginBypassed);
        mixerPanel.setChannelGain(visibleTrackIndex, gain);
        mixerPanel.setChannelPan(visibleTrackIndex, pan);
        mixerPanel.setChannelMuted(visibleTrackIndex, muted);
        mixerPanel.setChannelSoloed(visibleTrackIndex, soloed);

        midiSurface.setChannelName(visibleTrackIndex, name);
        midiSurface.setChannelGain(visibleTrackIndex, gain);
        midiSurface.setChannelPan(visibleTrackIndex, pan);
        midiSurface.setChannelMuted(visibleTrackIndex, muted);
        midiSurface.setChannelSoloed(visibleTrackIndex, soloed);
    }

    auto masterGain = engine.getMasterGain();
    mixerPanel.setMasterGain(masterGain);
    midiSurface.setMasterFaderValue(masterGain);
    midiSurface.refreshVisibleWindow();
    refreshInsertRack();
    saveSessionToDisk();
}

void MainComponent::removeTrack(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    auto trackName = engine.getTrackName(trackIndex);
    if (trackName.trim().isEmpty())
        trackName = "Track " + juce::String(trackIndex + 1);

    auto options = juce::MessageBoxOptions()
        .withIconType(juce::MessageBoxIconType::WarningIcon)
        .withTitle("Remove Track")
        .withMessage("Remove track \"" + trackName + "\"? This can be undone with Ctrl+Z.")
        .withButton("Remove")
        .withButton("Cancel");

    auto safeThis = juce::Component::SafePointer<MainComponent>(this);
    juce::AlertWindow::showAsync(options,
                                 [safeThis, trackIndex](int result)
                                 {
                                     if (safeThis == nullptr || result != 1)
                                         return;

                                     safeThis->performTrackRemoval(trackIndex);
                                 });
}

void MainComponent::performTrackRemoval(int trackIndex)
{
    if (! juce::isPositiveAndBelow(trackIndex, engine.getTrackCount()))
        return;

    juce::ValueTree undoSnapshot("UndoSnapshot");
    undoSnapshot.addChild(timelineModel.createState(), -1, nullptr);
    undoSnapshot.addChild(engine.createSessionState(), -1, nullptr);

    if (! engine.removeTrack(trackIndex))
        return;

    pushTimelineUndoState(undoSnapshot);

    timelineModel.removeTrack(trackIndex);
    trackerPanel.refreshTimelineView();

    if (juce::isPositiveAndBelow(trackIndex, (int) armedTracks.size()))
        armedTracks.erase(armedTracks.begin() + trackIndex);
    if (juce::isPositiveAndBelow(trackIndex, (int) monitoredTracks.size()))
        monitoredTracks.erase(monitoredTracks.begin() + trackIndex);

    syncTrackViews();

    auto trackCount = engine.getTrackCount();
    auto maxBankOffset = juce::jmax(0, trackCount - mixerPanel.getVisibleChannelCount());
    mixerPanel.setBankOffset(juce::jlimit(0, maxBankOffset, mixerPanel.getBankOffset()));
    midiSurface.setBankOffset(mixerPanel.getBankOffset());

    if (trackCount == 0)
    {
        pluginRackBar.setContextMaster();
        mixerPanel.setSelectedChannel(-1);
        trackerPanel.setSelectedTrack(-1);
    }
    else
    {
        auto selectedTrack = juce::jlimit(0, trackCount - 1, trackIndex);
        pluginRackBar.setContextTrack(selectedTrack, engine.getTrackName(selectedTrack));
        mixerPanel.setSelectedChannel(selectedTrack);
        trackerPanel.setSelectedTrack(selectedTrack);
    }

    midiSurface.refreshVisibleWindow();
    refreshInsertRack();
    saveSessionToDisk();
}

void MainComponent::refreshInsertRack()
{
    if (pluginRackBar.isTrackContext())
    {
        auto trackIndex = pluginRackBar.getTrackIndex();
        pluginRackBar.setPluginName(engine.getTrackPluginName(trackIndex));
        pluginRackBar.setBypassed(engine.isTrackPluginBypassed(trackIndex));
    }
    else
    {
        pluginRackBar.setPluginName(engine.getMasterPluginName());
        pluginRackBar.setBypassed(engine.isMasterPluginBypassed());
    }

    refreshFxStackWindow();
    refreshPluginsPanel();
}
