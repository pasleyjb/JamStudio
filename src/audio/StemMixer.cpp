#include "StemMixer.h"

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
    currentSamplePosition = 0;
    totalSamples = 0;
    sendChangeMessage();
}

bool StemMixer::loadStem (const juce::File& file)
{
    auto track = std::make_unique<StemTrack>();

    if (! track->loadFromFile (file, formatManager))
        return false;

    if (const auto* reader = track->getReader())
        totalSamples = juce::jmax (totalSamples, reader->lengthInSamples);

    stems.push_back (std::move (track));
    sendChangeMessage();
    return true;
}

bool StemMixer::loadStems (const juce::Array<juce::File>& files)
{
    clear();

    auto loadedAny = false;

    for (const auto& file : files)
        loadedAny = loadStem (file) || loadedAny;

    return loadedAny;
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
            totalSamples = 0;

            for (const auto& stem : stems)
                if (const auto* reader = stem->getReader())
                    totalSamples = juce::jmax (totalSamples, reader->lengthInSamples);

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
    currentSamplePosition = 0;
    sendChangeMessage();
}

void StemMixer::setPosition (const double seconds)
{
    currentSamplePosition = secondsToSamples (seconds);
    sendChangeMessage();
}

double StemMixer::getPosition() const noexcept
{
    return samplesToSeconds (currentSamplePosition);
}

double StemMixer::getLengthInSeconds() const noexcept
{
    return samplesToSeconds (totalSamples);
}

void StemMixer::prepareToPlay (const int /*samplesPerBlockExpected*/, const double newSampleRate)
{
    sampleRate = newSampleRate;
}

void StemMixer::releaseResources()
{
    pause();
}

void StemMixer::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (! playing || stems.empty() || sampleRate <= 0.0)
        return;

    const auto numSamples = bufferToFill.numSamples;
    const auto anySolo = anyStemSoloed();

    for (const auto& stem : stems)
        stem->readIntoBuffer (*bufferToFill.buffer,
                              currentSamplePosition,
                              numSamples,
                              anySolo);

    if (masterVolume < 0.999f)
        bufferToFill.buffer->applyGain (bufferToFill.startSample, numSamples, masterVolume);

    currentSamplePosition += numSamples;

    if (currentSamplePosition >= totalSamples)
    {
        currentSamplePosition = totalSamples;
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

int64 StemMixer::secondsToSamples (const double seconds) const noexcept
{
    return static_cast<int64> (seconds * sampleRate);
}

double StemMixer::samplesToSeconds (const int64 samples) const noexcept
{
    if (sampleRate <= 0.0)
        return 0.0;

    return static_cast<double> (samples) / sampleRate;
}

} // namespace jamstudio::audio