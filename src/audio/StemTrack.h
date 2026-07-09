#pragma once

#include "StemType.h"

#include <atomic>

namespace jamstudio::audio
{

/** A single separated or imported audio stem with mixer controls. */
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
    void setVolume (float newVolume) noexcept { volume = juce::jlimit (0.0f, 1.0f, newVolume); }

    [[nodiscard]] bool isMuted() const noexcept { return muted; }
    [[nodiscard]] bool isSolo() const noexcept { return solo; }
    [[nodiscard]] float getVolume() const noexcept { return volume; }

    /** Instantaneous envelope level 0..1 for meter drawing (thread-safe). */
    [[nodiscard]] float getMeterLevel() const noexcept { return meterLevel.load (std::memory_order_relaxed); }

    /** Peak-hold level 0..1 — sticks at highest peak like old stereo meters. */
    [[nodiscard]] float getMeterPeakHold() const noexcept { return meterPeakHold.load (std::memory_order_relaxed); }

    /** Call on the message thread (~30 Hz) to decay the sticky peak hold. */
    void tickMeterPeakHold (float deltaSeconds) noexcept;

    /** Reads audio starting at absolute song time, resampling file rate → device rate. */
    void readIntoBuffer (juce::AudioBuffer<float>& output,
                         double startSeconds,
                         int numOutputSamples,
                         double deviceSampleRate,
                         bool anySoloActive) const;

    [[nodiscard]] double getFileSampleRate() const noexcept;

private:
    void updateMeterFromBuffer (const juce::AudioBuffer<float>& buffer, float gain) const noexcept;

    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::File sourceFile;
    juce::String name;
    StemType type = StemType::unknown;
    float volume = 0.8f;
    bool muted = false;
    bool solo = false;

    mutable std::atomic<float> meterLevel { 0.0f };
    mutable std::atomic<float> meterPeakHold { 0.0f };
    mutable std::atomic<float> peakHoldTimer { 0.0f };
};

} // namespace jamstudio::audio
