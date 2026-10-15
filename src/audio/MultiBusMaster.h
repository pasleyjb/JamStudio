#pragma once

#include "LiveInputs.h"
#include "Metronome.h"
#include "MixBus.h"
#include "StageMediaPlayer.h"
#include "StemMixer.h"
#include "TakeRecorder.h"

#include <array>
#include <atomic>
#include <cstdint>

namespace jamstudio::audio
{

/**
 * Top-level mix matrix: stems (already multi-bus) + click + stage media
 * routed into FOH / Mon A / Mon B stereo pairs on the device.
 */
class MultiBusMaster : public juce::AudioSource
{
public:
    MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage,
                    TakeRecorder& takes, LiveInputs& live);

    void setClickBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getClickBusSend (MixBus bus) const noexcept;

    void setStageBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getStageBusSend (MixBus bus) const noexcept;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

    /** Peak of the block just rendered, before the output ceiling. */
    [[nodiscard]] float getRenderedPeak() const noexcept
    {
        return renderedPeak.load (std::memory_order_relaxed);
    }

    [[nodiscard]] std::uint32_t getRenderedBlocks() const noexcept
    {
        return renderedBlocks.load (std::memory_order_relaxed);
    }

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
    TakeRecorder& takeRecorder;
    LiveInputs& liveInputs;

    std::array<float, kNumMixBuses> clickSend { 0.0f, 0.85f, 0.55f }; // click mainly in monitors
    std::array<float, kNumMixBuses> stageSend { 1.0f, 0.35f, 0.25f }; // stage FX mainly FOH
    juce::AudioBuffer<float> auxScratch;
    juce::AudioBuffer<float> inputScratch;
    float outputCeiling = 1.0f;
    std::atomic<float> renderedPeak { 0.0f };
    std::atomic<std::uint32_t> renderedBlocks { 0 };
};

} // namespace jamstudio::audio
