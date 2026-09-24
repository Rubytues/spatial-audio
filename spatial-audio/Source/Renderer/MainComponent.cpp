#include "MainComponent.h"

namespace spatial
{

namespace
{
    const float barChoices[] = { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f, 16.0f, 32.0f };

    juce::PropertiesFile::Options propertiesOptions()
    {
        juce::PropertiesFile::Options o;
        o.applicationName = "Spatial Renderer";
        o.filenameSuffix = ".settings";
        o.folderName = "Spatial Renderer";
        o.osxLibrarySubFolder = "Application Support";
        return o;
    }

    juce::File defaultRoomsFolder()
    {
        return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Spatial Audio Rooms");
    }

    void styleButton (juce::TextButton& b, juce::Colour onColour)
    {
        b.setColour (juce::TextButton::buttonColourId, colours::panel.brighter (0.15f));
        b.setColour (juce::TextButton::buttonOnColourId, onColour.darker (0.3f));
        b.setColour (juce::TextButton::textColourOffId, colours::text);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
    }
}

MainComponent::MainComponent()
{
    properties = std::make_unique<juce::PropertiesFile> (propertiesOptions());

    // Room: last one used, or the first preset
    {
        juce::String error;
        RoomLayout saved;
        const auto json = properties->getValue ("room");
        if (json.isNotEmpty() && RoomLayout::fromJson (json, saved, error))
            room = saved;
        else
            room = RoomLayout::builtInPresets().front();
    }

    // Always-visible controls
    styleButton (playButton, colours::source);
    playButton.setClickingTogglesState (true);
    playButton.onClick = [this]
    {
        if (playButton.getToggleState())
        {
            engine.setSpeakerTest (false);
            speakerTestButton.setToggleState (false, juce::dontSendNotification);
        }
        engine.setPlaying (playButton.getToggleState());
    };
    addAndMakeVisible (playButton);

    styleButton (speakerTestButton, colours::regular);
    speakerTestButton.setClickingTogglesState (true);
    speakerTestButton.onClick = [this]
    {
        const bool on = speakerTestButton.getToggleState();
        if (on)
        {
            playButton.setToggleState (false, juce::dontSendNotification);
            engine.setPlaying (false);
        }
        engine.setSpeakerTest (on);
    };
    addAndMakeVisible (speakerTestButton);

    styleButton (audioSettingsButton, colours::regular);
    audioSettingsButton.onClick = [this] { showAudioSettings(); };
    addAndMakeVisible (audioSettingsButton);

    masterLabel.setColour (juce::Label::textColourId, colours::dimText);
    addAndMakeVisible (masterLabel);
    masterSlider.setRange (-60.0, 0.0, 0.5);
    masterSlider.setTextValueSuffix (" dB");
    masterSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 72, 20);
    masterSlider.setColour (juce::Slider::trackColourId, colours::source.withAlpha (0.6f));
    masterSlider.setColour (juce::Slider::thumbColourId, colours::source);
    masterSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    masterSlider.setValue (properties->getDoubleValue ("masterDb", -24.0), juce::dontSendNotification);
    masterSlider.onValueChange = [this] { engine.setMasterLevelDb ((float) masterSlider.getValue()); saveScheduled = true; };
    engine.setMasterLevelDb ((float) masterSlider.getValue());
    addAndMakeVisible (masterSlider);

    deviceStatus.setColour (juce::Label::textColourId, colours::dimText);
    deviceStatus.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (deviceStatus);

    // Tabs
    buildSoundPanel();
    buildPathPanel();

    const auto tabColour = colours::panel;
    tabs.addTab ("Sound", tabColour, &soundPanel, false);
    tabs.addTab ("Pathway", tabColour, &pathPanel, false);
    tabs.addTab ("Room", tabColour, &roomPanel, false);
    tabs.setTabBarDepth (32);
    tabs.setOutline (0);
    tabs.setCurrentTabIndex (properties->getIntValue ("tab", 1));
    addAndMakeVisible (tabs);

    // Room editor
    roomPanel.onRoomChanged = [this] { roomChanged(); };
    roomPanel.onSelectionChanged = [this] (int index) { view.setSelectedSpeaker (index); };
    roomPanel.onLoadPresetClicked = [this] { showPresetMenu(); };
    roomPanel.onOpenClicked = [this] { openRoomFile(); };
    roomPanel.onSaveClicked = [this] { saveRoomFile(); };

    // 3D view
    view.onSpeakerSelected = [this] (int index)
    {
        roomPanel.selectSpeaker (index);
        tabs.setCurrentTabIndex (2);
    };
    view.onSpeakerMoved = [this] (int index, Vec3 position)
    {
        if (index >= 0 && index < (int) room.speakers.size())
        {
            room.speakers[(size_t) index].position = position;
            roomPanel.refreshSelectedSpeaker();
            engine.setRoom (room);
        }
    };
    view.onSpeakerDragEnded = [this] { roomChanged(); };
    addAndMakeVisible (view);

    applyRoom (room);
    pushSettings();

    // Audio: open the last device, or the default one
    lastDeviceName = properties->getValue ("lastDevice");
    std::unique_ptr<juce::XmlElement> savedAudio (properties->getXmlValue ("audioDevice"));
    const auto error = deviceManager.initialise (0, 128, savedAudio.get(), true);
    if (error.isNotEmpty())
    {
        DBG ("Audio device error: " + error);
    }

    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (&engine);
    enableAllOutputsIfNewDevice();
    updateDeviceStatus();

    startTimerHz (10);
    setSize (1320, 820);
}

MainComponent::~MainComponent()
{
    saveState();
    deviceManager.removeAudioCallback (&engine);
    deviceManager.removeChangeListener (this);
}

//==============================================================================
void MainComponent::buildSoundPanel()
{
    auto& list = soundPanel.list();

    list.addHeading ("Test sound");
    list.addCombo ("Sound", { "Pink noise", "Pink noise bursts", "Tone 440 Hz", "Bass tone 60 Hz" }, 1,
                   [this] (int i) { engine.setTestSound ((AudioEngine::TestSound) i); });
    engine.setTestSound (AudioEngine::TestSound::pinkBursts);

    list.addHeading ("Speaker types");
    list.addSlider ("Regular level", -40.0, 12.0, 0.5, 0.0, " dB", [this] (double v)
                    { settings.regularLevelDb = (float) v; pushSettings(); });
    list.addSlider ("Sub level", -40.0, 12.0, 0.5, 0.0, " dB", [this] (double v)
                    { settings.subLevelDb = (float) v; pushSettings(); });
    list.addSlider ("Sub crossover", 40.0, 250.0, 1.0, settings.subCrossoverHz, " Hz", [this] (double v)
                    { settings.subCrossoverHz = (float) v; pushSettings(); });
    list.addSlider ("Transducer level", -40.0, 12.0, 0.5, 0.0, " dB", [this] (double v)
                    { settings.transducerLevelDb = (float) v; pushSettings(); });
    list.addSlider ("Transducer range", 20.0, 400.0, 1.0, settings.transducerCutoffHz, " Hz", [this] (double v)
                    { settings.transducerCutoffHz = (float) v; pushSettings(); });
    list.addNote ("Subs get everything below the crossover. Transducers get everything below their range "
                  "and follow the sound between them, so vibration moves with it.", 50);

    list.addHeading ("Panning");
    list.addSlider ("Focus", 3.0, 18.0, 0.5, settings.dbap.rolloffDb, " dB", [this] (double v)
                    { settings.dbap.rolloffDb = (float) v; pushSettings(); });
    list.addSlider ("Blur", 0.0, 3.0, 0.01, settings.dbap.blur, " m", [this] (double v)
                    { settings.dbap.blur = (float) v; pushSettings(); });
    list.addNote ("Focus: higher keeps the sound on the nearest speakers, lower spreads it wider. "
                  "Blur: softens the sound so it never snaps to a single speaker.", 50);
}

void MainComponent::buildPathPanel()
{
    auto& list = pathPanel.list();
    auto& p = settings.path;

    list.addHeading ("Pathway");
    list.addCombo ("Shape", { "None (stay still)", "Line", "Circle", "Triangle", "Square", "Figure eight", "Spiral" },
                   (int) p.shape, [this] (int i)
                   {
                       settings.path.shape = (PathShape) i;
                       updateSpiralRows();
                       pushSettings();
                   });
    list.addCombo ("Direction", { "Forward", "Reverse", "Back and forth" }, (int) p.direction, [this] (int i)
                   { settings.path.direction = (PathDirection) i; pushSettings(); });
    list.addButtonRow ({ { "Restart from the beginning", [this] { engine.restartPath(); } } }, 26);

    list.addHeading ("Speed");
    list.addToggle ("Lock to tempo", settings.speed.tempoSync, [this] (bool on)
                    {
                        settings.speed.tempoSync = on;
                        updateSpeedRows();
                        pushSettings();
                    });
    secondsSlider = &list.addSlider ("Seconds / pass", 0.5, 120.0, 0.1, settings.speed.secondsPerPass, " s",
                                     [this] (double v) { settings.speed.secondsPerPass = (float) v; pushSettings(); }, 10.0);
    bpmSlider = &list.addSlider ("Tempo", 40.0, 240.0, 0.1, settings.speed.bpm, " BPM",
                                 [this] (double v) { settings.speed.bpm = (float) v; pushSettings(); });
    barsCombo = &list.addCombo ("Bars / pass", { "1/4 bar", "1/2 bar", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars", "32 bars" },
                                4, [this] (int i) { settings.speed.barsPerPass = barChoices[juce::jlimit (0, 7, i)]; pushSettings(); });

    list.addHeading ("Position and size");
    list.addSlider ("Centre L / R", -15.0, 15.0, 0.01, p.centre.x, " m", [this] (double v)
                    { settings.path.centre.x = (float) v; pushSettings(); });
    list.addSlider ("Centre B / F", -15.0, 15.0, 0.01, p.centre.y, " m", [this] (double v)
                    { settings.path.centre.y = (float) v; pushSettings(); });
    list.addSlider ("Centre height", 0.0, 15.0, 0.01, p.centre.z, " m", [this] (double v)
                    { settings.path.centre.z = (float) v; pushSettings(); });
    list.addSlider ("Size", 0.05, 15.0, 0.01, p.size, " m", [this] (double v)
                    { settings.path.size = (float) v; pushSettings(); }, 2.0);
    list.addSlider ("Stretch", 0.1, 3.0, 0.01, p.stretch, " x", [this] (double v)
                    { settings.path.stretch = (float) v; pushSettings(); }, 1.0);
    list.addSlider ("Tilt", -90.0, 90.0, 1.0, p.tiltDegrees, juce::CharPointer_UTF8 ("\xc2\xb0"), [this] (double v)
                    { settings.path.tiltDegrees = (float) v; pushSettings(); });
    list.addSlider ("Rotation", -180.0, 180.0, 1.0, p.rotationDegrees, juce::CharPointer_UTF8 ("\xc2\xb0"), [this] (double v)
                    { settings.path.rotationDegrees = (float) v; pushSettings(); });
    list.addSlider ("Start point", 0.0, 1.0, 0.01, p.startOffset, {}, [this] (double v)
                    { settings.path.startOffset = (float) v; pushSettings(); });

    spiralTurns = &list.addSlider ("Spiral turns", 0.5, 12.0, 0.1, p.spiralTurns, {}, [this] (double v)
                                   { settings.path.spiralTurns = (float) v; pushSettings(); });
    spiralInner = &list.addSlider ("Spiral inner", 0.0, 1.0, 0.01, p.spiralInner, " x", [this] (double v)
                                   { settings.path.spiralInner = (float) v; pushSettings(); });
    spiralRise = &list.addSlider ("Spiral rise", -10.0, 10.0, 0.01, p.spiralRise, " m", [this] (double v)
                                  { settings.path.spiralRise = (float) v; pushSettings(); });

    list.addNote ("Tilt 0 lies flat like a tabletop, 90 stands up like a wheel. A spiral starts on the outside and "
                  "winds in; set Direction to Reverse to wind out, or Back and forth for both. "
                  "Spiral rise lifts it into a corkscrew (negative goes down).", 80);

    updateSpiralRows();
    updateSpeedRows();
}

void MainComponent::updateSpiralRows()
{
    const bool spiral = settings.path.shape == PathShape::spiral;
    auto& list = pathPanel.list();
    list.setRowVisible (*spiralTurns, spiral);
    list.setRowVisible (*spiralInner, spiral);
    list.setRowVisible (*spiralRise, spiral);
}

void MainComponent::updateSpeedRows()
{
    const bool sync = settings.speed.tempoSync;
    auto& list = pathPanel.list();
    list.setRowVisible (*secondsSlider, ! sync);
    list.setRowVisible (*bpmSlider, sync);
    list.setRowVisible (*barsCombo, sync);
}

void MainComponent::pushSettings()
{
    engine.setSettings (settings);
    view.setPath (settings.path);
}

void MainComponent::roomChanged()
{
    engine.setRoom (room);
    view.setRoom (room);
    saveScheduled = true;
}

void MainComponent::applyRoom (const RoomLayout& newRoom)
{
    room = newRoom;
    engine.setRoom (room);
    view.setRoom (room);
    view.setSelectedSpeaker (-1);
    roomPanel.selectSpeaker (-1);
    roomPanel.refreshAll();
    saveScheduled = true;
}

//==============================================================================
void MainComponent::showPresetMenu()
{
    juce::PopupMenu menu;
    const auto presets = RoomLayout::builtInPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
        menu.addItem (i + 1, presets[(size_t) i].name);

    menu.showMenuAsync (juce::PopupMenu::Options(), [this, presets] (int result)
    {
        if (result <= 0)
            return;

        const auto chosen = presets[(size_t) result - 1];
        juce::AlertWindow::showOkCancelBox (juce::MessageBoxIconType::QuestionIcon, "Load preset",
                                            "Replace the current room with \"" + chosen.name + "\"?\n"
                                            "Save your current room first if you want to keep it.",
                                            "Replace", "Cancel", this,
                                            juce::ModalCallbackFunction::create ([this, chosen] (int ok)
                                            {
                                                if (ok != 0)
                                                    applyRoom (chosen);
                                            }));
    });
}

void MainComponent::openRoomFile()
{
    defaultRoomsFolder().createDirectory();
    fileChooser = std::make_unique<juce::FileChooser> ("Open a room", defaultRoomsFolder(), "*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (file == juce::File())
            return;

        RoomLayout loaded;
        juce::String error;
        if (RoomLayout::fromJson (file.loadFileAsString(), loaded, error))
            applyRoom (loaded);
        else
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't open room", error);
    });
}

void MainComponent::saveRoomFile()
{
    defaultRoomsFolder().createDirectory();
    const auto suggested = defaultRoomsFolder().getChildFile (juce::File::createLegalFileName (room.name) + ".json");
    fileChooser = std::make_unique<juce::FileChooser> ("Save this room", suggested, "*.json");
    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("json"))
            file = file.withFileExtension ("json");

        if (! file.replaceWithText (room.toJson()))
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Couldn't save",
                                                    "The room couldn't be saved to " + file.getFullPathName());
    });
}

//==============================================================================
void MainComponent::showAudioSettings()
{
    auto* selector = new juce::AudioDeviceSelectorComponent (deviceManager, 0, 0, 1, 128, false, false, false, false);
    selector->setSize (560, 480);

    juce::DialogWindow::LaunchOptions options;
    options.content.setOwned (selector);
    options.dialogTitle = "Audio settings";
    options.dialogBackgroundColour = colours::panel;
    options.escapeKeyTriggersCloseButton = true;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.launchAsync();
}

void MainComponent::enableAllOutputsIfNewDevice()
{
    auto* device = deviceManager.getCurrentAudioDevice();
    if (device == nullptr)
        return;

    const auto name = device->getName();
    if (name == lastDeviceName)
        return;

    // First time we see this interface: turn on all of its outputs.
    lastDeviceName = name;
    const int available = device->getOutputChannelNames().size();
    auto setup = deviceManager.getAudioDeviceSetup();

    if (device->getActiveOutputChannels().countNumberOfSetBits() < available)
    {
        setup.useDefaultOutputChannels = false;
        setup.outputChannels.clear();
        setup.outputChannels.setRange (0, available, true);
        deviceManager.setAudioDeviceSetup (setup, true);
    }

    saveScheduled = true;
}

void MainComponent::updateDeviceStatus()
{
    auto* device = deviceManager.getCurrentAudioDevice();

    if (device == nullptr)
    {
        deviceStatus.setText ("No audio output open", juce::dontSendNotification);
        roomPanel.setDeviceOutputCount (0);
        return;
    }

    const int active = device->getActiveOutputChannels().countNumberOfSetBits();
    deviceStatus.setText (device->getName() + "  |  " + juce::String (active) + " outputs  |  "
                              + juce::String ((int) device->getCurrentSampleRate()) + " Hz",
                          juce::dontSendNotification);
    roomPanel.setDeviceOutputCount (active);
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    enableAllOutputsIfNewDevice();
    updateDeviceStatus();
    saveScheduled = true;
}

void MainComponent::timerCallback()
{
    // Keep the buttons in step with the engine
    if (speakerTestButton.getToggleState() != engine.isSpeakerTesting())
        speakerTestButton.setToggleState (engine.isSpeakerTesting(), juce::dontSendNotification);

    if (saveScheduled)
    {
        saveScheduled = false;
        saveState();
    }
}

void MainComponent::saveState()
{
    if (properties == nullptr)
        return;

    properties->setValue ("room", room.toJson());
    properties->setValue ("masterDb", masterSlider.getValue());
    properties->setValue ("lastDevice", lastDeviceName);
    properties->setValue ("tab", tabs.getCurrentTabIndex());

    if (auto xml = deviceManager.createStateXml())
        properties->setValue ("audioDevice", xml.get());

    properties->saveIfNeeded();
}

//==============================================================================
void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (17.0f, juce::Font::bold));
    g.drawText ("Spatial Renderer", 16, 12, 300, 24, juce::Justification::centredLeft);
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto side = area.removeFromLeft (380);
    view.setBounds (area);

    side.removeFromTop (42);
    auto header = side.removeFromTop (150).reduced (12, 0);

    auto row = header.removeFromTop (40);
    playButton.setBounds (row.removeFromLeft (row.getWidth() / 2 - 3));
    row.removeFromLeft (6);
    speakerTestButton.setBounds (row);

    header.removeFromTop (8);
    row = header.removeFromTop (26);
    masterLabel.setBounds (row.removeFromLeft (100));
    masterSlider.setBounds (row);

    header.removeFromTop (8);
    audioSettingsButton.setBounds (header.removeFromTop (28));
    header.removeFromTop (4);
    deviceStatus.setBounds (header.removeFromTop (20));

    tabs.setBounds (side);
}

} // namespace spatial
