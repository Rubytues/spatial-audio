// Spatial Audio - the renderer's main window contents
#pragma once

#include "JuceIncludes.h"
#include "AudioEngine.h"
#include "ControlList.h"
#include "RoomLayout.h"
#include "RoomPanel.h"
#include "RoomView3D.h"

namespace spatial
{

class MainComponent : public juce::Component,
                      private juce::ChangeListener,
                      private juce::Timer
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void buildSoundPanel();
    void buildPathPanel();
    void pushSettings();
    void roomChanged();
    void applyRoom (const RoomLayout& newRoom);
    void updateSpiralRows();
    void updateSpeedRows();
    void updateDeviceStatus();
    void enableAllOutputsIfNewDevice();
    void showAudioSettings();
    void showPresetMenu();
    void openRoomFile();
    void saveRoomFile();
    void saveState();

    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    juce::AudioDeviceManager deviceManager;
    AudioEngine engine;
    AudioEngine::Settings settings;
    RoomLayout room;

    std::unique_ptr<juce::PropertiesFile> properties;
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::String lastDeviceName;

    // Always-visible controls
    juce::TextButton playButton { "Play test sound" };
    juce::TextButton speakerTestButton { "Test each speaker" };
    juce::TextButton audioSettingsButton { "Audio settings..." };
    juce::Slider masterSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label masterLabel { {}, "Master level" };
    juce::Label deviceStatus;

    ScrollingPanel soundPanel, pathPanel;
    RoomPanel roomPanel { room };
    juce::TabbedComponent tabs { juce::TabbedButtonBar::TabsAtTop }; // after the panels it shows
    RoomView3D view { engine };

    // Pathway rows that come and go
    juce::Slider *spiralTurns = nullptr, *spiralInner = nullptr, *spiralRise = nullptr;
    juce::Slider *secondsSlider = nullptr, *bpmSlider = nullptr;
    juce::ComboBox* barsCombo = nullptr;

    bool saveScheduled = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

} // namespace spatial
