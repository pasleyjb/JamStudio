#pragma once

#include "StemType.h"

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

    void setMuted (bool shouldMute) noexcept { muted = shouldMute; }
    void setSolo (bool shouldSolo) noexcept { solo = shouldSolo; }
    void setVolume (float newVolume) noexcept { volume = juce::jlimit (0.0f, 1.0f, newVolume); }

    [[nodiscard]] bool isMuted() const noexcept { return muted; }
    [[nodiscard]] bool isSolo() const noexcept { return solo; }
    [[nodiscard]] float getVolume() const noexcept { return volume; }

    void readIntoBuffer (juce::AudioBuffer<float>& output,
                         int64 startSample,
                         int numSamples,
                         bool anySoloActive) const;

private:
    std::unique_ptr<juce::AudioFormatReader> reader;
    juce::File sourceFile;
    juce::String name;
    StemType type = StemType::unknown;
    float volume = 0.8f;
    bool muted = false;
    bool solo = false;
};

} // namespace jamstudio::audio