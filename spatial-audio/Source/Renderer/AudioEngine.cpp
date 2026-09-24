#include "AudioEngine.h"

namespace spatial
{

AudioEngine::AudioEngine()
{
    for (auto& l : speakerLevels)
        l.store (0.0f);
}

void AudioEngine::setSettings (const Settings& newSettings)
{
    const juce::SpinLock::ScopedLockType sl (pendingLock);
    pending.settings = newSettings;
    pendingChanged = true;
}

void AudioEngine::setRoom (const RoomLayout& room)
{
    const juce::SpinLock::ScopedLockType sl (pendingLock);
    pending.numSpeakers = (int) juce::jmin ((size_t) maxSpeakers, room.speakers.size());

    for (int i = 0; i < pending.numSpeakers; ++i)
    {
        const auto& s = room.speakers[(size_t) i];
        auto& rt = pending.speakers[(size_t) i];
        rt.position = s.position;
        rt.channel = s.output - 1;
        rt.type = s.type;
        rt.trimGain = juce::Decibels::decibelsToGain (s.trimDb);
    }

    pendingChanged = true;
}

float AudioEngine::getSpeakerLevel (int index) const
{
    if (index < 0 || index >= maxSpeakers)
        return 0.0f;
    return speakerLevels[(size_t) index].load();
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device->getCurrentSampleRate();
    const int blockSize = juce::jmax (64, device->getCurrentBufferSizeSamples());
    monoBuffer.assign ((size_t) blockSize, 0.0f);
    subBuffer.assign ((size_t) blockSize, 0.0f);
    transducerBuffer.assign ((size_t) blockSize, 0.0f);

    masterSmoothed.reset (sampleRate, 0.05);
    masterSmoothed.setCurrentAndTargetValue (0.0f);
    lastSubHz = lastTransducerHz = 0.0f;
    subFilter.reset();
    transducerFilter.reset();
    currentGains.fill (0.0f);
    deviceOutputs.store (device->getActiveOutputChannels().countNumberOfSetBits());
}

void AudioEngine::audioDeviceStopped()
{
    deviceOutputs.store (0);
}

void AudioEngine::updateFilters()
{
    const auto& s = active.settings;
    const float nyquistLimit = (float) sampleRate * 0.45f;

    if (std::abs (s.subCrossoverHz - lastSubHz) > 0.01f)
    {
        subFilter.setCutoff (sampleRate, juce::jlimit (20.0f, nyquistLimit, s.subCrossoverHz));
        lastSubHz = s.subCrossoverHz;
    }

    if (std::abs (s.transducerCutoffHz - lastTransducerHz) > 0.01f)
    {
        transducerFilter.setCutoff (sampleRate, juce::jlimit (20.0f, nyquistLimit, s.transducerCutoffHz));
        lastTransducerHz = s.transducerCutoffHz;
    }
}

float AudioEngine::nextPink()
{
    // Paul Kellet's pink noise filter
    const float white = random.nextFloat() * 2.0f - 1.0f;
    pink[0] = 0.99886f * pink[0] + white * 0.0555179f;
    pink[1] = 0.99332f * pink[1] + white * 0.0750759f;
    pink[2] = 0.96900f * pink[2] + white * 0.1538520f;
    pink[3] = 0.86650f * pink[3] + white * 0.3104856f;
    pink[4] = 0.55000f * pink[4] + white * 0.5329522f;
    pink[5] = -0.7616f * pink[5] - white * 0.0168980f;
    const float out = pink[0] + pink[1] + pink[2] + pink[3] + pink[4] + pink[5] + pink[6] + white * 0.5362f;
    pink[6] = white * 0.115926f;
    return out * 0.11f;
}

void AudioEngine::generateTestSound (float* dest, int numSamples)
{
    const auto sound = (TestSound) testSound.load();

    for (int i = 0; i < numSamples; ++i)
    {
        float v = 0.0f;

        switch (sound)
        {
            case TestSound::pinkNoise:
                v = nextPink();
                break;

            case TestSound::pinkBursts:
            {
                // 300 ms on, 200 ms off, with short fades
                const auto period = (juce::int64) (sampleRate * 0.5);
                const auto onLength = (juce::int64) (sampleRate * 0.3);
                const auto fade = (juce::int64) (sampleRate * 0.005);
                const auto pos = burstSample % period;
                float env = 0.0f;
                if (pos < fade)                 env = (float) pos / (float) fade;
                else if (pos > onLength - fade && pos < onLength) env = (float) (onLength - pos) / (float) fade;
                else if (pos < onLength)        env = 1.0f;
                v = nextPink() * env;
                ++burstSample;
                break;
            }

            case TestSound::tone440:
            case TestSound::bassTone60:
            {
                const double freq = sound == TestSound::tone440 ? 440.0 : 60.0;
                v = (float) std::sin (tonePhase) * 0.5f;
                tonePhase += juce::MathConstants<double>::twoPi * freq / sampleRate;
                if (tonePhase > juce::MathConstants<double>::twoPi)
                    tonePhase -= juce::MathConstants<double>::twoPi;
                break;
            }
        }

        dest[i] = v;
    }
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputChannelData, int numOutputChannels,
                                                    int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    // Pick up changes from the user interface, if it isn't busy writing them.
    {
        const juce::SpinLock::ScopedTryLockType tl (pendingLock);
        if (tl.isLocked() && pendingChanged)
        {
            active = pending;
            pendingChanged = false;
        }
    }

    updateFilters();

    if (restartRequested.exchange (false))
        passes = 0.0;

    const int chunkSize = (int) monoBuffer.size();
    if (chunkSize == 0)
        return;

    for (int start = 0; start < numSamples; start += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - start);

        if (speakerTestRequested.load())
            renderSpeakerTest (outputChannelData, numOutputChannels, start, n);
        else
            renderChunk (outputChannelData, numOutputChannels, start, n);
    }
}

void AudioEngine::renderSpeakerTest (float* const* outputs, int numOutputs, int start, int numSamples)
{
    if (! wasSpeakerTesting)
    {
        speakerTestSample = 0;
        burstSample = 0;
        wasSpeakerTesting = true;
    }

    const int count = active.numSpeakers;
    if (count == 0)
    {
        speakerTestIndex.store (-1);
        return;
    }

    const auto samplesPerSpeaker = (juce::int64) sampleRate;
    const int index = (int) ((speakerTestSample / samplesPerSpeaker) % count);
    speakerTestIndex.store (index);

    for (int i = 0; i < count; ++i)
        speakerLevels[(size_t) i].store (i == index ? 1.0f : 0.0f);

    const float master = juce::Decibels::decibelsToGain (masterDb.load(), -80.0f);
    const auto& spk = active.speakers[(size_t) index];

    for (int i = 0; i < numSamples; ++i)
    {
        const auto pos = (speakerTestSample + i) % samplesPerSpeaker;
        const auto onLength = (juce::int64) (sampleRate * 0.7);
        const auto fade = (juce::int64) (sampleRate * 0.01);
        float env = 0.0f;
        if (pos < fade)               env = (float) pos / (float) fade;
        else if (pos < onLength - fade) env = 1.0f;
        else if (pos < onLength)      env = (float) (onLength - pos) / (float) fade;
        monoBuffer[(size_t) i] = nextPink() * env * master * spk.trimGain;
    }

    speakerTestSample += numSamples;

    if (spk.channel >= 0 && spk.channel < numOutputs && outputs[spk.channel] != nullptr)
        juce::FloatVectorOperations::add (outputs[spk.channel] + start, monoBuffer.data(), numSamples);
}

void AudioEngine::renderChunk (float* const* outputs, int numOutputs, int start, int numSamples)
{
    if (wasSpeakerTesting)
    {
        wasSpeakerTesting = false;
        speakerTestIndex.store (-1);
        masterSmoothed.setCurrentAndTargetValue (0.0f);
        currentGains.fill (0.0f);
    }

    const auto& s = active.settings;
    const int count = active.numSpeakers;

    // Move along the path
    const float secondsPerPass = s.speed.effectiveSecondsPerPass();
    if (s.path.shape != PathShape::none && playing.load())
        passes += (double) numSamples / (sampleRate * (double) secondsPerPass);

    const float t = passesToPathPosition (passes, s.path.direction, s.path.startOffset);
    const Vec3 source = s.path.shape == PathShape::none ? s.path.centre : pathPoint (s.path, t);
    srcX.store (source.x);
    srcY.store (source.y);
    srcZ.store (source.z);

    // Work out gains, separately for each type of speaker
    for (int i = 0; i < count; ++i)
        positions[(size_t) i] = active.speakers[(size_t) i].position;

    targetGains.fill (0.0f);
    std::array<float, maxSpeakers> groupGains {};

    auto panGroup = [&] (SpeakerType type, DbapSettings dbap, float levelDb)
    {
        bool any = false;
        for (int i = 0; i < count; ++i)
        {
            includeMask[(size_t) i] = active.speakers[(size_t) i].type == type;
            any = any || includeMask[(size_t) i];
        }

        if (! any)
            return;

        computeDbapGains (source, positions.data(), includeMask.data(), (size_t) count, dbap, groupGains.data());
        const float level = juce::Decibels::decibelsToGain (levelDb, -80.0f);

        for (int i = 0; i < count; ++i)
            if (includeMask[(size_t) i])
                targetGains[(size_t) i] = groupGains[(size_t) i] * level * active.speakers[(size_t) i].trimGain;
    };

    panGroup (SpeakerType::regular, s.dbap, s.regularLevelDb);

    // Subs: bass is hard to place, so blur it widely between subs
    DbapSettings subDbap = s.dbap;
    subDbap.blur = juce::jmax (s.dbap.blur, 2.0f);
    panGroup (SpeakerType::sub, subDbap, s.subLevelDb);

    // Transducers: follow the sound, a little softer than the speakers
    DbapSettings txDbap = s.dbap;
    txDbap.blur = juce::jmax (s.dbap.blur, 0.5f);
    panGroup (SpeakerType::transducer, txDbap, s.transducerLevelDb);

    // Generate the sound, fading in and out when play starts or stops
    const float masterTarget = playing.load() ? juce::Decibels::decibelsToGain (masterDb.load(), -80.0f) : 0.0f;
    masterSmoothed.setTargetValue (masterTarget);

    const bool silent = masterTarget <= 0.0f && ! masterSmoothed.isSmoothing();

    for (int i = 0; i < count; ++i)
        speakerLevels[(size_t) i].store (playing.load() ? targetGains[(size_t) i] : 0.0f);

    if (silent)
    {
        currentGains = targetGains;
        return;
    }

    generateTestSound (monoBuffer.data(), numSamples);

    for (int i = 0; i < numSamples; ++i)
        monoBuffer[(size_t) i] *= masterSmoothed.getNextValue();

    for (int i = 0; i < numSamples; ++i)
    {
        subBuffer[(size_t) i] = subFilter.process (monoBuffer[(size_t) i]);
        transducerBuffer[(size_t) i] = transducerFilter.process (monoBuffer[(size_t) i]);
    }

    // Send to each speaker, ramping smoothly from the old gain to the new one
    for (int spkIndex = 0; spkIndex < count; ++spkIndex)
    {
        const auto& spk = active.speakers[(size_t) spkIndex];
        const float from = currentGains[(size_t) spkIndex];
        const float to = targetGains[(size_t) spkIndex];

        if (spk.channel < 0 || spk.channel >= numOutputs || outputs[spk.channel] == nullptr)
            continue;

        if (from <= 0.0f && to <= 0.0f)
            continue;

        const float* signal = spk.type == SpeakerType::sub        ? subBuffer.data()
                            : spk.type == SpeakerType::transducer ? transducerBuffer.data()
                                                                  : monoBuffer.data();

        float* out = outputs[spk.channel] + start;
        const float step = (to - from) / (float) numSamples;
        float g = from;

        for (int i = 0; i < numSamples; ++i)
        {
            g += step;
            out[i] += signal[i] * g;
        }
    }

    currentGains = targetGains;
}

} // namespace spatial
