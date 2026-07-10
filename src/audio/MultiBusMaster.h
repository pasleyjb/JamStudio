#pragma once

#include "Metronome.h"
#include "MixBus.h"
#include "StageMediaPlayer.h"
#include "StemMixer.h"

#include <array>

namespace jamstudio::audio
{

/**
 * Top-level mix matrix: stems (already multi-bus) + click + stage media
 * routed into FOH / Mon A / Mon B stereo pairs on the device.
 */
class MultiBusMaster : public juce::AudioSource
{
public:
    MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage);

    void setClickBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getClickBusSend (MixBus bus) const noexcept;

    void setStageBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getStageBusSend (MixBus bus) const noexcept;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    void addSourceToBuses (const juce::AudioBuffer<float>& source,
                           int startSample,
                           int numSamples,
                           juce::AudioBuffer<float>& dest,
                           int destStart,
                           const std::array<float, kNumMixBuses>& sends);

    StemMixer& stemMixer;
    Metronome& metronome;
    StageMediaPlayer& stageMedia;

    std::array<float, kNumMixBuses> clickSend { 0.0f, 0.85f, 0.55f }; // click mainly in monitors
    std::array<float, kNumMixBuses> stageSend { 1.0f, 0.35f, 0.25f }; // stage FX mainly FOH
    juce::AudioBuffer<float> auxScratch;
};

} // namespace jamstudio::audio
