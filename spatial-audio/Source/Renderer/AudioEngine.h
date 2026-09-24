// Spatial Audio - the renderer's audio engine
//
// Takes a sound (for now, a built-in test sound), moves it along a pathway,
// and works out how loud it should be in every speaker. Regular speakers get
// the full sound, subs get the low end, and transducers get a low band that
// follows the sound around among the transducers.
#pragma once

#include "JuceIncludes.h"
#include "RoomLayout.h"
#include "../Core/Dbap.h"
#include "../Core/Filters.h"
#include "../Core/Pathway.h"

#include <array>
#include <atomic>

namespace spatial
{

class AudioEngine : public juce::AudioIODeviceCallback
{
public:
    static constexpr int maxSpeakers = 128;

    enum class TestSound
    {
        pinkNoise = 0,
        pinkBursts,
        tone440,
        bassTone60
    };

    struct Settings
    {
        PathParams path;
        SpeedParams speed;
        DbapSettings dbap;
        float subCrossoverHz = 100.0f;
        float transducerCutoffHz = 200.0f;
        float regularLevelDb = 0.0f;
        float subLevelDb = 0.0f;
        float transducerLevelDb = 0.0f;
    };

    AudioEngine();

    // Called from the user interface
    void setSettings (const Settings& newSettings);
    void setRoom (const RoomLayout& room);
    void setPlaying (bool shouldPlay)          { playing.store (shouldPlay); }
    bool isPlaying() const                     { return playing.load(); }
    void setMasterLevelDb (float db)           { masterDb.store (db); }
    void setTestSound (TestSound s)            { testSound.store ((int) s); }
    void restartPath()                         { restartRequested.store (true); }

    // Speaker test: plays a burst through each speaker in turn, one per second.
    void setSpeakerTest (bool shouldRun)       { speakerTestRequested.store (shouldRun); }
    bool isSpeakerTesting() const              { return speakerTestRequested.load(); }
    int getSpeakerTestIndex() const            { return speakerTestIndex.load(); }

    // Read by the 3D view
    Vec3 getSourcePosition() const             { return { srcX.load(), srcY.load(), srcZ.load() }; }
    float getSpeakerLevel (int index) const;
    int getNumDeviceOutputs() const            { return deviceOutputs.load(); }

    // juce::AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData, int numInputChannels,
                                           float* const* outputChannelData, int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct RtSpeaker
    {
        Vec3 position;
        int channel = 0;            // 0-based output channel
        SpeakerType type = SpeakerType::regular;
        float trimGain = 1.0f;
    };

    struct RtState
    {
        Settings settings;
        std::array<RtSpeaker, maxSpeakers> speakers {};
        int numSpeakers = 0;
    };

    void renderChunk (float* const* outputs, int numOutputs, int start, int numSamples);
    void renderSpeakerTest (float* const* outputs, int numOutputs, int start, int numSamples);
    float nextPink();
    void generateTestSound (float* dest, int numSamples);
    void updateFilters();

    // Hand-over from the user interface to the audio thread
    juce::SpinLock pendingLock;
    RtState pending;
    bool pendingChanged = true;

    // Audio thread only
    RtState active;
    double sampleRate = 48000.0;
    std::vector<float> monoBuffer, subBuffer, transducerBuffer;
    std::array<float, maxSpeakers> currentGains {}, targetGains {};
    std::array<Vec3, maxSpeakers> positions {};
    std::array<bool, maxSpeakers> includeMask {};
    LowPassLR4 subFilter, transducerFilter;
    float lastSubHz = 0.0f, lastTransducerHz = 0.0f;
    juce::SmoothedValue<float> masterSmoothed;
    double passes = 0.0;
    double tonePhase = 0.0;
    juce::int64 burstSample = 0, speakerTestSample = 0;
    bool wasSpeakerTesting = false;
    float pink[7] {};
    juce::Random random;

    // Shared between threads
    std::atomic<bool> playing { false }, speakerTestRequested { false }, restartRequested { false };
    std::atomic<float> masterDb { -24.0f };
    std::atomic<int> testSound { (int) TestSound::pinkBursts };
    std::atomic<int> speakerTestIndex { -1 };
    std::atomic<int> deviceOutputs { 0 };
    std::atomic<float> srcX { 0.0f }, srcY { 0.0f }, srcZ { 1.2f };
    std::array<std::atomic<float>, maxSpeakers> speakerLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace spatial
