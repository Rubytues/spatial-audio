#include "ControlList.h"
#include "RoomView3D.h" // colours

namespace spatial
{

ControlList::ControlList() = default;

ControlList::Row& ControlList::newRow (int height)
{
    rows.push_back ({});
    rows.back().height = height;
    return rows.back();
}

juce::Label* ControlList::makeLabel (const juce::String& text)
{
    auto* label = own (new juce::Label ({}, text));
    label->setColour (juce::Label::textColourId, colours::dimText);
    label->setFont (juce::FontOptions (13.0f));
    label->setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (label);
    return label;
}

void ControlList::addHeading (const juce::String& text)
{
    auto& row = newRow (30);
    row.isHeading = true;
    row.fullWidth = true;
    auto* label = makeLabel (text.toUpperCase());
    label->setColour (juce::Label::textColourId, colours::text);
    label->setFont (juce::FontOptions (12.0f, juce::Font::bold));
    label->setJustificationType (juce::Justification::bottomLeft);
    row.components.push_back (label);
}

juce::Label& ControlList::addNote (const juce::String& text, int height)
{
    auto& row = newRow (height);
    row.fullWidth = true;
    auto* label = makeLabel (text);
    label->setFont (juce::FontOptions (12.0f));
    label->setJustificationType (juce::Justification::topLeft);
    label->setMinimumHorizontalScale (1.0f);
    row.components.push_back (label);
    return *label;
}

juce::Slider& ControlList::addSlider (const juce::String& labelText, double min, double max, double interval,
                                      double value, const juce::String& suffix,
                                      std::function<void (double)> onChange, double skewMidPoint)
{
    auto& row = newRow (26);
    row.label = makeLabel (labelText);

    auto* slider = own (new juce::Slider (juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight));
    slider->setRange (min, max, interval);
    if (skewMidPoint > min && skewMidPoint < max)
        slider->setSkewFactorFromMidPoint (skewMidPoint);
    slider->setTextValueSuffix (suffix);
    slider->setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    slider->setValue (value, juce::dontSendNotification);
    slider->setDoubleClickReturnValue (true, value);
    slider->setColour (juce::Slider::trackColourId, colours::regular.withAlpha (0.6f));
    slider->setColour (juce::Slider::thumbColourId, colours::regular);
    slider->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider->setColour (juce::Slider::textBoxTextColourId, colours::text);
    slider->onValueChange = [slider, onChange] { if (onChange) onChange (slider->getValue()); };
    addAndMakeVisible (slider);
    row.components.push_back (slider);
    return *slider;
}

juce::ComboBox& ControlList::addCombo (const juce::String& labelText, const juce::StringArray& items, int selectedIndex,
                                       std::function<void (int)> onChange)
{
    auto& row = newRow (26);
    row.label = makeLabel (labelText);

    auto* combo = own (new juce::ComboBox());
    combo->addItemList (items, 1);
    combo->setSelectedItemIndex (selectedIndex, juce::dontSendNotification);
    combo->onChange = [combo, onChange] { if (onChange) onChange (combo->getSelectedItemIndex()); };
    addAndMakeVisible (combo);
    row.components.push_back (combo);
    return *combo;
}

juce::ToggleButton& ControlList::addToggle (const juce::String& text, bool initialState, std::function<void (bool)> onChange)
{
    auto& row = newRow (26);
    row.fullWidth = true;
    auto* toggle = own (new juce::ToggleButton (text));
    toggle->setToggleState (initialState, juce::dontSendNotification);
    toggle->setColour (juce::ToggleButton::textColourId, colours::text);
    toggle->setColour (juce::ToggleButton::tickColourId, colours::regular);
    toggle->onClick = [toggle, onChange] { if (onChange) onChange (toggle->getToggleState()); };
    addAndMakeVisible (toggle);
    row.components.push_back (toggle);
    return *toggle;
}

juce::TextEditor& ControlList::addTextField (const juce::String& labelText, const juce::String& text,
                                             std::function<void (const juce::String&)> onChange)
{
    auto& row = newRow (26);
    row.label = makeLabel (labelText);
    auto* editor = own (new juce::TextEditor());
    editor->setText (text, false);
    editor->onTextChange = [editor, onChange] { if (onChange) onChange (editor->getText()); };
    editor->onReturnKey = [editor] { editor->unfocusAllComponents(); };
    addAndMakeVisible (editor);
    row.components.push_back (editor);
    return *editor;
}

std::vector<juce::TextButton*> ControlList::addButtonRow (const std::vector<std::pair<juce::String, std::function<void()>>>& buttons,
                                                          int height)
{
    auto& row = newRow (height);
    row.fullWidth = true;
    std::vector<juce::TextButton*> result;

    for (const auto& [text, fn] : buttons)
    {
        auto* b = own (new juce::TextButton (text));
        b->onClick = fn;
        addAndMakeVisible (b);
        row.components.push_back (b);
        result.push_back (b);
    }

    return result;
}

void ControlList::addCustom (juce::Component& component, int height)
{
    auto& row = newRow (height);
    row.fullWidth = true;
    addAndMakeVisible (component);
    row.components.push_back (&component);
}

void ControlList::setRowVisible (juce::Component& anyComponentInRow, bool visible)
{
    for (auto& row : rows)
    {
        if (std::find (row.components.begin(), row.components.end(), &anyComponentInRow) == row.components.end())
            continue;

        if (row.visible == visible)
            return;

        row.visible = visible;
        if (row.label != nullptr)
            row.label->setVisible (visible);
        for (auto* c : row.components)
            c->setVisible (visible);

        if (onLayoutChanged)
            onLayoutChanged();
        resized();
        return;
    }
}

int ControlList::getIdealHeight() const
{
    int h = 8;
    for (const auto& row : rows)
        if (row.visible)
            h += row.height + gap;
    return h + 12;
}

void ControlList::resized()
{
    int y = 8;
    const int width = getWidth() - 20;

    for (const auto& row : rows)
    {
        if (! row.visible)
            continue;

        juce::Rectangle<int> area (10, y, width, row.height);
        y += row.height + gap;

        if (row.label != nullptr)
            row.label->setBounds (area.removeFromLeft (labelWidth));

        if (row.components.size() == 1)
        {
            row.components.front()->setBounds (area);
            continue;
        }

        const int n = (int) row.components.size();
        const int each = (area.getWidth() - gap * (n - 1)) / juce::jmax (1, n);
        for (auto* c : row.components)
        {
            c->setBounds (area.removeFromLeft (each));
            area.removeFromLeft (gap);
        }
    }
}

//==============================================================================
ScrollingPanel::ScrollingPanel()
{
    viewport.setViewedComponent (&controls, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
    controls.onLayoutChanged = [this] { resized(); };
}

void ScrollingPanel::resized()
{
    viewport.setBounds (getLocalBounds());
    controls.setSize (viewport.getMaximumVisibleWidth(), controls.getIdealHeight());
}

} // namespace spatial
