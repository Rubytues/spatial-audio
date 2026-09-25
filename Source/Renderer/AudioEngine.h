// Spatial Audio - the renderer's audio engine
//
// Takes a sound (a built-in test sound or an audio file), moves it along a
// pathway, and works out how much of it each speaker plays. Neighbouring
// speakers always share the sound, and the balance glides continuously as it
// moves, so it travels smoothly rather than jumping from speaker to speaker.
//
// Frequencies are shared differently: low frequencies spread wider across the
// speakers, high frequencies stay more focused. Subs get only the low end,
// and transducers get a low band that follows the sound among the transducers.
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

    // Gains are recalculated at least this often (in samples), however large
    // the audio buffer is, so movement stays smooth.
    static constexpr int maxChunk = 64;

    // Below this frequency, regular speakers use the wider "low spread".
    static constexpr float lowSplitHz = 300.0f;

    enum class TestSound
    {
        pinkNoise = 0,
        pinkPulses,
        tone440,
        bassTone60,
        audioFile
    };

    struct Settings
    {
        Settings() { dbap.blur = 0.5f; }

        PathParams path;
        SpeedParams speed;
        DbapSettings dbap;              // focus (rolloff) and spread (blur) for regular speakers
        float lowSpread = 1.0f;         // extra spread in metres for frequencies below lowSplitHz
        float glideMs = 80.0f;          // smooths the movement of the sound's position
        float subCrossoverHz = 80.0f;
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

    // A mono recording to loop as the test sound. Call from the message thread.
    void setAudioFile (std::unique_ptr<juce::AudioBuffer<float>> monoAudio, double fileSampleRate);
    bool hasAudioFile() const                  { return fileLoaded.load(); }

    // Speaker test: plays a burst through each speaker in turn, one per second.
    void setSpeakerTest (bool shouldRun)       { speakerTestRequested.store (shouldRun); }
    bool isSpeakerTesting() const              { return speakerTestRequested.load(); }
    int getSpeakerTestIndex() const            { return speakerTestIndex.load(); }

    // Read by the 3D view
    Vec3 getSourcePosition() const             { return { srcX.load(), srcY.load(), srcZ.load() }; }

    // How much of the sound each speaker is playing, 0..1 (before level trims).
    // Squared, it's the speaker's share of the sound within its type.
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

    struct AudioFile
    {
        std::unique_ptr<juce::AudioBuffer<float>> audio;
        double sampleRate = 48000.0;
    };

    void renderChunk (float* const* outputs, int numOutputs, int start, int numSamples);
    void renderSpeakerTest (float* const* outputs, int numOutputs, int start, int numSamples);
    void computeGains (const Vec3& source);
    float nextPink();
    void generateTestSound (float* dest, int numSamples);
    void updateFilters();

    // Hand-over from the user interface to the audio thread
    juce::SpinLock pendingLock;
    RtState pending;
    bool pendingChanged = true;

    juce::SpinLock fileLock;
    AudioFile pendingFile;
    bool fileSwapRequested = false;

    // Audio thread only
    RtState active;
    AudioFile activeFile;
    double filePosition = 0.0;
    double sampleRate = 48000.0;
    std::array<float, maxChunk> monoBuffer {}, lowBuffer {}, highBuffer {}, subBuffer {}, transducerBuffer {};
    std::array<float, maxSpeakers> currentGains {}, targetGains {};        // main band (high band for regular speakers)
    std::array<float, maxSpeakers> currentLowGains {}, targetLowGains {};  // low band, regular speakers only
    std::array<float, maxSpeakers> displayLevels {};
    std::array<Vec3, maxSpeakers> positions {};
    std::array<bool, maxSpeakers> includeMask {};
    std::array<float, maxSpeakers> scratchGains {};
    LowPassLR4 splitFilter, subFilter, transducerFilter;
    float lastSubHz = 0.0f, lastTransducerHz = 0.0f;
    juce::SmoothedValue<float> masterSmoothed;
    double passes = 0.0;
    Vec3 glidePosition;
    bool snapPosition = true;
    double tonePhase = 0.0;
    juce::int64 pulseSample = 0, speakerTestSample = 0;
    bool wasSpeakerTesting = false;
    float pink[7] {};
    juce::Random random;

    // Shared between threads
    std::atomic<bool> playing { false }, speakerTestRequested { false }, restartRequested { false };
    std::atomic<bool> fileLoaded { false };
    std::atomic<float> masterDb { -24.0f };
    std::atomic<int> testSound { (int) TestSound::pinkNoise };
    std::atomic<int> speakerTestIndex { -1 };
    std::atomic<int> deviceOutputs { 0 };
    std::atomic<float> srcX { 0.0f }, srcY { 0.0f }, srcZ { 1.2f };
    std::array<std::atomic<float>, maxSpeakers> speakerLevels;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};

} // namespace spatial
