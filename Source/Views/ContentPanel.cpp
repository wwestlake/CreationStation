#include "ContentPanel.h"

namespace
{
juce::Colour contentPanelColour() { return juce::Colour(0xff11151c); }
juce::Colour contentCardColour() { return juce::Colour(0xff1a2030); }
juce::Colour contentAccentColour() { return juce::Colour(0xff6fa8ff); }

juce::String describeAction(const ContentLibrary::Item& item)
{
    if (item.origin == ContentLibrary::Origin::remote)
    {
        if (item.accessState == ContentLibrary::AccessState::available)
            return "Download";

        if (item.accessState == ContentLibrary::AccessState::locked)
            return "Locked";
    }

    if (item.accessState == ContentLibrary::AccessState::installed && item.file.existsAsFile())
    {
        if (item.type == "patch")
            return "Open Patch";
        if (item.type == "audio")
            return "Import Audio";
        return "Reveal";
    }

    return {};
}
}

ContentPanel::TutorialCard::TutorialCard()
{
    launchButton.onClick = [this]
    {
        if (onLaunchRequested)
            onLaunchRequested(item);
    };
    addAndMakeVisible(launchButton);

    revealButton.onClick = [this]
    {
        if (onRevealRequested)
            onRevealRequested(item);
    };
    addAndMakeVisible(revealButton);
}

void ContentPanel::TutorialCard::setItem(const TutorialItem& newItem)
{
    item = newItem;
    repaint();
}

void ContentPanel::TutorialCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(contentCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto area = getLocalBounds().reduced(14);
    auto textArea = area.removeFromLeft(area.getWidth() - 210);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText(item.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff9fb0c8));
    g.setFont(juce::Font(13.0f));
    g.drawText(item.builtIn ? "Built-in tutorial" : "User tutorial",
               textArea.removeFromTop(18),
               juce::Justification::centredLeft,
               true);
    g.drawText(item.description,
               textArea.removeFromTop(34),
               juce::Justification::topLeft,
               true);
}

void ContentPanel::TutorialCard::resized()
{
    auto buttons = getLocalBounds().removeFromRight(196).reduced(14, 18);
    launchButton.setBounds(buttons.removeFromTop(28));
    buttons.removeFromTop(8);
    revealButton.setBounds(buttons.removeFromTop(28));
}

ContentPanel::ProjectAssetCard::ProjectAssetCard()
{
    openButton.onClick = [this]
    {
        if (onOpenRequested)
            onOpenRequested(asset);
    };
    addAndMakeVisible(openButton);

    placeButton.onClick = [this]
    {
        if (onPlaceRequested)
            onPlaceRequested(asset);
    };
    addAndMakeVisible(placeButton);
}

void ContentPanel::ProjectAssetCard::setAsset(const ProjectManager::ProjectAsset& newAsset)
{
    asset = newAsset;
    placeButton.setVisible(asset.type == "audioFile" || asset.type == "render");
    repaint();
}

void ContentPanel::ProjectAssetCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(juce::Colour(0xff172336));
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff49d3ff));
    g.drawRoundedRectangle(bounds, 12.0f, 1.2f);

    auto area = getLocalBounds().reduced(14);
    auto textArea = area;
    textArea.removeFromRight(190);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText(asset.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff9fb0c8));
    g.setFont(juce::Font(13.0f));
    g.drawText(asset.category + "  |  " + asset.type, textArea.removeFromTop(18), juce::Justification::centredLeft, true);
    g.drawText(asset.description, textArea.removeFromTop(20), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff7dd36f));
    g.drawText(asset.relativePath, textArea.removeFromBottom(20), juce::Justification::centredLeft, true);
}

void ContentPanel::ProjectAssetCard::resized()
{
    auto buttons = getLocalBounds().removeFromRight(184).reduced(14, 18);
    openButton.setBounds(buttons.removeFromLeft(78));
    buttons.removeFromLeft(8);
    placeButton.setBounds(buttons.removeFromLeft(78));
}

ContentPanel::ItemCard::ItemCard()
{
    actionButton.onClick = [this]
    {
        if (item.origin == ContentLibrary::Origin::remote
            && item.accessState == ContentLibrary::AccessState::available
            && onDownloadRequested)
        {
            onDownloadRequested(item);
            return;
        }

        if (item.file.existsAsFile() && onRevealRequested)
            onRevealRequested(item);
    };

    addAndMakeVisible(actionButton);
}

void ContentPanel::ItemCard::setItem(const ContentLibrary::Item& newItem)
{
    item = newItem;
    updateActionButton();
    repaint();
}

void ContentPanel::ItemCard::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    g.setColour(contentCardColour());
    g.fillRoundedRectangle(bounds, 12.0f);
    g.setColour(juce::Colour(0xff2a3445));
    g.drawRoundedRectangle(bounds, 12.0f, 1.0f);

    auto area = getLocalBounds().reduced(14);
    auto textArea = area;
    textArea.removeFromRight(116);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(17.0f).boldened());
    g.drawText(item.name, textArea.removeFromTop(22), juce::Justification::centredLeft, true);

    g.setColour(juce::Colour(0xff9fb0c8));
    g.setFont(juce::Font(13.0f));
    g.drawText(item.category + "  |  " + item.type, textArea.removeFromTop(18), juce::Justification::centredLeft, true);
    g.drawText(item.description, textArea.removeFromTop(20), juce::Justification::centredLeft, true);

    auto footer = textArea.removeFromBottom(22);
    g.setColour(contentAccentColour());
    g.drawText(ContentLibrary::originName(item.origin), footer.removeFromLeft(120), juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xff7dd36f));
    g.drawText(ContentLibrary::accessName(item.accessState), footer.removeFromLeft(100), juce::Justification::centredLeft, true);
    g.setColour(juce::Colour(0xff9fb0c8));
    g.drawText(juce::File::descriptionOfSizeInBytes(item.fileSizeBytes), footer, juce::Justification::centredRight, true);
}

void ContentPanel::ItemCard::resized()
{
    actionButton.setBounds(getLocalBounds().removeFromRight(116).reduced(14, 18));
}

void ContentPanel::ItemCard::updateActionButton()
{
    auto actionText = describeAction(item);
    actionButton.setVisible(actionText.isNotEmpty());
    actionButton.setButtonText(actionText);
    actionButton.setEnabled(actionText.isNotEmpty() && actionText != "Locked");
}

ContentPanel::ContentPanel()
{
    setName("Library");

    titleLabel.setText("Content Library", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(24.0f).boldened());
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    addAndMakeVisible(titleLabel);

    subtitleLabel.setText("All Creation Station content will live here - free, premium, downloaded, and your own local library.", juce::dontSendNotification);
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colour(0xff8ea0b7));
    addAndMakeVisible(subtitleLabel);

    storageLabel.setColour(juce::Label::textColourId, juce::Colour(0xff9fb0c8));
    addAndMakeVisible(storageLabel);

    statusLabel.setColour(juce::Label::textColourId, juce::Colour(0xffa7b6cb));
    addAndMakeVisible(statusLabel);

    projectAssetsLabel.setText("Project Assets", juce::dontSendNotification);
    projectAssetsLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    projectAssetsLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(projectAssetsLabel);

    tutorialLabel.setText("Tutorial Studio", juce::dontSendNotification);
    tutorialLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    tutorialLabel.setFont(juce::Font(18.0f).boldened());
    addAndMakeVisible(tutorialLabel);

    refreshButton.onClick = [this]
    {
        if (onRefreshRequested)
            onRefreshRequested();
    };
    addAndMakeVisible(refreshButton);

    openFolderButton.onClick = [this]
    {
        if (onOpenContentFolderRequested)
            onOpenContentFolderRequested();
    };
    addAndMakeVisible(openFolderButton);

    adminPublishButton.onClick = [this]
    {
        if (onAdminPublishRequested)
            onAdminPublishRequested();
    };
    addAndMakeVisible(adminPublishButton);
    adminPublishButton.setVisible(false);

    projectAssetsViewport.setViewedComponent(&projectAssetsHost, false);
    projectAssetsViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(projectAssetsViewport);

    tutorialViewport.setViewedComponent(&tutorialHost, false);
    tutorialViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(tutorialViewport);

    viewport.setViewedComponent(&cardsHost, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);

    setStatusText("Waiting for content storage.");
}

void ContentPanel::setItems(const juce::Array<ContentLibrary::Item>& newItems)
{
    items = newItems;
    cards.clear(true);

    for (const auto& item : items)
    {
        auto* card = cards.add(new ItemCard());
        card->onDownloadRequested = [this](const ContentLibrary::Item& selectedItem)
        {
            if (onDownloadRequested)
                onDownloadRequested(selectedItem);
        };
        card->onRevealRequested = [this](const ContentLibrary::Item& selectedItem)
        {
            if (onRevealItemRequested)
                onRevealItemRequested(selectedItem);
        };
        card->setItem(item);
        cardsHost.addAndMakeVisible(card);
    }

    resized();
}

void ContentPanel::setProjectAssets(const juce::Array<ProjectManager::ProjectAsset>& newAssets)
{
    projectAssets = newAssets;
    projectAssetCards.clear(true);

    for (const auto& asset : projectAssets)
    {
        auto* card = projectAssetCards.add(new ProjectAssetCard());
        card->onOpenRequested = [this](const ProjectManager::ProjectAsset& selectedAsset)
        {
            if (onOpenProjectAssetRequested)
                onOpenProjectAssetRequested(selectedAsset);
        };
        card->onPlaceRequested = [this](const ProjectManager::ProjectAsset& selectedAsset)
        {
            if (onPlaceProjectAssetRequested)
                onPlaceProjectAssetRequested(selectedAsset);
        };
        card->setAsset(asset);
        projectAssetsHost.addAndMakeVisible(card);
    }

    projectAssetsLabel.setText("Project Assets (" + juce::String(projectAssets.size()) + ")", juce::dontSendNotification);
    resized();
}

void ContentPanel::setTutorialItems(const juce::Array<TutorialItem>& newItems)
{
    tutorialItems = newItems;
    tutorialCards.clear(true);

    for (const auto& item : tutorialItems)
    {
        auto* card = tutorialCards.add(new TutorialCard());
        card->onLaunchRequested = [this](const TutorialItem& tutorial)
        {
            if (onLaunchTutorialRequested)
                onLaunchTutorialRequested(tutorial);
        };
        card->onRevealRequested = [this](const TutorialItem& tutorial)
        {
            if (onRevealTutorialRequested)
                onRevealTutorialRequested(tutorial);
        };
        card->setItem(item);
        tutorialHost.addAndMakeVisible(card);
    }

    resized();
}

void ContentPanel::setStoragePath(const juce::String& path)
{
    storageLabel.setText("Storage: " + (path.isNotEmpty() ? path : "Not configured"), juce::dontSendNotification);
}

void ContentPanel::setStatusText(const juce::String& text)
{
    statusLabel.setText(text, juce::dontSendNotification);
}

void ContentPanel::setAuthState(bool isSignedIn, bool isAdmin)
{
    signedIn = isSignedIn;
    admin = isAdmin;
    refreshButton.setEnabled(signedIn);
    adminPublishButton.setVisible(admin);
    resized();
}

void ContentPanel::paint(juce::Graphics& g)
{
    g.fillAll(contentPanelColour());
}

void ContentPanel::resized()
{
    auto area = getLocalBounds().reduced(18);
    titleLabel.setBounds(area.removeFromTop(32));
    subtitleLabel.setBounds(area.removeFromTop(22));
    storageLabel.setBounds(area.removeFromTop(22));
    statusLabel.setBounds(area.removeFromTop(22));
    area.removeFromTop(8);

    auto buttons = area.removeFromTop(30);
    refreshButton.setBounds(buttons.removeFromLeft(130));
    buttons.removeFromLeft(10);
    openFolderButton.setBounds(buttons.removeFromLeft(150));
    if (adminPublishButton.isVisible())
    {
        buttons.removeFromLeft(10);
        adminPublishButton.setBounds(buttons.removeFromLeft(120));
    }
    area.removeFromTop(10);

    projectAssetsLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    auto projectAssetArea = area.removeFromTop(juce::jmin(220, juce::jmax(96, 18 + projectAssetCards.size() * 100)));
    projectAssetsViewport.setBounds(projectAssetArea);
    area.removeFromTop(12);

    tutorialLabel.setBounds(area.removeFromTop(24));
    area.removeFromTop(6);
    auto tutorialArea = area.removeFromTop(juce::jmin(220, juce::jmax(96, 18 + tutorialCards.size() * 100)));
    tutorialViewport.setBounds(tutorialArea);
    area.removeFromTop(12);

    viewport.setBounds(area);

    auto tutorialWidth = juce::jmax(320, tutorialViewport.getWidth() - 24);
    auto projectAssetWidth = juce::jmax(320, projectAssetsViewport.getWidth() - 24);
    auto projectAssetY = 0;
    for (auto* card : projectAssetCards)
    {
        card->setBounds(0, projectAssetY, projectAssetWidth, 92);
        projectAssetY += 100;
    }
    projectAssetsHost.setSize(projectAssetWidth, juce::jmax(projectAssetY, projectAssetsViewport.getHeight()));

    auto tutorialY = 0;
    for (auto* card : tutorialCards)
    {
        card->setBounds(0, tutorialY, tutorialWidth, 92);
        tutorialY += 100;
    }
    tutorialHost.setSize(tutorialWidth, juce::jmax(tutorialY, tutorialViewport.getHeight()));

    auto cardWidth = juce::jmax(320, viewport.getWidth() - 24);
    auto y = 0;
    for (auto* card : cards)
    {
        card->setBounds(0, y, cardWidth, 92);
        y += 100;
    }

    cardsHost.setSize(cardWidth, juce::jmax(y, viewport.getHeight()));
}
