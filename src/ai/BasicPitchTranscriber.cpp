#include "BasicPitchTranscriber.h"

#include "../notation/MidiScoreConverter.h"
#include "CommandUtils.h"

namespace jamstudio::ai
{

BasicPitchTranscriber::BasicPitchTranscriber()
{
    if (commandExists ("basic-pitch"))
        basicPitchExecutable = "basic-pitch";
    else if (commandExists ("python3"))
        basicPitchExecutable = "python3 -m basic_pitch";
    else if (commandExists ("python"))
        basicPitchExecutable = "python -m basic_pitch";
}

bool BasicPitchTranscriber::isAvailable() const
{
    return basicPitchExecutable.isNotEmpty();
}

void BasicPitchTranscriber::transcribeAsync (const juce::File& audioFile,
                                             std::function<void (PitchTranscriptionResult)> onComplete,
                                             PitchTranscriptionProgressCallback onProgress)
{
    shouldCancel = false;

    juce::Thread::launch ([this, audioFile, onComplete = std::move (onComplete), onProgress = std::move (onProgress)]
    {
        PitchTranscriptionResult result;

        if (shouldCancel)
        {
            result.errorMessage = "Transcription cancelled.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (! isAvailable())
        {
            result.errorMessage = "basic-pitch is not installed. Install with: pip install basic-pitch";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (! audioFile.existsAsFile())
        {
            result.errorMessage = "Audio file does not exist.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto outputDirectory = juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("JamStudio")
            .getChildFile ("basic-pitch")
            .getChildFile (audioFile.getFileNameWithoutExtension());

        outputDirectory.deleteRecursively();
        outputDirectory.createDirectory();

        juce::StringArray command;

        if (basicPitchExecutable.contains (" -m "))
        {
            command.addTokens (basicPitchExecutable, " ", "\"'");
            command.add (outputDirectory.getFullPathName());
            command.add (audioFile.getFullPathName());
        }
        else
        {
            command.add (basicPitchExecutable);
            command.add (outputDirectory.getFullPathName());
            command.add (audioFile.getFullPathName());
        }

        juce::ChildProcess process;

        if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            result.errorMessage = "Failed to start basic-pitch process.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.05f, "Transcribing notes with basic-pitch...");
            });
        }

        while (process.isRunning())
        {
            if (shouldCancel)
            {
                process.kill();
                result.errorMessage = "Transcription cancelled.";
                juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
                return;
            }

            if (onProgress != nullptr)
            {
                juce::MessageManager::callAsync ([onProgress]
                {
                    onProgress (0.5f, "Transcribing notes with basic-pitch...");
                });
            }

            juce::Thread::sleep (500);
        }

        if (process.getExitCode() != 0)
        {
            result.errorMessage = "basic-pitch failed with exit code " + juce::String (process.getExitCode());
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto midiFile = findMidiFile (outputDirectory);

        if (! midiFile.existsAsFile())
        {
            result.errorMessage = "basic-pitch completed but no MIDI file was found.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        juce::String convertError;

        if (! jamstudio::notation::MidiScoreConverter::convertFile (midiFile, result.score, convertError))
        {
            result.errorMessage = convertError;
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (1.0f, "Note transcription complete.");
            });
        }

        result.success = true;
        juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
    });
}

void BasicPitchTranscriber::cancel()
{
    shouldCancel = true;
}

juce::File BasicPitchTranscriber::findMidiFile (const juce::File& outputDirectory) const
{
    if (! outputDirectory.isDirectory())
        return {};

    for (const auto& entry : juce::RangedDirectoryIterator (outputDirectory, true, "*.mid", juce::File::findFiles))
        return entry.getFile();

    for (const auto& entry : juce::RangedDirectoryIterator (outputDirectory, true, "*.midi", juce::File::findFiles))
        return entry.getFile();

    return {};
}

} // namespace jamstudio::ai