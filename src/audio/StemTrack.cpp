#include "StemTrack.h"

namespace jamstudio::audio
{

bool StemTrack::loadFromFile (const juce::File& file, juce::AudioFormatManager& formatManager)
{
    if (! file.existsAsFile())
        return false;

    auto* newReader = formatManager.createReaderFor (file);

    if (newReader == nullptr)
        return false;

    reader.reset (newReader);
    sourceFile = file;
    name = file.getFileNameWithoutExtension();
    type = stemTypeFromFileName (name);
    muted = false;
    solo = false;
    volume = 0.8f;
    return true;
}

void StemTrack::readIntoBuffer (juce::AudioBuffer<float>& output,
                                const int64 startSample,
                                const int numSamples,
                                const bool anySoloActive) const
{
    if (reader == nullptr || numSamples <= 0)
        return;

    const bool isAudible = anySoloActive ? solo : ! muted;

    if (! isAudible || volume <= 0.0f)
        return;

    juce::AudioBuffer<float> tempBuffer (static_cast<int> (reader->numChannels), numSamples);
    tempBuffer.clear();

    if (! reader->read (&tempBuffer, 0, numSamples, startSample, true, true))
        return;

    const auto outputChannels = output.getNumChannels();
    const auto sourceChannels = tempBuffer.getNumChannels();

    for (int channel = 0; channel < outputChannels; ++channel)
    {
        const auto sourceChannel = juce::jmin (channel, sourceChannels - 1);
        output.addFrom (channel, 0, tempBuffer, sourceChannel, 0, numSamples, volume);
    }
}

} // namespace jamstudio::audio