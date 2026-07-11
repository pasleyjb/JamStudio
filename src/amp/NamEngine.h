#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <memory>

// Forward-declare so UI/headers don't need Eigen/json includes.
namespace nam
{
class DSP;
}

namespace jamstudio::amp
{

/** Thread-safe wrapper around NeuralAmpModelerCore DSP.
 *  Load models off the audio thread; process() is real-time safe (spin-try).
 */
class NamEngine
{
public:
    NamEngine();
    ~NamEngine();

    NamEngine (const NamEngine&) = delete;
    NamEngine& operator= (const NamEngine&) = delete;

    /** Load a .nam model. Call from a non-audio thread. Returns false on failure. */
    bool loadModel (const juce::File& namFile, juce::String& errorMessage);

    /** Unload the current model (non-audio thread preferred). */
    void clear();

    /** Prepare DSP for the current device configuration. Safe from audio thread. */
    void prepare (double sampleRate, int maximumBlockSize);

    /** Process mono audio. If no model or busy swapping, copies input to output. */
    void process (const float* input, float* output, int numSamples) noexcept;

    [[nodiscard]] bool isLoaded() const noexcept;
    [[nodiscard]] juce::File getModelFile() const;
    [[nodiscard]] juce::String getModelDisplayName() const;
    [[nodiscard]] double getExpectedSampleRate() const noexcept;

    /** Slim amount 0..1 for slimmable models (no-op otherwise). */
    void setSlim (float slim01) noexcept;
    [[nodiscard]] float getSlim() const noexcept { return slim.load (std::memory_order_relaxed); }

private:
    void applyPreparedState (nam::DSP& dsp) const;
    void disposeRetiredModels();

    mutable juce::SpinLock processLock;
    std::unique_ptr<nam::DSP> dsp;
    std::vector<std::unique_ptr<nam::DSP>> retired; // free off audio thread
    mutable juce::CriticalSection retireLock;

    juce::File modelFile;
    double preparedSampleRate = 0.0;
    int preparedMaxBlock = 0;
    std::atomic<float> slim { 0.0f };
    std::atomic<bool> loaded { false };
};

} // namespace jamstudio::amp
