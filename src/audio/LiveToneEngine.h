#pragma once

#include "../performance/ToneProfile.h"

#include <JuceHeader.h>
#include <array>
#include <atomic>

namespace jamstudio::audio
{

/**
 * Three live instrument paths (G1 / G2 / Bass) processed from hardware inputs.
 * Lightweight amp-style DSP until a full NAM runtime is linked; UI still presents
 * NAM A2-style controls and .nam model path fields for future models.
 *
 * Runs as an AudioIODeviceCallback so it can read inputs while stems play via
 * TransportController's AudioSourcePlayer.
 */
class LiveToneEngine : public juce::AudioIODeviceCallback
{
public:
    LiveToneEngine();
    ~LiveToneEngine() override = default;

    void setEnabled (bool shouldEnable) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    /** Hardware input channel index for each path (default 0, 1, 2). */
    void setInputChannel (jamstudio::performance::LiveInstrumentRole role, int channelIndex) noexcept;
    [[nodiscard]] int getInputChannel (jamstudio::performance::LiveInstrumentRole role) const noexcept;

    void setPathEnabled (jamstudio::performance::LiveInstrumentRole role, bool shouldEnable) noexcept;
    [[nodiscard]] bool isPathEnabled (jamstudio::performance::LiveInstrumentRole role) const noexcept;

    /** Apply a full profile to a path (thread-safe for RT params). */
    void applyProfile (jamstudio::performance::LiveInstrumentRole role,
                       const jamstudio::performance::ToneProfile& profile);

    /** Snapshot of params currently on a path (for UI). */
    [[nodiscard]] jamstudio::performance::ToneProfile getProfileSnapshot (
        jamstudio::performance::LiveInstrumentRole role) const;

    [[nodiscard]] float getInputMeter (jamstudio::performance::LiveInstrumentRole role) const noexcept;
    [[nodiscard]] float getOutputMeter (jamstudio::performance::LiveInstrumentRole role) const noexcept;

    // AudioIODeviceCallback
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct PathState
    {
        std::atomic<int> inputChannel { 0 };
        std::atomic<bool> enabled { true };
        std::atomic<bool> bypass { false };
        std::atomic<float> inputGain { 0.55f };
        std::atomic<float> drive { 0.45f };
        std::atomic<float> bass { 0.5f };
        std::atomic<float> mid { 0.5f };
        std::atomic<float> treble { 0.5f };
        std::atomic<float> presence { 0.45f };
        std::atomic<float> outputLevel { 0.7f };
        std::atomic<float> inMeter { 0.0f };
        std::atomic<float> outMeter { 0.0f };

        // Non-atomic UI-only labels (written on message thread)
        juce::String profileId;
        juce::String profileName;
        juce::String namModelPath;
        juce::String cabIrPath;
        jamstudio::performance::LiveInstrumentRole role =
            jamstudio::performance::LiveInstrumentRole::guitar1;

        // Simple one-pole filters (audio thread)
        float lpState = 0.0f;
        float hpState = 0.0f;
        float midState = 0.0f;
    };

    void processPath (PathState& path,
                      const float* input,
                      float* outL,
                      float* outR,
                      int numSamples,
                      double sampleRate) noexcept;

    std::array<PathState, jamstudio::performance::kNumLiveTonePaths> paths;
    std::atomic<bool> engineEnabled { false };
    std::atomic<double> currentSampleRate { 48000.0 };
    mutable juce::CriticalSection labelLock;
};

} // namespace jamstudio::audio
