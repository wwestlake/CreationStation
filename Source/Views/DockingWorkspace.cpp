#include "DockingWorkspace.h"

namespace
{
juce::Colour zoneColour(DockingWorkspace::Zone zone)
{
    switch (zone)
    {
        case DockingWorkspace::Zone::top: return juce::Colour(0xff11151c);
        case DockingWorkspace::Zone::left: return juce::Colour(0xff13171d);
        case DockingWorkspace::Zone::centre: return juce::Colour(0xff171a21);
        case DockingWorkspace::Zone::right: return juce::Colour(0xff141820);
        case DockingWorkspace::Zone::bottom: return juce::Colour(0xff10141a);
    }

    return juce::Colour(0xff171a21);
}

juce::Colour highlightColour()
{
    return juce::Colour(0xff5f93ff).withAlpha(0.28f);
}
}

DockingWorkspace::DockGroup::TabButton::TabButton(DockGroup& ownerRef,
                                                  juce::Component& paneRef,
                                                  const juce::String& titleRef)
    : juce::Button(titleRef), owner(ownerRef), pane(paneRef), title(titleRef)
{
    setClickingTogglesState(false);
}

void DockingWorkspace::DockGroup::TabButton::setSelected(bool shouldSelect)
{
    selected = shouldSelect;
    repaint();
}

void DockingWorkspace::DockGroup::TabButton::paintButton(juce::Graphics& g,
                                                         bool shouldDrawButtonAsHighlighted,
                                                         bool shouldDrawButtonAsDown)
{
    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    auto colour = selected ? juce::Colour(0xff566c94)
                           : (shouldDrawButtonAsDown ? juce::Colour(0xff3a4a66)
                                                     : shouldDrawButtonAsHighlighted ? juce::Colour(0xff2f3b52)
                                                                                     : juce::Colour(0xff202735));

    g.setColour(colour);
    g.fillRoundedRectangle(bounds, 6.0f);

    g.setColour(selected ? juce::Colour(0xff8eb1ff) : juce::Colour(0xff39465d));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);

    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(13.0f).boldened());
    g.drawFittedText(title, getLocalBounds().reduced(8, 0), juce::Justification::centred, 1);
}

void DockingWorkspace::DockGroup::TabButton::mouseDown(const juce::MouseEvent& event)
{
    mouseDownPosition = event.getMouseDownScreenPosition();
    dragStarted = false;
    owner.setActivePane(pane);
}

void DockingWorkspace::DockGroup::TabButton::mouseDrag(const juce::MouseEvent& event)
{
    auto distance = mouseDownPosition.getDistanceFrom(event.getScreenPosition());
    if (! dragStarted && distance > 6)
    {
        dragStarted = true;
        if (owner.owner != nullptr)
            owner.owner->beginPaneDrag(pane, owner.getZone());
    }

    if (dragStarted)
        if (owner.owner != nullptr)
            owner.owner->updateDragTarget(event.getScreenPosition().roundToInt());
}

void DockingWorkspace::DockGroup::TabButton::mouseUp(const juce::MouseEvent& event)
{
    if (dragStarted)
        if (owner.owner != nullptr)
            owner.owner->completePaneDrag(event.getScreenPosition().roundToInt());
}

void DockingWorkspace::DockGroup::initialise(DockingWorkspace& ownerRef, Zone zoneRef)
{
    owner = &ownerRef;
    zone = zoneRef;
}

void DockingWorkspace::DockGroup::updateSelection()
{
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto* entry = tabs.getUnchecked(i);
        if (entry->button != nullptr)
            entry->button->setSelected(i == activeIndex);
    }
}

void DockingWorkspace::DockGroup::addPane(juce::Component& pane, const juce::String& title)
{
    if (indexOfPane(pane) >= 0)
        return;

    auto entry = std::make_unique<TabEntry>();
    entry->pane = &pane;
    entry->title = title;
    entry->button = std::make_unique<TabButton>(*this, pane, title);
    addAndMakeVisible(*entry->button);
    addAndMakeVisible(pane);
    pane.setVisible(true);
    tabs.add(entry.release());

    if (activeIndex < 0)
        activeIndex = 0;

    updateSelection();
    resized();
}

void DockingWorkspace::DockGroup::removePane(juce::Component& pane)
{
    for (int i = tabs.size(); --i >= 0;)
    {
        auto* entry = tabs.getUnchecked(i);
        if (entry->pane != &pane)
            continue;

        if (entry->button != nullptr)
            removeChildComponent(entry->button.get());

        removeChildComponent(&pane);
        tabs.remove(i);

        if (activeIndex >= tabs.size())
            activeIndex = tabs.size() - 1;
        else if (activeIndex == i)
            activeIndex = juce::jlimit(-1, tabs.size() - 1, activeIndex);

        updateSelection();
        resized();
        return;
    }
}

void DockingWorkspace::DockGroup::setActivePane(juce::Component& pane)
{
    auto index = indexOfPane(pane);
    if (index < 0)
        return;

    activeIndex = index;
    updateSelection();
    resized();
}

juce::Component* DockingWorkspace::DockGroup::getActivePane() const noexcept
{
    return juce::isPositiveAndBelow(activeIndex, tabs.size()) ? tabs.getUnchecked(activeIndex)->pane : nullptr;
}

juce::Component* DockingWorkspace::DockGroup::getPaneAt(int index) const noexcept
{
    return juce::isPositiveAndBelow(index, tabs.size()) ? tabs.getUnchecked(index)->pane : nullptr;
}

int DockingWorkspace::DockGroup::indexOfPane(juce::Component& pane) const noexcept
{
    for (int i = 0; i < tabs.size(); ++i)
        if (tabs.getUnchecked(i)->pane == &pane)
            return i;

    return -1;
}

bool DockingWorkspace::DockGroup::containsScreenPoint(juce::Point<int> screenPoint) const
{
    return getScreenBounds().contains(screenPoint);
}

void DockingWorkspace::DockGroup::setHighlighted(bool shouldHighlight)
{
    highlighted = shouldHighlight;
    repaint();
}

void DockingWorkspace::DockGroup::paint(juce::Graphics& g)
{
    g.fillAll(zoneColour(zone));

    auto bounds = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(highlighted ? highlightColour() : juce::Colour(0xff263140));
    g.drawRoundedRectangle(bounds, 12.0f, highlighted ? 3.0f : 1.0f);
}

void DockingWorkspace::DockGroup::resized()
{
    auto area = getLocalBounds().reduced(10, 10);
    auto header = area.removeFromTop(30);

    auto tabWidth = juce::jlimit(72, 140, header.getWidth() / juce::jmax(1, tabs.size()));
    auto tabArea = header;

    for (int i = 0; i < tabs.size(); ++i)
    {
        auto* entry = tabs.getUnchecked(i);
        if (entry->button != nullptr)
        {
            entry->button->setVisible(true);
            entry->button->setBounds(tabArea.removeFromLeft(tabWidth).reduced(2, 0));
        }
    }

    auto* activePane = getActivePane();
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto* pane = tabs.getUnchecked(i)->pane;
        pane->setVisible(pane == activePane);
        if (pane == activePane)
            pane->setBounds(area);
    }
}

DockingWorkspace::DockingWorkspace()
{
    topGroup = std::make_unique<DockGroup>(); topGroup->initialise(*this, Zone::top); addAndMakeVisible(*topGroup);
    leftGroup = std::make_unique<DockGroup>(); leftGroup->initialise(*this, Zone::left); addAndMakeVisible(*leftGroup);
    centreGroup = std::make_unique<DockGroup>(); centreGroup->initialise(*this, Zone::centre); addAndMakeVisible(*centreGroup);
    rightGroup = std::make_unique<DockGroup>(); rightGroup->initialise(*this, Zone::right); addAndMakeVisible(*rightGroup);
    bottomGroup = std::make_unique<DockGroup>(); bottomGroup->initialise(*this, Zone::bottom); addAndMakeVisible(*bottomGroup);
}

DockingWorkspace::DockGroup& DockingWorkspace::getGroup(Zone zone)
{
    switch (zone)
    {
        case Zone::top: return *topGroup;
        case Zone::left: return *leftGroup;
        case Zone::centre: return *centreGroup;
        case Zone::right: return *rightGroup;
        case Zone::bottom: return *bottomGroup;
    }

    return *centreGroup;
}

const DockingWorkspace::DockGroup& DockingWorkspace::getGroup(Zone zone) const
{
    return const_cast<DockingWorkspace*>(this)->getGroup(zone);
}

void DockingWorkspace::addPane(juce::Component& pane, const juce::String& title, Zone zone)
{
    panes.add({ &pane, title, zone });
    getGroup(zone).addPane(pane, title);
    pane.setVisible(true);
}

void DockingWorkspace::movePaneToZone(juce::Component& pane, Zone zone)
{
    PaneEntry* entry = nullptr;
    for (auto& candidate : panes)
    {
        if (candidate.pane == &pane)
        {
            entry = &candidate;
            break;
        }
    }

    if (entry == nullptr || entry->zone == zone)
        return;

    getGroup(entry->zone).removePane(pane);
    entry->zone = zone;
    getGroup(zone).addPane(pane, entry->title);
    getGroup(zone).setActivePane(pane);
    repaint();
}

DockingWorkspace::Zone DockingWorkspace::getPaneZone(juce::Component& pane) const
{
    for (auto& candidate : panes)
        if (candidate.pane == &pane)
            return candidate.zone;

    return Zone::centre;
}

void DockingWorkspace::beginPaneDrag(juce::Component& pane, Zone fromZone)
{
    dragState.pane = &pane;
    dragState.sourceZone = fromZone;
    dragState.hoveredZone = fromZone;
    dragState.active = true;
    updateDragTarget(juce::Desktop::getInstance().getMainMouseSource().getScreenPosition().roundToInt());
}

void DockingWorkspace::updateDragTarget(juce::Point<int> screenPoint)
{
    if (! dragState.active)
        return;

    auto targetZone = findZoneAtScreenPoint(screenPoint);
    dragState.hoveredZone = targetZone;

    for (auto* group : { topGroup.get(), leftGroup.get(), centreGroup.get(), rightGroup.get(), bottomGroup.get() })
        group->setHighlighted(group == &getGroup(targetZone));
}

void DockingWorkspace::completePaneDrag(juce::Point<int> screenPoint)
{
    if (! dragState.active || dragState.pane == nullptr)
        return;

    auto targetZone = findZoneAtScreenPoint(screenPoint);
    if (targetZone != dragState.sourceZone)
        movePaneToZone(*dragState.pane, targetZone);

    clearDragState();
}

void DockingWorkspace::clearDragState()
{
    dragState = {};

    for (auto* group : { topGroup.get(), leftGroup.get(), centreGroup.get(), rightGroup.get(), bottomGroup.get() })
        group->setHighlighted(false);
}

DockingWorkspace::Zone DockingWorkspace::findZoneAtScreenPoint(juce::Point<int> screenPoint) const
{
    for (auto* group : { topGroup.get(), leftGroup.get(), centreGroup.get(), rightGroup.get(), bottomGroup.get() })
        if (group->containsScreenPoint(screenPoint))
            return group->getZone();

    return dragState.sourceZone;
}

void DockingWorkspace::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff0f1115));
}

void DockingWorkspace::paintOverChildren(juce::Graphics& g)
{
    if (! dragState.active)
        return;

    auto* group = &getGroup(dragState.hoveredZone);
    auto bounds = group->getBoundsInParent().toFloat();
    g.setColour(juce::Colour(0xff5f93ff).withAlpha(0.12f));
    g.fillRoundedRectangle(bounds.reduced(2.0f), 14.0f);
    g.setColour(juce::Colour(0xff5f93ff).withAlpha(0.75f));
    g.drawRoundedRectangle(bounds.reduced(2.0f), 14.0f, 2.0f);
}

void DockingWorkspace::resized()
{
    auto area = getLocalBounds().reduced(8);
    auto topArea = area.removeFromTop(165);
    topGroup->setBounds(topArea.reduced(4));

    auto bottomArea = area.removeFromBottom(185);
    bottomGroup->setBounds(bottomArea.reduced(4));

    auto middle = area;
    auto leftArea = middle.removeFromLeft(middle.getWidth() / 4);
    auto rightArea = middle.removeFromRight(middle.getWidth() / 4);
    leftGroup->setBounds(leftArea.reduced(4));
    rightGroup->setBounds(rightArea.reduced(4));
    centreGroup->setBounds(middle.reduced(4));
}
