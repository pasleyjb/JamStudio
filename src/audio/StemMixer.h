#pragma once

#include "StemTrack.h"

namespace jamstudio::audio
{

/** Mixes multiple stem tracks from a shared playback position. */
class StemMixer : public juce::AudioSource,
                  public juce::ChangeBroadcaster
{
public:
    explicit StemMixer (juce::AudioFormatManager& formatManager);

    void clear();
    bool loadStem (const juce::File& file);
    bool loadStems (const juce::Array<juce::File>& files);

    [[nodiscard]] int getNumStems() const noexcept { return static_cast<int> (stems.size()); }
    [[nodiscard]] StemTrack* getStem (int index) noexcept;
    [[nodiscard]] const StemTrack* getStem (int index) const noexcept;

    void setStemMuted (int index, bool muted);
    void setStemSolo (int index, bool solo);
    void setStemVolume (int index, float volume);

    void setMasterVolume (float volume) noexcept;
    [[nodiscard]] float getMasterVolume() const noexcept { return masterVolume; }

    void play();
    void pause();
    void stop();
    [[nodiscard]] bool isPlaying() const noexcept { return playing; }

    void setPosition (double seconds);
    [[nodiscard]] double getPosition() const noexcept;
    [[nodiscard]] double getLengthInSeconds() const noexcept;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    [[nodiscard]] bool anyStemSoloed() const noexcept;
    [[nodiscard]] int64 secondsToSamples (double seconds) const noexcept;
    [[nodiscard]] double samplesToSeconds (int64 samples) const noexcept;

    juce::AudioFormatManager& formatManager;
    std::vector<std::unique_ptr<StemTrack>> stems;
    double sampleRate = 44100.0;
    int64 currentSamplePosition = 0;
    int64 totalSamples = 0;
    bool playing = false;
    float masterVolume = 1.0f;
};

} // namespace jamstudio::audio