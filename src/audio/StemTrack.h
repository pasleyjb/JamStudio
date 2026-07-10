#pragma once

#include "MixBus.h"
#include "StemType.h"

#include <atomic>
#include <array>

namespace jamstudio::audio
{

/** A single separated or imported audio stem with multi-bus mixer controls. */
class StemTrack
{
public:
    StemTrack() = default;

    bool loadFromFile (const juce::File& file, juce::AudioFormatManager& formatManager);

    [[nodiscard]] bool isLoaded() const noexcept { return reader != nullptr; }
    [[nodiscard]] const juce::AudioFormatReader* getReader() const noexcept { return reader.get(); }
    [[nodiscard]] juce::String getName() const noexcept { return name; }
    [[nodiscard]] StemType getType() const noexcept { return type; }
    [[nodiscard]] juce::File getFile() const noexcept { return sourceFile; }

    void setName (const juce::String& newName) noexcept { name = newName; }
    void setType (StemType newType) noexcept { type = newType; }
    void setMuted (bool shouldMute) noexcept { muted = shouldMute; }
    void setSolo (bool shouldSolo) noexcept { solo = shouldSolo; }

    /** FOH / channel fader (legacy name). Same as bus send for FOH. */
    void setVolume (float newVolume) noexcept { setBusSend (MixBus::foh, newVolume); }
    [[nodiscard]] float getVolume() const noexcept { return getBusSend (MixBus::foh); }

    void setBusSend (MixBus bus, float gain) noexcept;
    [[nodiscard]] float getBusSend (MixBus bus) const noexcept;

    [[nodiscard]] bool isMuted() const noexcept { return muted; }
    [[nodiscard]] bool isSolo() const noexcept { return solo; }

    [[nodiscard]] float getMeterLevel() const noexcept { return meterLevel.load (std::memory_order_relaxed); }
    [[nodiscard]] float getMeterPeakHold() const noexcept { return meterPeakHold.load (std::memory_order_relaxed); }
    void tickMeterPeakHold (float deltaSeconds) noexcept;

    /**
     * Resample dry stem into `temp` (stereo-ish), applying mute/solo only.
     * Returns false if silent / not loaded. Caller applies bus gains.
     */
    bool readDryResampled (juce::AudioBuffer<float>& temp,
                           double startSeconds,
                           int numOutputSamples,
                           double deviceSampleRate,
                           bool anySoloActive) const;

    void updateMeterFromDry (const juce::AudioBuffer<float>& dry, float displayGain) const noexcept;

    [[nodiscard]] double getFileSampleRate() const noexcept;

private:
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::File sourceFile;
    juce::String name;
    StemType type = StemType::unknown;
    std::array<float, kNumMixBuses> busSends { 0.8f, 0.7f, 0.0f }; // FOH, MonA, MonB
    bool muted = false;
    bool solo = false;

    mutable std::atomic<float> meterLevel { 0.0f };
    mutable std::atomic<float> meterPeakHold { 0.0f };
    mutable std::atomic<float> peakHoldTimer { 0.0f };
};

} // namespace jamstudio::audio
