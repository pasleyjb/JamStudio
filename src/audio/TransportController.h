#pragma once

#include "Metronome.h"
#include "StemMixer.h"

namespace jamstudio::audio
{

/** Owns the master audio graph: stem mixer + metronome. */
class TransportController : public juce::ChangeBroadcaster
{
public:
    explicit TransportController (juce::AudioDeviceManager& deviceManager);

    StemMixer& getStemMixer() noexcept { return stemMixer; }
    Metronome& getMetronome() noexcept { return metronome; }
    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    void play();
    void pause();
    void stop();
    void togglePlayPause();
    [[nodiscard]] bool isPlaying() const noexcept;

    void setPosition (double seconds);
    [[nodiscard]] double getPosition() const noexcept;
    [[nodiscard]] double getLengthInSeconds() const noexcept;

private:
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::MixerAudioSource masterMixer;
    juce::AudioFormatManager formatManager;
    StemMixer stemMixer;
    Metronome metronome;
};

} // namespace jamstudio::audio