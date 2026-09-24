// Spatial Audio - a simple vertical list of labelled controls
#pragma once

#include "JuceIncludes.h"

namespace spatial
{

class ControlList : public juce::Component
{
public:
    ControlList();

    void addHeading (const juce::String& text);
    juce::Label& addNote (const juce::String& text, int height = 34);

    juce::Slider& addSlider (const juce::String& label, double min, double max, double interval,
                             double value, const juce::String& suffix,
                             std::function<void (double)> onChange, double skewMidPoint = 0.0);

    juce::ComboBox& addCombo (const juce::String& label, const juce::StringArray& items, int selectedIndex,
                              std::function<void (int)> onChange);

    juce::ToggleButton& addToggle (const juce::String& text, bool initialState, std::function<void (bool)> onChange);

    juce::TextEditor& addTextField (const juce::String& label, const juce::String& text,
                                    std::function<void (const juce::String&)> onChange);

    std::vector<juce::TextButton*> addButtonRow (const std::vector<std::pair<juce::String, std::function<void()>>>& buttons,
                                                  int height = 28);

    void addCustom (juce::Component& component, int height);

    // Show or hide the row that holds this component
    void setRowVisible (juce::Component& anyComponentInRow, bool visible);

    int getIdealHeight() const;
    void resized() override;

    std::function<void()> onLayoutChanged;

private:
    struct Row
    {
        juce::Label* label = nullptr;
        std::vector<juce::Component*> components;
        int height = 26;
        bool visible = true;
        bool isHeading = false;
        bool fullWidth = false;
    };

    Row& newRow (int height);
    template <typename T> T* own (T* c) { owned.add (c); return c; }
    juce::Label* makeLabel (const juce::String& text);

    std::vector<Row> rows;
    juce::OwnedArray<juce::Component> owned;

    static constexpr int labelWidth = 104;
    static constexpr int gap = 5;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ControlList)
};

// A ControlList that scrolls when it's taller than the space available
class ScrollingPanel : public juce::Component
{
public:
    ScrollingPanel();
    ControlList& list() { return controls; }
    void resized() override;

private:
    juce::Viewport viewport;
    ControlList controls;
};

} // namespace spatial
