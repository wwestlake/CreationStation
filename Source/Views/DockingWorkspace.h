#pragma once

#include <JuceHeader.h>

class DockingWorkspace final : public juce::Component
{
public:
    enum class Zone
    {
        top,
        left,
        centre,
        right,
        bottom
    };

    DockingWorkspace();

    void addPane(juce::Component& pane, const juce::String& title, Zone zone);
    void movePaneToZone(juce::Component& pane, Zone zone);
    Zone getPaneZone(juce::Component& pane) const;

    void resized() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    struct PaneEntry
    {
        juce::Component* pane = nullptr;
        juce::String title;
        Zone zone = Zone::centre;
    };

    class DockGroup final : public juce::Component
    {
    public:
        class TabButton final : public juce::Button
        {
        public:
            TabButton(DockGroup& ownerRef, juce::Component& paneRef, const juce::String& titleRef);

            void paintButton(juce::Graphics& g,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override;
            void mouseDown(const juce::MouseEvent& event) override;
            void mouseDrag(const juce::MouseEvent& event) override;
            void mouseUp(const juce::MouseEvent& event) override;

            void setSelected(bool shouldSelect);

        private:
            DockGroup& owner;
            juce::Component& pane;
            juce::String title;
            bool selected = false;
            bool dragStarted = false;
            juce::Point<int> mouseDownPosition;
        };

        DockGroup() = default;

        void initialise(DockingWorkspace& ownerRef, Zone zoneRef);
        void addPane(juce::Component& pane, const juce::String& title);
        void removePane(juce::Component& pane);
        void setActivePane(juce::Component& pane);
        juce::Component* getActivePane() const noexcept;
        juce::Component* getPaneAt(int index) const noexcept;
        int indexOfPane(juce::Component& pane) const noexcept;
        bool containsScreenPoint(juce::Point<int> screenPoint) const;
        void setHighlighted(bool shouldHighlight);
        Zone getZone() const noexcept { return zone; }

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        struct TabEntry
        {
            juce::Component* pane = nullptr;
            juce::String title;
            std::unique_ptr<TabButton> button;
        };

        DockingWorkspace* owner = nullptr;
        Zone zone = Zone::centre;
        juce::OwnedArray<TabEntry> tabs;
        int activeIndex = -1;
        bool highlighted = false;

        void updateSelection();
    };

    struct DragState
    {
        juce::Component* pane = nullptr;
        Zone sourceZone = Zone::centre;
        Zone hoveredZone = Zone::centre;
        bool active = false;
    };

    std::unique_ptr<DockGroup> topGroup;
    std::unique_ptr<DockGroup> leftGroup;
    std::unique_ptr<DockGroup> centreGroup;
    std::unique_ptr<DockGroup> rightGroup;
    std::unique_ptr<DockGroup> bottomGroup;

    juce::Array<PaneEntry> panes;
    DragState dragState;

    DockGroup& getGroup(Zone zone);
    const DockGroup& getGroup(Zone zone) const;

    void beginPaneDrag(juce::Component& pane, Zone fromZone);
    void updateDragTarget(juce::Point<int> screenPoint);
    void completePaneDrag(juce::Point<int> screenPoint);
    void clearDragState();
    Zone findZoneAtScreenPoint(juce::Point<int> screenPoint) const;
};
