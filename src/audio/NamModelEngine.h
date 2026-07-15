#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>
#include <vector>

namespace nam { class DSP; }

namespace jamstudio::audio
{

/** Real-time-safe wrapper around NeuralAmpModelerCore for one mono path. */
class NamModelEngine
{
public:
    NamModelEngine();
    ~NamModelEngine();

    NamModelEngine (const NamModelEngine&) = delete;
    NamModelEngine& operator= (const NamModelEngine&) = delete;

    /** Load off the audio thread. Returns false on failure. */
    bool loadModel (const juce::File& namFile, juce::String& errorMessage);

    void clear();
    void prepare (double sampleRate, int maximumBlockSize);

    /** RT-safe process; copies input→output if unloaded/busy. */
    void process (const float* input, float* output, int numSamples) noexcept;

    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] juce::File getModelFile() const;
    [[nodiscard]] juce::String getModelDisplayName() const;

    void disposeRetiredModels();

private:
    void applyPreparedState (nam::DSP& dsp) const;

    mutable juce::SpinLock processLock;
    std::unique_ptr<nam::DSP> dsp;
    std::vector<std::unique_ptr<nam::DSP>> retired;
    mutable juce::CriticalSection retireLock;

    juce::File modelFile;
    double preparedSampleRate = 0.0;
    int preparedMaxBlock = 0;
    std::atomic<bool> loaded { false };
};

} // namespace jamstudio::audio
