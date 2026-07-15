#include "LiveToneEngine.h"

namespace jamstudio::audio
{

LiveToneEngine::LiveToneEngine()
{
    for (int i = 0; i < jamstudio::performance::kNumLiveTonePaths; ++i)
    {
        auto& p = paths[static_cast<size_t> (i)];
        p.role = static_cast<jamstudio::performance::LiveInstrumentRole> (i);
        p.inputChannel.store (i, std::memory_order_relaxed);
        const auto def = jamstudio::performance::ToneProfile::makeDefault (p.role);
        applyProfile (p.role, def);
    }
}

void LiveToneEngine::setEnabled (const bool shouldEnable) noexcept
{
    engineEnabled.store (shouldEnable, std::memory_order_relaxed);
}

bool LiveToneEngine::isEnabled() const noexcept
{
    return engineEnabled.load (std::memory_order_relaxed);
}

void LiveToneEngine::setInputChannel (const jamstudio::performance::LiveInstrumentRole role,
                                      const int channelIndex) noexcept
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths))
        paths[static_cast<size_t> (i)].inputChannel.store (juce::jmax (0, channelIndex),
                                                           std::memory_order_relaxed);
}

int LiveToneEngine::getInputChannel (const jamstudio::performance::LiveInstrumentRole role) const noexcept
{
    const auto i = static_cast<int> (role);
    return juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths)
               ? paths[static_cast<size_t> (i)].inputChannel.load (std::memory_order_relaxed)
               : 0;
}

void LiveToneEngine::setPathEnabled (const jamstudio::performance::LiveInstrumentRole role,
                                     const bool shouldEnable) noexcept
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths))
        paths[static_cast<size_t> (i)].enabled.store (shouldEnable, std::memory_order_relaxed);
}

bool LiveToneEngine::isPathEnabled (const jamstudio::performance::LiveInstrumentRole role) const noexcept
{
    const auto i = static_cast<int> (role);
    return juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths)
           && paths[static_cast<size_t> (i)].enabled.load (std::memory_order_relaxed);
}

void LiveToneEngine::applyProfile (const jamstudio::performance::LiveInstrumentRole role,
                                   const jamstudio::performance::ToneProfile& profile)
{
    const auto i = static_cast<int> (role);
    if (! juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths))
        return;

    auto& p = paths[static_cast<size_t> (i)];
    p.inputGain.store (profile.inputGain, std::memory_order_relaxed);
    p.drive.store (profile.drive, std::memory_order_relaxed);
    p.bass.store (profile.bass, std::memory_order_relaxed);
    p.mid.store (profile.mid, std::memory_order_relaxed);
    p.treble.store (profile.treble, std::memory_order_relaxed);
    p.presence.store (profile.presence, std::memory_order_relaxed);
    p.outputLevel.store (profile.outputLevel, std::memory_order_relaxed);
    p.bypass.store (profile.bypass, std::memory_order_relaxed);

    const juce::ScopedLock sl (labelLock);
    p.profileId = profile.id;
    p.profileName = profile.name;
    p.namModelPath = profile.namModelPath;
    p.cabIrPath = profile.cabIrPath;
    p.role = profile.role;
}

jamstudio::performance::ToneProfile LiveToneEngine::getProfileSnapshot (
    const jamstudio::performance::LiveInstrumentRole role) const
{
    jamstudio::performance::ToneProfile t;
    const auto i = static_cast<int> (role);
    if (! juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths))
        return t;

    const auto& p = paths[static_cast<size_t> (i)];
    t.inputGain = p.inputGain.load (std::memory_order_relaxed);
    t.drive = p.drive.load (std::memory_order_relaxed);
    t.bass = p.bass.load (std::memory_order_relaxed);
    t.mid = p.mid.load (std::memory_order_relaxed);
    t.treble = p.treble.load (std::memory_order_relaxed);
    t.presence = p.presence.load (std::memory_order_relaxed);
    t.outputLevel = p.outputLevel.load (std::memory_order_relaxed);
    t.bypass = p.bypass.load (std::memory_order_relaxed);
    t.role = role;

    const juce::ScopedLock sl (labelLock);
    t.id = p.profileId;
    t.name = p.profileName;
    t.namModelPath = p.namModelPath;
    t.cabIrPath = p.cabIrPath;
    return t;
}

float LiveToneEngine::getInputMeter (const jamstudio::performance::LiveInstrumentRole role) const noexcept
{
    const auto i = static_cast<int> (role);
    return juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths)
               ? paths[static_cast<size_t> (i)].inMeter.load (std::memory_order_relaxed)
               : 0.0f;
}

float LiveToneEngine::getOutputMeter (const jamstudio::performance::LiveInstrumentRole role) const noexcept
{
    const auto i = static_cast<int> (role);
    return juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths)
               ? paths[static_cast<size_t> (i)].outMeter.load (std::memory_order_relaxed)
               : 0.0f;
}

void LiveToneEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (device != nullptr)
        currentSampleRate.store (device->getCurrentSampleRate(), std::memory_order_relaxed);

    for (auto& p : paths)
    {
        p.lpState = 0.0f;
        p.hpState = 0.0f;
        p.midState = 0.0f;
    }
}

void LiveToneEngine::audioDeviceStopped()
{
    for (auto& p : paths)
    {
        p.inMeter.store (0.0f, std::memory_order_relaxed);
        p.outMeter.store (0.0f, std::memory_order_relaxed);
    }
}

void LiveToneEngine::processPath (PathState& path,
                                  const float* input,
                                  float* outL,
                                  float* outR,
                                  const int numSamples,
                                  const double sampleRate) noexcept
{
    if (input == nullptr || outL == nullptr)
        return;

    const bool bypass = path.bypass.load (std::memory_order_relaxed);
    const float inG = juce::jmap (path.inputGain.load (std::memory_order_relaxed), 0.0f, 1.0f, 0.0f, 2.5f);
    const float drive = path.drive.load (std::memory_order_relaxed);
    const float bass = path.bass.load (std::memory_order_relaxed);
    const float mid = path.mid.load (std::memory_order_relaxed);
    const float treble = path.treble.load (std::memory_order_relaxed);
    const float presence = path.presence.load (std::memory_order_relaxed);
    const float outG = juce::jmap (path.outputLevel.load (std::memory_order_relaxed), 0.0f, 1.0f, 0.0f, 1.4f);

    // One-pole coefficients (approximate tone stack)
    const float sr = static_cast<float> (sampleRate > 0.0 ? sampleRate : 48000.0);
    const float lpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmap (treble, 0.0f, 1.0f, 1200.0f, 8000.0f) / sr);
    const float hpCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * juce::jmap (bass, 0.0f, 1.0f, 40.0f, 220.0f) / sr);
    const float midCoeff = std::exp (-2.0f * juce::MathConstants<float>::pi * 800.0f / sr);
    const float driveAmt = 1.0f + drive * 8.0f;
    const float presenceBoost = 1.0f + (presence - 0.5f) * 0.8f;
    const float midGain = 0.6f + mid * 0.9f;

    float peakIn = 0.0f;
    float peakOut = 0.0f;

    for (int n = 0; n < numSamples; ++n)
    {
        float x = input[n];
        peakIn = juce::jmax (peakIn, std::abs (x));

        if (bypass)
        {
            const float y = x * outG;
            outL[n] += y;
            if (outR != nullptr)
                outR[n] += y;
            peakOut = juce::jmax (peakOut, std::abs (y));
            continue;
        }

        x *= inG;

        // Soft-clip drive (cheap amp-ish saturation)
        x = std::tanh (x * driveAmt) * (0.85f + (1.0f - drive) * 0.15f);

        // High-pass (bass control)
        path.hpState += hpCoeff * (x - path.hpState);
        float y = x - path.hpState;
        y *= (0.7f + bass * 0.6f);

        // Mid band emphasis
        path.midState += midCoeff * (y - path.midState);
        y = path.midState + (y - path.midState) * midGain;

        // Low-pass (treble)
        path.lpState += (1.0f - lpCoeff) * (y - path.lpState);
        y = path.lpState;

        y *= presenceBoost * outG;
        y = juce::jlimit (-1.2f, 1.2f, y);

        outL[n] += y;
        if (outR != nullptr)
            outR[n] += y;
        peakOut = juce::jmax (peakOut, std::abs (y));
    }

    // Meter ballistics
    const float prevIn = path.inMeter.load (std::memory_order_relaxed);
    const float prevOut = path.outMeter.load (std::memory_order_relaxed);
    path.inMeter.store (peakIn > prevIn ? peakIn : prevIn * 0.92f, std::memory_order_relaxed);
    path.outMeter.store (peakOut > prevOut ? peakOut : prevOut * 0.92f, std::memory_order_relaxed);
}

void LiveToneEngine::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                       const int numInputChannels,
                                                       float* const* outputChannelData,
                                                       const int numOutputChannels,
                                                       const int numSamples,
                                                       const juce::AudioIODeviceCallbackContext&)
{
    if (! engineEnabled.load (std::memory_order_relaxed)
        || inputChannelData == nullptr
        || outputChannelData == nullptr
        || numSamples <= 0
        || numOutputChannels < 1)
        return;

    float* outL = outputChannelData[0];
    float* outR = numOutputChannels > 1 ? outputChannelData[1] : outputChannelData[0];
    if (outL == nullptr)
        return;

    const double sr = currentSampleRate.load (std::memory_order_relaxed);

    for (auto& path : paths)
    {
        if (! path.enabled.load (std::memory_order_relaxed))
        {
            path.inMeter.store (path.inMeter.load (std::memory_order_relaxed) * 0.9f,
                                std::memory_order_relaxed);
            path.outMeter.store (path.outMeter.load (std::memory_order_relaxed) * 0.9f,
                                 std::memory_order_relaxed);
            continue;
        }

        const int ch = path.inputChannel.load (std::memory_order_relaxed);
        if (ch < 0 || ch >= numInputChannels || inputChannelData[ch] == nullptr)
            continue;

        processPath (path, inputChannelData[ch], outL, outR, numSamples, sr);
    }
}

} // namespace jamstudio::audio
