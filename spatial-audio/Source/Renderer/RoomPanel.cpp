#include "RoomPanel.h"
#include "RoomView3D.h" // colours

namespace spatial
{

RoomPanel::RoomPanel (RoomLayout& r) : room (r)
{
    auto& list = panel.list();

    list.addHeading ("Room");
    list.addButtonRow ({ { "Load preset...", [this] { if (onLoadPresetClicked) onLoadPresetClicked(); } },
                         { "Open...",        [this] { if (onOpenClicked) onOpenClicked(); } },
                         { "Save...",        [this] { if (onSaveClicked) onSaveClicked(); } } });

    roomName = &list.addTextField ("Name", room.name, [this] (const juce::String& t)
    {
        if (updating) return;
        room.name = t;
        changed();
    });

    widthSlider = &list.addSlider ("Width", 1.0, 60.0, 0.1, room.width, " m", [this] (double v)
    {
        if (updating) return;
        room.width = (float) v;
        updateRanges();
        changed();
    }, 12.0);

    depthSlider = &list.addSlider ("Depth", 1.0, 60.0, 0.1, room.depth, " m", [this] (double v)
    {
        if (updating) return;
        room.depth = (float) v;
        updateRanges();
        changed();
    }, 12.0);

    heightSlider = &list.addSlider ("Height", 1.0, 30.0, 0.1, room.height, " m", [this] (double v)
    {
        if (updating) return;
        room.height = (float) v;
        updateRanges();
        changed();
    }, 6.0);

    list.addHeading ("Speakers");
    speakerList.setRowHeight (24);
    speakerList.setColour (juce::ListBox::backgroundColourId, colours::background);
    speakerList.setColour (juce::ListBox::outlineColourId, colours::grid);
    speakerList.setOutlineThickness (1);
    list.addCustom (speakerList, 220);

    list.addButtonRow ({ { "Add",       [this] { addSpeaker (false); } },
                         { "Duplicate", [this] { addSpeaker (true); } },
                         { "Remove",    [this] { removeSelected(); } } });

    warning = &list.addNote ({}, 34);
    warning->setColour (juce::Label::textColourId, colours::sub);

    list.addHeading ("Selected speaker");
    selectedHeadingNote = &list.addNote ("Click a speaker in the list or the 3D view.", 20);

    speakerName = &list.addTextField ("Name", {}, [this] (const juce::String& t)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].name = t;
        speakerList.repaint();
        changed();
    });

    speakerType = &list.addCombo ("Type", { "Regular", "Sub", "Transducer" }, 0, [this] (int index)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].type = (SpeakerType) index;
        speakerList.repaint();
        changed();
    });

    xSlider = &list.addSlider ("Left / right", -3.0, 3.0, 0.01, 0.0, " m", [this] (double v)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].position.x = (float) v;
        speakerList.repaint();
        changed();
    });

    ySlider = &list.addSlider ("Back / front", -3.0, 3.0, 0.01, 0.0, " m", [this] (double v)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].position.y = (float) v;
        speakerList.repaint();
        changed();
    });

    zSlider = &list.addSlider ("Height", 0.0, 3.0, 0.01, 1.2, " m", [this] (double v)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].position.z = (float) v;
        speakerList.repaint();
        changed();
    });

    outputSlider = &list.addSlider ("Output", 1.0, 64.0, 1.0, 1.0, {}, [this] (double v)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].output = (int) v;
        speakerList.repaint();
        updateWarning();
        changed();
    });
    outputSlider->setSliderStyle (juce::Slider::IncDecButtons);
    outputSlider->setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);

    trimSlider = &list.addSlider ("Level trim", -24.0, 12.0, 0.5, 0.0, " dB", [this] (double v)
    {
        if (updating || selected < 0) return;
        room.speakers[(size_t) selected].trimDb = (float) v;
        changed();
    });

    list.addNote ("Positions are in metres from the centre of the room. Height is measured from the floor. "
                  "You can also drag speakers in the 3D view (hold Shift to change height).", 64);

    addAndMakeVisible (panel);
    refreshAll();
}

void RoomPanel::resized()
{
    panel.setBounds (getLocalBounds());
}

void RoomPanel::changed()
{
    if (onRoomChanged)
        onRoomChanged();
}

void RoomPanel::updateRanges()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    xSlider->setRange (-room.width * 0.5, room.width * 0.5, 0.01);
    ySlider->setRange (-room.depth * 0.5, room.depth * 0.5, 0.01);
    zSlider->setRange (0.0, room.height, 0.01);
}

void RoomPanel::setDeviceOutputCount (int count)
{
    deviceOutputs = count;
    updateWarning();
    speakerList.repaint();
}

void RoomPanel::updateWarning()
{
    juce::StringArray problems;

    if (deviceOutputs > 0)
    {
        int unreachable = 0;
        for (const auto& s : room.speakers)
            if (s.output > deviceOutputs)
                ++unreachable;

        if (unreachable > 0)
            problems.add (juce::String (unreachable) + " speaker(s) use outputs above " + juce::String (deviceOutputs)
                          + ", the number your interface has turned on. Check Audio settings.");
    }
    else
    {
        problems.add ("No audio output is open. Choose your interface in Audio settings.");
    }

    // Two speakers on the same output
    for (size_t i = 0; i < room.speakers.size(); ++i)
        for (size_t j = i + 1; j < room.speakers.size(); ++j)
            if (room.speakers[i].output == room.speakers[j].output)
            {
                problems.add ("\"" + room.speakers[i].name + "\" and \"" + room.speakers[j].name
                              + "\" share output " + juce::String (room.speakers[i].output) + ".");
                i = room.speakers.size();
                break;
            }

    warning->setText (problems.joinIntoString (" "), juce::dontSendNotification);
}

void RoomPanel::refreshAll()
{
    {
        const juce::ScopedValueSetter<bool> svs (updating, true);
        roomName->setText (room.name, false);
        widthSlider->setValue (room.width, juce::dontSendNotification);
        depthSlider->setValue (room.depth, juce::dontSendNotification);
        heightSlider->setValue (room.height, juce::dontSendNotification);
    }

    updateRanges();

    if (selected >= (int) room.speakers.size())
        selected = room.speakers.empty() ? -1 : 0;

    speakerList.updateContent();
    speakerList.repaint();

    if (selected >= 0)
        speakerList.selectRow (selected, false, true);
    else
        speakerList.deselectAllRows();

    refreshSelectedSpeaker();
    updateWarning();
}

void RoomPanel::refreshSelectedSpeaker()
{
    const juce::ScopedValueSetter<bool> svs (updating, true);
    const bool has = selected >= 0 && selected < (int) room.speakers.size();

    for (juce::Component* c : { (juce::Component*) speakerName, (juce::Component*) speakerType, (juce::Component*) xSlider,
                                (juce::Component*) ySlider, (juce::Component*) zSlider, (juce::Component*) outputSlider,
                                (juce::Component*) trimSlider })
        c->setEnabled (has);

    selectedHeadingNote->setText (has ? juce::String() : "Click a speaker in the list or the 3D view.",
                                  juce::dontSendNotification);

    if (! has)
        return;

    const auto& s = room.speakers[(size_t) selected];
    speakerName->setText (s.name, false);
    speakerType->setSelectedItemIndex ((int) s.type, juce::dontSendNotification);
    xSlider->setValue (s.position.x, juce::dontSendNotification);
    ySlider->setValue (s.position.y, juce::dontSendNotification);
    zSlider->setValue (s.position.z, juce::dontSendNotification);
    outputSlider->setValue (s.output, juce::dontSendNotification);
    trimSlider->setValue (s.trimDb, juce::dontSendNotification);
    speakerList.repaintRow (selected);
}

void RoomPanel::selectSpeaker (int index)
{
    if (index < 0 || index >= (int) room.speakers.size())
        index = -1;

    selected = index;

    if (index >= 0)
    {
        speakerList.selectRow (index, false, true);
        speakerList.scrollToEnsureRowIsOnscreen (index);
    }
    else
    {
        speakerList.deselectAllRows();
    }

    refreshSelectedSpeaker();
}

void RoomPanel::addSpeaker (bool duplicateSelected)
{
    Speaker s;

    if (duplicateSelected && selected >= 0)
    {
        s = room.speakers[(size_t) selected];
        s.name = s.name + " copy";
        s.position.x = juce::jlimit (-room.width * 0.5f, room.width * 0.5f, s.position.x + 0.5f);
    }
    else
    {
        s.name = "Speaker " + juce::String ((int) room.speakers.size() + 1);
        s.position = { 0.0f, juce::jmin (1.5f, room.depth * 0.5f), 1.2f };
    }

    s.output = room.nextFreeOutput();
    room.speakers.push_back (s);
    speakerList.updateContent();
    selectSpeaker ((int) room.speakers.size() - 1);
    updateWarning();
    changed();

    if (onSelectionChanged)
        onSelectionChanged (selected);
}

void RoomPanel::removeSelected()
{
    if (selected < 0 || selected >= (int) room.speakers.size())
        return;

    room.speakers.erase (room.speakers.begin() + selected);
    speakerList.updateContent();
    selectSpeaker (juce::jmin (selected, (int) room.speakers.size() - 1));
    updateWarning();
    changed();

    if (onSelectionChanged)
        onSelectionChanged (selected);
}

//==============================================================================
int RoomPanel::getNumRows()
{
    return (int) room.speakers.size();
}

void RoomPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool isSelected)
{
    if (row < 0 || row >= (int) room.speakers.size())
        return;

    const auto& s = room.speakers[(size_t) row];

    if (isSelected)
        g.fillAll (colours::roomEdge.withAlpha (0.5f));

    const auto colour = colours::forSpeaker (s.type);
    const bool unreachable = deviceOutputs > 0 && s.output > deviceOutputs;

    auto area = juce::Rectangle<int> (0, 0, width, height).reduced (6, 0);

    g.setColour (unreachable ? juce::Colours::grey : colour);
    g.fillEllipse (area.removeFromLeft (10).toFloat().withSizeKeepingCentre (8.0f, 8.0f));
    area.removeFromLeft (6);

    g.setFont (juce::FontOptions (12.5f, juce::Font::bold));
    g.drawText (juce::String (s.output), area.removeFromLeft (24), juce::Justification::centredLeft);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (12.5f));
    auto coords = area.removeFromRight (122);
    g.drawText (s.name, area, juce::Justification::centredLeft, true);

    g.setColour (colours::dimText);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (s.position.x, 1) + ", " + juce::String (s.position.y, 1) + ", " + juce::String (s.position.z, 1),
                coords, juce::Justification::centredRight);
}

void RoomPanel::selectedRowsChanged (int lastRowSelected)
{
    if (lastRowSelected == selected)
        return;

    selected = lastRowSelected;
    refreshSelectedSpeaker();

    if (onSelectionChanged)
        onSelectionChanged (selected);
}

} // namespace spatial
