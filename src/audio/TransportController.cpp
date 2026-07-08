#include "TransportController.h"

namespace jamstudio::audio
{

TransportController::TransportController (juce::AudioDeviceManager& deviceManager)
    : stemMixer (formatManager)
{
    formatManager.registerBasicFormats();

    masterMixer.addInputSource (&stemMixer, false);
    masterMixer.addInputSource (&metronome, false);
    audioSourcePlayer.setSource (&masterMixer);

    deviceManager.addAudioCallback (&audioSourcePlayer);
}

void TransportController::play()
{
    stemMixer.play();
    sendChangeMessage();
}

void TransportController::pause()
{
    stemMixer.pause();
    sendChangeMessage();
}

void TransportController::stop()
{
    stemMixer.stop();
    metronome.reset();
    sendChangeMessage();
}

void TransportController::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

bool TransportController::isPlaying() const noexcept
{
    return stemMixer.isPlaying();
}

void TransportController::setPosition (const double seconds)
{
    stemMixer.setPosition (seconds);
    metronome.reset();
    sendChangeMessage();
}

double TransportController::getPosition() const noexcept
{
    return stemMixer.getPosition();
}

double TransportController::getLengthInSeconds() const noexcept
{
    return stemMixer.getLengthInSeconds();
}

} // namespace jamstudio::audio