#pragma once

#include "NamEngine.h"

#include <JuceHeader.h>

#include <atomic>
#include <functional>

namespace jamstudio::amp
{

/** Live amp path: device input → gain → NAM → gain → monitor output.
 *  Registered as a secondary AudioDeviceManager callback so stems stay mixed in.
 */
class AmpProcessor : public juce::AudioIODeviceCallback
{
public:
    AmpProcessor();
    ~AmpProcessor() override;

    NamEngine& getEngine() noexcept { return engine; }
    const NamEngine& getEngine() const noexcept { return engine; }

    void setEnabled (bool shouldBeEnabled) noexcept;
    [[nodiscard]] bool isEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setBypass (bool shouldBypass) noexcept;
    [[nodiscard]] bool isBypassed() const noexcept { return bypass.load (std::memory_order_relaxed); }

    void setInputGainDb (float gainDb) noexcept;
    void setOutputGainDb (float gainDb) noexcept;
    [[nodiscard]] float getInputGainDb() const noexcept { return inputGainDb.load (std::memory_order_relaxed); }
    [[nodiscard]] float getOutputGainDb() const noexcept { return outputGainDb.load (std::memory_order_relaxed); }

    /** Async load on a background thread; invokes callback on message thread. */
    void loadModelAsync (const juce::File& namFile,
                         std::function<void (bool success, juce::String error)> onComplete = nullptr);

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

    [[nodiscard]] float getInputPeak() noexcept { return inputPeak.exchange (0.0f); }
    [[nodiscard]] float getOutputPeak() noexcept { return outputPeak.exchange (0.0f); }

private:
    static float dbToGain (float db) noexcept;

    NamEngine engine;
    juce::AudioBuffer<float> processBuffer;

    std::atomic<bool> enabled { false };
    std::atomic<bool> bypass { false };
    std::atomic<float> inputGainDb { 0.0f };
    std::atomic<float> outputGainDb { 0.0f };
    std::atomic<float> inputGainLinear { 1.0f };
    std::atomic<float> outputGainLinear { 1.0f };
    std::atomic<float> inputPeak { 0.0f };
    std::atomic<float> outputPeak { 0.0f };

    double sampleRate = 44100.0;
    int maxBlockSize = 512;

    juce::ThreadPool loadPool { 1 };
};

} // namespace jamstudio::amp
