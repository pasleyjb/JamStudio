#include "AmpProcessor.h"

#include <cmath>

namespace jamstudio::amp
{

float AmpProcessor::dbToGain (const float db) noexcept
{
    return std::pow (10.0f, db * 0.05f);
}

AmpProcessor::AmpProcessor() = default;

AmpProcessor::~AmpProcessor()
{
    loadPool.removeAllJobs (true, 8000);
}

void AmpProcessor::setEnabled (const bool shouldBeEnabled) noexcept
{
    enabled.store (shouldBeEnabled, std::memory_order_relaxed);
}

void AmpProcessor::setBypass (const bool shouldBypass) noexcept
{
    bypass.store (shouldBypass, std::memory_order_relaxed);
}

void AmpProcessor::setInputGainDb (const float gainDb) noexcept
{
    inputGainDb.store (gainDb, std::memory_order_relaxed);
    inputGainLinear.store (dbToGain (gainDb), std::memory_order_relaxed);
}

void AmpProcessor::setOutputGainDb (const float gainDb) noexcept
{
    outputGainDb.store (gainDb, std::memory_order_relaxed);
    outputGainLinear.store (dbToGain (gainDb), std::memory_order_relaxed);
}

void AmpProcessor::loadModelAsync (const juce::File& namFile,
                                   std::function<void (bool, juce::String)> onComplete)
{
    loadPool.addJob ([this, namFile, onComplete]
    {
        juce::String error;
        const bool ok = engine.loadModel (namFile, error);

        if (ok)
            enabled.store (true, std::memory_order_relaxed);

        if (onComplete != nullptr)
        {
            juce::MessageManager::callAsync ([onComplete, ok, error]
            {
                onComplete (ok, error);
            });
        }
    });
}

void AmpProcessor::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    sampleRate = device != nullptr ? device->getCurrentSampleRate() : 44100.0;
    maxBlockSize = device != nullptr ? device->getCurrentBufferSizeSamples() : 512;
    processBuffer.setSize (1, juce::jmax (maxBlockSize, 512));
    engine.prepare (sampleRate, maxBlockSize);
}

void AmpProcessor::audioDeviceStopped()
{
    processBuffer.setSize (0, 0);
}

void AmpProcessor::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                     const int numInputChannels,
                                                     float* const* outputChannelData,
                                                     const int numOutputChannels,
                                                     const int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (context);

    // Clear our contribution; JUCE mixes secondary callbacks into the main output.
    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

    if (! enabled.load (std::memory_order_relaxed)
        || numInputChannels <= 0
        || inputChannelData == nullptr
        || inputChannelData[0] == nullptr
        || numSamples <= 0)
    {
        return;
    }

    if (processBuffer.getNumSamples() < numSamples)
        processBuffer.setSize (1, numSamples, false, false, true);

    auto* work = processBuffer.getWritePointer (0);
    const float inGain = inputGainLinear.load (std::memory_order_relaxed);
    const float outGain = outputGainLinear.load (std::memory_order_relaxed);

    juce::FloatVectorOperations::copyWithMultiply (work, inputChannelData[0], inGain, numSamples);

    float inPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        inPeak = juce::jmax (inPeak, std::abs (work[i]));
    inputPeak.store (inPeak, std::memory_order_relaxed);

    if (! bypass.load (std::memory_order_relaxed) && engine.isLoaded())
        engine.process (work, work, numSamples);

    if (outGain != 1.0f)
        juce::FloatVectorOperations::multiply (work, outGain, numSamples);

    float outPeak = 0.0f;
    for (int i = 0; i < numSamples; ++i)
        outPeak = juce::jmax (outPeak, std::abs (work[i]));
    outputPeak.store (outPeak, std::memory_order_relaxed);

    for (int ch = 0; ch < numOutputChannels; ++ch)
        if (outputChannelData[ch] != nullptr)
            juce::FloatVectorOperations::copy (outputChannelData[ch], work, numSamples);
}

} // namespace jamstudio::amp
