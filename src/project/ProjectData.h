#pragma once

#include <JuceHeader.h>

namespace jamstudio::project
{

struct StemState
{
    juce::String filePath;
    juce::String name;
    bool muted = false;
    bool solo = false;
    float volume = 0.8f;
};

struct ProjectData
{
    static constexpr int currentVersion = 2;

    int version = currentVersion;
    juce::String songFilePath;
    juce::String scoreFilePath;
    juce::String lyricsFilePath;
    juce::Array<StemState> stems;
    bool metronomeEnabled = false;
    double metronomeBpm = 120.0;
    double transportPosition = 0.0;
    float masterVolume = 1.0f;

    bool hasEmbeddedScore = false;
    bool hasEmbeddedLyrics = false;
    juce::var embeddedScore;
    juce::var embeddedLyrics;
};

} // namespace jamstudio::project