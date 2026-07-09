#include "StemMixer.h"

#include <algorithm>

namespace jamstudio::audio
{

StemMixer::StemMixer (juce::AudioFormatManager& manager)
    : formatManager (manager)
{
}

void StemMixer::clear()
{
    stop();
    stems.clear();
    positionSeconds = 0.0;
    lengthSeconds = 0.0;
    sendChangeMessage();
}

void StemMixer::recomputeLength()
{
    lengthSeconds = 0.0;

    for (const auto& stem : stems)
    {
        if (const auto* reader = stem->getReader())
        {
            const auto sr = reader->sampleRate > 0.0 ? reader->sampleRate : 44100.0;
            lengthSeconds = juce::jmax (lengthSeconds,
                                        static_cast<double> (reader->lengthInSamples) / sr);
        }
    }
}

bool StemMixer::loadStem (const juce::File& file)
{
    auto track = std::make_unique<StemTrack>();

    if (! track->loadFromFile (file, formatManager))
        return false;

    stems.push_back (std::move (track));
    recomputeLength();
    sendChangeMessage();
    return true;
}

bool StemMixer::loadStems (const juce::Array<juce::File>& files)
{
    clear();

    auto loadedAny = false;

    for (const auto& file : files)
        loadedAny = loadStem (file) || loadedAny;

    if (loadedAny)
        sortStemsForPractice();

    return loadedAny;
}

void StemMixer::sortStemsForPractice()
{
    // Guitar-first order so learners see their instrument at the top of the mixer.
    std::stable_sort (stems.begin(), stems.end(),
                      [] (const std::unique_ptr<StemTrack>& a, const std::unique_ptr<StemTrack>& b)
                      {
                          return stemTypeSortOrder (a->getType()) < stemTypeSortOrder (b->getType());
                      });

    sendChangeMessage();
}

StemTrack* StemMixer::getStem (const int index) noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (stems.size())))
        return nullptr;

    return stems[static_cast<size_t> (index)].get();
}

const StemTrack* StemMixer::getStem (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (stems.size())))
        return nullptr;

    return stems[static_cast<size_t> (index)].get();
}

void StemMixer::setStemMuted (const int index, const bool muted)
{
    if (auto* stem = getStem (index))
    {
        stem->setMuted (muted);
        sendChangeMessage();
    }
}

void StemMixer::setStemSolo (const int index, const bool solo)
{
    if (auto* stem = getStem (index))
    {
        stem->setSolo (solo);
        sendChangeMessage();
    }
}

void StemMixer::setStemVolume (const int index, const float volume)
{
    if (auto* stem = getStem (index))
    {
        stem->setVolume (volume);
        sendChangeMessage();
    }
}

void StemMixer::setStemName (const int index, const juce::String& name)
{
    if (auto* stem = getStem (index))
    {
        stem->setName (name);
        sendChangeMessage();
    }
}

bool StemMixer::removeStemByFile (const juce::File& file)
{
    const auto targetPath = file.getFullPathName();

    for (auto it = stems.begin(); it != stems.end(); ++it)
    {
        if ((*it)->getFile().getFullPathName() == targetPath)
        {
            stems.erase (it);
            recomputeLength();
            positionSeconds = juce::jmin (positionSeconds, lengthSeconds);
            sendChangeMessage();
            return true;
        }
    }

    return false;
}

void StemMixer::setMasterVolume (const float volume) noexcept
{
    masterVolume = juce::jlimit (0.0f, 1.0f, volume);
    sendChangeMessage();
}

void StemMixer::play()
{
    if (stems.empty())
        return;

    playing = true;
    sendChangeMessage();
}

void StemMixer::pause()
{
    playing = false;
    sendChangeMessage();
}

void StemMixer::stop()
{
    playing = false;
    positionSeconds = 0.0;
    sendChangeMessage();
}

void StemMixer::setPosition (const double seconds)
{
    positionSeconds = juce::jlimit (0.0, juce::jmax (0.0, lengthSeconds), seconds);
    sendChangeMessage();
}

double StemMixer::getPosition() const noexcept
{
    return positionSeconds;
}

double StemMixer::getLengthInSeconds() const noexcept
{
    return lengthSeconds;
}

void StemMixer::prepareToPlay (const int /*samplesPerBlockExpected*/, const double newSampleRate)
{
    deviceSampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
}

void StemMixer::releaseResources()
{
    pause();
}

void StemMixer::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (! playing || stems.empty() || deviceSampleRate <= 0.0)
        return;

    const auto numSamples = bufferToFill.numSamples;
    const auto anySolo = anyStemSoloed();
    const auto startSeconds = positionSeconds;

    for (const auto& stem : stems)
        stem->readIntoBuffer (*bufferToFill.buffer,
                              startSeconds,
                              numSamples,
                              deviceSampleRate,
                              anySolo);

    if (masterVolume < 0.999f)
        bufferToFill.buffer->applyGain (bufferToFill.startSample, numSamples, masterVolume);

    positionSeconds += static_cast<double> (numSamples) / deviceSampleRate;

    if (positionSeconds >= lengthSeconds && lengthSeconds > 0.0)
    {
        positionSeconds = lengthSeconds;
        playing = false;
        sendChangeMessage();
    }
}

bool StemMixer::anyStemSoloed() const noexcept
{
    for (const auto& stem : stems)
        if (stem->isSolo())
            return true;

    return false;
}

} // namespace jamstudio::audio