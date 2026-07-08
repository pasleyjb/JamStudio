#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

struct RecordingExportResult
{
    bool success = false;
    juce::String errorMessage;
    juce::File wavFile;
    juce::File mp3File;
    juce::File oggFile;
};

/** Exports recorded audio to WAV and optional compressed formats. */
class RecordingExporter
{
public:
    explicit RecordingExporter (juce::AudioFormatManager& formatManager);

    [[nodiscard]] bool isMp3ExportAvailable() const;

    RecordingExportResult exportRecording (const juce::File& wavFile);

private:
    [[nodiscard]] bool exportToOgg (const juce::File& inputFile, const juce::File& outputFile, juce::String& error) const;
    [[nodiscard]] bool exportToMp3 (const juce::File& inputFile, const juce::File& outputFile, juce::String& error) const;

    juce::AudioFormatManager& formatManager;
    bool lameAvailable = false;
};

} // namespace jamstudio::audio