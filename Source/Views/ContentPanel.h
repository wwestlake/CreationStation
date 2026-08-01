#include <creation/assets/ProjectManifest.h>
#include <creation/assets/AssetTypes.h>
#pragma once

#include <JuceHeader.h>
#include "../Content/ContentLibrary.h"

class ContentPanel final : public juce::Component
{
public:
    struct TutorialItem
    {
        juce::String name;
        juce::String description;
        juce::File file;
        bool builtIn = false;
    };

    ContentPanel();

    void setItems(const juce::Array<ContentLibrary::Item>& newItems);
    void setProjectAssets(const juce::Array<creation::assets::AssetDescriptor>& newAssets);
    void setTutorialItems(const juce::Array<TutorialItem>& newItems);
    void setStoragePath(const juce::String& path);
    void setStatusText(const juce::String& text);
    void setAuthState(bool isSignedIn, bool isAdmin);

    std::function<void()> onRefreshRequested;
    std::function<void()> onOpenContentFolderRequested;
    std::function<void()> onAdminPublishRequested;
    std::function<void(const ContentLibrary::Item&)> onDownloadRequested;
    std::function<void(const ContentLibrary::Item&)> onRevealItemRequested;
    std::function<void(const creation::assets::AssetDescriptor&)> onOpenProjectAssetRequested;
    std::function<void(const creation::assets::AssetDescriptor&)> onPlaceProjectAssetRequested;
    std::function<void(const creation::assets::AssetDescriptor&)> onExportProjectAssetRequested;
    std::function<void(const TutorialItem&)> onLaunchTutorialRequested;
    std::function<void(const TutorialItem&)> onRevealTutorialRequested;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    class ItemCard final : public juce::Component
    {
    public:
        ItemCard();

        void setItem(const ContentLibrary::Item& newItem);
        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void(const ContentLibrary::Item&)> onDownloadRequested;
        std::function<void(const ContentLibrary::Item&)> onRevealRequested;

    private:
        void updateActionButton();

        ContentLibrary::Item item;
        juce::TextButton actionButton;
    };

    class TutorialCard final : public juce::Component
    {
    public:
        TutorialCard();

        void setItem(const TutorialItem& newItem);
        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void(const TutorialItem&)> onLaunchRequested;
        std::function<void(const TutorialItem&)> onRevealRequested;

    private:
        TutorialItem item;
        juce::TextButton launchButton { "Run Tutorial" };
        juce::TextButton revealButton { "Reveal" };
    };

    class ProjectAssetCard final : public juce::Component
    {
    public:
        ProjectAssetCard();

        void setAsset(const creation::assets::AssetDescriptor& newAsset);
        void paint(juce::Graphics& g) override;
        void resized() override;

        std::function<void(const creation::assets::AssetDescriptor&)> onOpenRequested;
        std::function<void(const creation::assets::AssetDescriptor&)> onPlaceRequested;
        std::function<void(const creation::assets::AssetDescriptor&)> onExportRequested;

    private:
        creation::assets::AssetDescriptor asset;
        juce::TextButton openButton { "Open" };
        juce::TextButton placeButton { "Place" };
        juce::TextButton exportButton { "Export Raw" };
    };

    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::Label storageLabel;
    juce::Label statusLabel;
    juce::Label projectAssetsLabel;
    juce::Label tutorialLabel;
    juce::TextButton refreshButton { "Refresh Library" };
    juce::TextButton openFolderButton { "Open Content Folder" };
    juce::TextButton adminPublishButton { "Admin Publish" };
    juce::Viewport projectAssetsViewport;
    juce::Component projectAssetsHost;
    juce::OwnedArray<ProjectAssetCard> projectAssetCards;
    juce::Array<creation::assets::AssetDescriptor> projectAssets;
    juce::Viewport tutorialViewport;
    juce::Component tutorialHost;
    juce::OwnedArray<TutorialCard> tutorialCards;
    juce::Array<TutorialItem> tutorialItems;
    juce::Viewport viewport;
    juce::Component cardsHost;
    juce::OwnedArray<ItemCard> cards;
    juce::Array<ContentLibrary::Item> items;
    bool signedIn = false;
    bool admin = false;
};
