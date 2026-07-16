#pragma once

#include "Metronome.h"
#include "MixBus.h"
#include "StageMediaPlayer.h"
#include "StemMixer.h"

#include <array>
#include <atomic>

namespace jamstudio::audio
{

/**
 * Top-level mix matrix: stems + click + stage media into FOH / Mon A / Mon B.
 *
 * Always builds a full FOH + Mon1–5 matrix (up to 12 channels), then either:
 *  - copies all buses to a multi-out interface, or
 *  - folds the selected bus (or sum) to stereo for PC / virtual-interface listen.
 */
class MultiBusMaster : public juce::AudioSource
{
public:
    MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage);

    void setClickBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getClickBusSend (MixBus bus) const noexcept;

    void setStageBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getStageBusSend (MixBus bus) const noexcept;

    /** Which bus is heard on stereo / PC speakers (and optional multi-out cue). */
    void setOutputMonitorSelect (OutputMonitorSelect select) noexcept;
    [[nodiscard]] OutputMonitorSelect getOutputMonitorSelect() const noexcept;

    /**
     * When true, only the selected monitor bus is sent to outs 1–2 (PC / headphones),
     * even if the device has 6+ channels. When false and the device has 6+ outs,
     * the full FOH/Mon A/Mon B matrix is written to hardware.
     */
    void setStereoFoldListen (bool shouldFold) noexcept;
    [[nodiscard]] bool isStereoFoldListen() const noexcept;

    /** Peak levels 0..1 for each stereo bus after the matrix (for meters). */
    [[nodiscard]] float getBusMeterLevel (MixBus bus) const noexcept;

    /**
     * User-facing bus names (e.g. "Jay IEM", "Vocals"). Empty / default short name
     * falls back to mixBusName / mixBusLongName. Persisted with load/saveSettings.
     */
    void setBusDisplayName (MixBus bus, juce::String name);
    [[nodiscard]] juce::String getBusDisplayName (MixBus bus) const;
    [[nodiscard]] juce::String getBusLongDisplayName (MixBus bus) const;
    /** Label for PC listen combo: "Name (1-2)" or default hardware form. */
    [[nodiscard]] juce::String getOutputMonitorSelectDisplayName (OutputMonitorSelect select) const;

    void loadSettings();
    void saveSettings() const;

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

    void updateBusMeters (const juce::AudioBuffer<float>& busBuffer, int numSamples) noexcept;
    void foldMonitorToStereo (const juce::AudioBuffer<float>& busBuffer,
                              juce::AudioBuffer<float>& dest,
                              int destStart,
                              int numSamples) const;

    StemMixer& stemMixer;
    Metronome& metronome;
    StageMediaPlayer& stageMedia;

    std::array<float, kNumMixBuses> clickSend = defaultClickBusSends();
    std::array<float, kNumMixBuses> stageSend = defaultStageBusSends();
    std::array<juce::String, kNumMixBuses> busDisplayNames {};

    std::atomic<int> monitorSelect { static_cast<int> (OutputMonitorSelect::foh) };
    std::atomic<bool> stereoFoldListen { true }; // good default for PC / virtual Scarlett

    juce::AudioBuffer<float> auxScratch;
    juce::AudioBuffer<float> busScratch; // always kMaxMixChannels

    std::array<std::atomic<float>, kNumMixBuses> busMeter {};
    mutable juce::CriticalSection labelLock;
};

} // namespace jamstudio::audio
