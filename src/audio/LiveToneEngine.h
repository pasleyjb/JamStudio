#pragma once

#include "NamModelEngine.h"
#include "../performance/ToneProfile.h"

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include <functional>

namespace jamstudio::audio
{

/**
 * Three live instrument paths (G1 / G2 / Bass) from hardware inputs.
 * Uses real NAM (.nam) when a model is loaded; otherwise lightweight amp sim.
 */
class LiveToneEngine : public juce::AudioIODeviceCallback
{
public:
    using LoadCompleteCallback = std::function<void (jamstudio::performance::LiveInstrumentRole role,
                                                     bool ok,
                                                     juce::String message)>;

    LiveToneEngine();
    ~LiveToneEngine() override;

    void setEnabled (bool shouldEnable) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept;

    void setInputChannel (jamstudio::performance::LiveInstrumentRole role, int channelIndex) noexcept;
    [[nodiscard]] int getInputChannel (jamstudio::performance::LiveInstrumentRole role) const noexcept;

    void setPathEnabled (jamstudio::performance::LiveInstrumentRole role, bool shouldEnable) noexcept;
    [[nodiscard]] bool isPathEnabled (jamstudio::performance::LiveInstrumentRole role) const noexcept;

    void applyProfile (jamstudio::performance::LiveInstrumentRole role,
                       const jamstudio::performance::ToneProfile& profile);

    /** Async NAM load for one path (message-thread callback). */
    void loadNamModelAsync (jamstudio::performance::LiveInstrumentRole role,
                            const juce::File& namFile,
                            LoadCompleteCallback onComplete = nullptr);

    void clearNamModel (jamstudio::performance::LiveInstrumentRole role);

    [[nodiscard]] bool isNamLoaded (jamstudio::performance::LiveInstrumentRole role) const noexcept;
    [[nodiscard]] juce::String getNamModelName (jamstudio::performance::LiveInstrumentRole role) const;

    [[nodiscard]] jamstudio::performance::ToneProfile getProfileSnapshot (
        jamstudio::performance::LiveInstrumentRole role) const;

    [[nodiscard]] float getInputMeter (jamstudio::performance::LiveInstrumentRole role) const noexcept;
    [[nodiscard]] float getOutputMeter (jamstudio::performance::LiveInstrumentRole role) const noexcept;

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
        std::atomic<bool> namReady { false };

        juce::String profileId;
        juce::String profileName;
        juce::String namModelPath;
        juce::String cabIrPath;
        jamstudio::performance::LiveInstrumentRole role =
            jamstudio::performance::LiveInstrumentRole::guitar1;

        float lpState = 0.0f;
        float hpState = 0.0f;
        float midState = 0.0f;

        NamModelEngine nam;
        juce::AudioBuffer<float> namScratch;
    };

    void processPath (PathState& path,
                      const float* input,
                      float* outL,
                      float* outR,
                      int numSamples,
                      double sampleRate) noexcept;

    void processBuiltinAmp (PathState& path,
                            float* work,
                            int numSamples,
                            double sampleRate) noexcept;

    std::array<PathState, jamstudio::performance::kNumLiveTonePaths> paths;
    std::atomic<bool> engineEnabled { false };
    std::atomic<double> currentSampleRate { 48000.0 };
    std::atomic<int> maxBlock { 512 };
    mutable juce::CriticalSection labelLock;
    juce::ThreadPool loadPool { 1 };
};

} // namespace jamstudio::audio
