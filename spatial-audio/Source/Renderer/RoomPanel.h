// Spatial Audio - the room editor: room size, speakers, types and outputs
#pragma once

#include "JuceIncludes.h"
#include "ControlList.h"
#include "RoomLayout.h"

namespace spatial
{

class RoomPanel : public juce::Component,
                  private juce::ListBoxModel
{
public:
    explicit RoomPanel (RoomLayout& roomToEdit);

    // Call after the room was replaced or changed elsewhere (e.g. dragged in the 3D view)
    void refreshAll();
    void refreshSelectedSpeaker();

    int getSelectedSpeaker() const { return selected; }
    void selectSpeaker (int index);

    void setDeviceOutputCount (int count);

    // Something changed that the engine and view need to know about
    std::function<void()> onRoomChanged;
    std::function<void (int index)> onSelectionChanged;
    std::function<void()> onLoadPresetClicked, onOpenClicked, onSaveClicked;

    void resized() override;

private:
    // juce::ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
    void selectedRowsChanged (int lastRowSelected) override;

    void addSpeaker (bool duplicateSelected);
    void removeSelected();
    void updateRanges();
    void updateWarning();
    void changed();

    RoomLayout& room;
    int selected = -1;
    int deviceOutputs = 0;
    bool updating = false;

    ScrollingPanel panel;
    juce::ListBox speakerList { "Speakers", this };

    juce::TextEditor* roomName = nullptr;
    juce::Slider *widthSlider = nullptr, *depthSlider = nullptr, *heightSlider = nullptr;
    juce::TextEditor* speakerName = nullptr;
    juce::ComboBox* speakerType = nullptr;
    juce::Slider *xSlider = nullptr, *ySlider = nullptr, *zSlider = nullptr, *outputSlider = nullptr, *trimSlider = nullptr;
    juce::Label* warning = nullptr;
    juce::Label* selectedHeadingNote = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomPanel)
};

} // namespace spatial
