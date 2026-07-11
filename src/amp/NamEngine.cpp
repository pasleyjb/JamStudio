#include "NamEngine.h"

#include <NAM/dsp.h>
#include <NAM/get_dsp.h>
#include <NAM/slimmable.h>
#include <NAM/version.h>

#include <cmath>
#include <filesystem>
#include <vector>

namespace jamstudio::amp
{

namespace
{
constexpr int kMaxScratchFrames = 8192;

void applySlimIfSupported (nam::DSP* model, float slim01)
{
    if (model == nullptr)
        return;

    if (auto* slimmable = dynamic_cast<nam::SlimmableModel*> (model))
        slimmable->SetSlimmableSize (static_cast<double> (juce::jlimit (0.0f, 1.0f, slim01)));
}
} // namespace

NamEngine::NamEngine() = default;

NamEngine::~NamEngine()
{
    clear();
    disposeRetiredModels();
}

bool NamEngine::loadModel (const juce::File& namFile, juce::String& errorMessage)
{
    disposeRetiredModels();

    if (! namFile.existsAsFile())
    {
        errorMessage = "Model file does not exist: " + namFile.getFullPathName();
        return false;
    }

    if (! namFile.hasFileExtension (".nam"))
    {
        errorMessage = "Expected a .nam model file.";
        return false;
    }

    std::unique_ptr<nam::DSP> newDsp;

    try
    {
        nam::DspLoadOptions options;
        // Avoid long prewarm inside get_dsp; we prewarm via Reset after prepare.
        options.prewarm = false;
        newDsp = nam::get_dsp (std::filesystem::path (namFile.getFullPathName().toStdString()), options);
    }
    catch (const std::exception& e)
    {
        errorMessage = "Failed to load NAM model: " + juce::String (e.what());
        return false;
    }
    catch (...)
    {
        errorMessage = "Failed to load NAM model (unknown error).";
        return false;
    }

    if (newDsp == nullptr)
    {
        errorMessage = "NAM returned no DSP for: " + namFile.getFileName();
        return false;
    }

    if (preparedSampleRate > 0.0 && preparedMaxBlock > 0)
        applyPreparedState (*newDsp);

    applySlimIfSupported (newDsp.get(), slim.load (std::memory_order_relaxed));

    {
        const juce::SpinLock::ScopedLockType lock (processLock);
        if (dsp != nullptr)
        {
            const juce::ScopedLock retire (retireLock);
            retired.push_back (std::move (dsp));
        }
        dsp = std::move (newDsp);
        modelFile = namFile;
        loaded.store (true, std::memory_order_release);
    }

    return true;
}

void NamEngine::clear()
{
    const juce::SpinLock::ScopedLockType lock (processLock);

    if (dsp != nullptr)
    {
        const juce::ScopedLock retire (retireLock);
        retired.push_back (std::move (dsp));
    }

    modelFile = juce::File();
    loaded.store (false, std::memory_order_release);
}

void NamEngine::prepare (const double sampleRate, const int maximumBlockSize)
{
    preparedSampleRate = sampleRate;
    preparedMaxBlock = juce::jmax (1, maximumBlockSize);

    const juce::SpinLock::ScopedLockType lock (processLock);

    if (dsp != nullptr)
        applyPreparedState (*dsp);
}

void NamEngine::applyPreparedState (nam::DSP& model) const
{
    if (preparedSampleRate <= 0.0 || preparedMaxBlock <= 0)
        return;

    // Reset() prewarms by default — OK off RT / during device start.
    model.Reset (preparedSampleRate, preparedMaxBlock);
    applySlimIfSupported (&model, slim.load (std::memory_order_relaxed));
}

void NamEngine::process (const float* input, float* output, const int numSamples) noexcept
{
    if (input == nullptr || output == nullptr || numSamples <= 0)
        return;

    const juce::SpinLock::ScopedTryLockType tryLock (processLock);

    if (! tryLock.isLocked() || dsp == nullptr)
    {
        if (input != output)
            juce::FloatVectorOperations::copy (output, input, numSamples);
        return;
    }

    // NAM process uses NAM_SAMPLE** channel pointers. With NAM_SAMPLE_FLOAT
    // this is float; process in-place via channel pointer arrays.
    float* inPtr = const_cast<float*> (input);
    float* outPtr = output;

    // If blocks exceed safety, process in chunks.
    int offset = 0;

    while (offset < numSamples)
    {
        const int chunk = juce::jmin (numSamples - offset, kMaxScratchFrames);
        float* inChunk = inPtr + offset;
        float* outChunk = outPtr + offset;
        NAM_SAMPLE* inputs[] = { reinterpret_cast<NAM_SAMPLE*> (inChunk) };
        NAM_SAMPLE* outputs[] = { reinterpret_cast<NAM_SAMPLE*> (outChunk) };
        dsp->process (inputs, outputs, chunk);
        offset += chunk;
    }
}

bool NamEngine::isLoaded() const noexcept
{
    return loaded.load (std::memory_order_acquire);
}

juce::File NamEngine::getModelFile() const
{
    const juce::SpinLock::ScopedLockType lock (processLock);
    return modelFile;
}

juce::String NamEngine::getModelDisplayName() const
{
    const juce::SpinLock::ScopedLockType lock (processLock);

    if (modelFile == juce::File())
        return {};

    return modelFile.getFileNameWithoutExtension();
}

double NamEngine::getExpectedSampleRate() const noexcept
{
    const juce::SpinLock::ScopedTryLockType tryLock (processLock);

    if (! tryLock.isLocked() || dsp == nullptr)
        return -1.0;

    return dsp->GetExpectedSampleRate();
}

void NamEngine::setSlim (const float slim01) noexcept
{
    const float clamped = juce::jlimit (0.0f, 1.0f, slim01);
    slim.store (clamped, std::memory_order_relaxed);

    const juce::SpinLock::ScopedTryLockType tryLock (processLock);

    if (tryLock.isLocked() && dsp != nullptr)
        applySlimIfSupported (dsp.get(), clamped);
}

void NamEngine::disposeRetiredModels()
{
    std::vector<std::unique_ptr<nam::DSP>> toDelete;

    {
        const juce::ScopedLock lock (retireLock);
        toDelete.swap (retired);
    }

    toDelete.clear();
}

} // namespace jamstudio::amp
