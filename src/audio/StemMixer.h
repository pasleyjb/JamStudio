#pragma once

#include "MixBus.h"
#include "StemTrack.h"

#include <array>

namespace jamstudio::audio
{

/**
 * Mixes stems to multiple stereo buses (FOH + monitors).
 * Output layout when device has enough channels (stereo pairs):
 *   1-2 FOH, 3-4 M1, 5-6 M2, 7-8 M3, 9-10 M4, 11-12 M5
 * Fewer channels → only the buses that fit (FOH first).
 * With stereo-fold PC listen, all buses are still mixed internally.
 */
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
    void setStemVolume (int index, float volume); // FOH
    void setStemBusSend (int index, MixBus bus, float gain);
    void setStemName (int index, const juce::String& name);
    bool removeStemByFile (const juce::File& file);

    void setMasterVolume (float volume) noexcept; // FOH master alias
    [[nodiscard]] float getMasterVolume() const noexcept { return getBusMaster (MixBus::foh); }

    void setBusMaster (MixBus bus, float volume) noexcept;
    [[nodiscard]] float getBusMaster (MixBus bus) const noexcept;

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
    void sortStemsForPractice();
    void recomputeLength();

    juce::AudioFormatManager& formatManager;
    std::vector<std::unique_ptr<StemTrack>> stems;
    std::array<float, kNumMixBuses> busMaster { 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };
    double deviceSampleRate = 44100.0;
    double positionSeconds = 0.0;
    double lengthSeconds = 0.0;
    bool playing = false;
    juce::AudioBuffer<float> dryScratch;
};

} // namespace jamstudio::audio
