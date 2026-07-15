#include "NamModelEngine.h"

#include <NAM/dsp.h>
#include <NAM/get_dsp.h>

#include <filesystem>

namespace jamstudio::audio
{

namespace
{
constexpr int kMaxChunk = 8192;
}

NamModelEngine::NamModelEngine() = default;

NamModelEngine::~NamModelEngine()
{
    clear();
    disposeRetiredModels();
}

bool NamModelEngine::loadModel (const juce::File& namFile, juce::String& errorMessage)
{
    disposeRetiredModels();

    if (! namFile.existsAsFile())
    {
        errorMessage = "Model file not found: " + namFile.getFullPathName();
        return false;
    }

    if (! namFile.hasFileExtension (".nam"))
    {
        errorMessage = "Expected a .nam file.";
        return false;
    }

    std::unique_ptr<nam::DSP> newDsp;

    try
    {
        nam::DspLoadOptions options;
        options.prewarm = false;
        newDsp = nam::get_dsp (std::filesystem::path (namFile.getFullPathName().toStdString()), options);
    }
    catch (const std::exception& e)
    {
        errorMessage = juce::String ("NAM load failed: ") + e.what();
        return false;
    }
    catch (...)
    {
        errorMessage = "NAM load failed (unknown error).";
        return false;
    }

    if (newDsp == nullptr)
    {
        errorMessage = "NAM returned no model for " + namFile.getFileName();
        return false;
    }

    if (preparedSampleRate > 0.0 && preparedMaxBlock > 0)
        applyPreparedState (*newDsp);

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

void NamModelEngine::clear()
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

void NamModelEngine::prepare (const double sampleRate, const int maximumBlockSize)
{
    preparedSampleRate = sampleRate;
    preparedMaxBlock = juce::jmax (1, maximumBlockSize);

    const juce::SpinLock::ScopedLockType lock (processLock);
    if (dsp != nullptr)
        applyPreparedState (*dsp);
}

void NamModelEngine::applyPreparedState (nam::DSP& model) const
{
    if (preparedSampleRate <= 0.0 || preparedMaxBlock <= 0)
        return;
    model.Reset (preparedSampleRate, preparedMaxBlock);
}

void NamModelEngine::process (const float* input, float* output, const int numSamples) noexcept
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

    int offset = 0;
    while (offset < numSamples)
    {
        const int chunk = juce::jmin (numSamples - offset, kMaxChunk);
        float* inChunk = const_cast<float*> (input) + offset;
        float* outChunk = output + offset;
        NAM_SAMPLE* inputs[] = { reinterpret_cast<NAM_SAMPLE*> (inChunk) };
        NAM_SAMPLE* outputs[] = { reinterpret_cast<NAM_SAMPLE*> (outChunk) };
        dsp->process (inputs, outputs, chunk);
        offset += chunk;
    }
}

bool NamModelEngine::isLoaded() const noexcept
{
    return loaded.load (std::memory_order_acquire);
}

juce::File NamModelEngine::getModelFile() const
{
    const juce::SpinLock::ScopedLockType lock (processLock);
    return modelFile;
}

juce::String NamModelEngine::getModelDisplayName() const
{
    const juce::SpinLock::ScopedLockType lock (processLock);
    return modelFile != juce::File() ? modelFile.getFileNameWithoutExtension() : juce::String();
}

void NamModelEngine::disposeRetiredModels()
{
    std::vector<std::unique_ptr<nam::DSP>> doomed;
    {
        const juce::ScopedLock lock (retireLock);
        doomed.swap (retired);
    }
    doomed.clear();
}

} // namespace jamstudio::audio
