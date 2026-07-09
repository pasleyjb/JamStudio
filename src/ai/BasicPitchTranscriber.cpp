#include "BasicPitchTranscriber.h"

#include "../notation/MidiScoreConverter.h"
#include "CommandUtils.h"

namespace jamstudio::ai
{

BasicPitchTranscriber::BasicPitchTranscriber()
{
    const auto venvBasicPitch = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
        .getChildFile (".local")
        .getChildFile ("share")
        .getChildFile ("jamstudio-venvs")
        .getChildFile ("basic-pitch")
        .getChildFile ("bin")
        .getChildFile ("basic-pitch");

    juce::StringArray candidates { "basic-pitch", "python3 -m basic_pitch", "python -m basic_pitch" };

    if (venvBasicPitch.existsAsFile())
        candidates.add (venvBasicPitch.getFullPathName());

    basicPitchExecutable = findWorkingExecutable (candidates);
}

bool BasicPitchTranscriber::isAvailable() const
{
    return basicPitchExecutable.isNotEmpty();
}

BasicPitchTranscriber::~BasicPitchTranscriber()
{
    cancel();
}

void BasicPitchTranscriber::clearActiveProcess()
{
    const std::lock_guard<std::mutex> lock (processMutex);
    activeProcess = nullptr;
}

void BasicPitchTranscriber::cancel()
{
    shouldCancel = true;
    const std::lock_guard<std::mutex> lock (processMutex);

    if (activeProcess != nullptr)
        activeProcess->kill();
}

void BasicPitchTranscriber::transcribeAsync (const juce::File& audioFile,
                                             std::function<void (PitchTranscriptionResult)> onComplete,
                                             PitchTranscriptionProgressCallback onProgress)
{
    shouldCancel = false;

    juce::Thread::launch ([this, audioFile, onComplete = std::move (onComplete), onProgress = std::move (onProgress)]
    {
        PitchTranscriptionResult result;
        auto finish = [&] (PitchTranscriptionResult r)
        {
            clearActiveProcess();
            juce::MessageManager::callAsync ([onComplete, r = std::move (r)] { onComplete (r); });
        };

        if (shouldCancel)
        {
            result.errorMessage = "Transcription cancelled.";
            finish (std::move (result));
            return;
        }

        if (! isAvailable())
        {
            result.errorMessage = "basic-pitch is not installed. Install with: pip install basic-pitch";
            finish (std::move (result));
            return;
        }

        if (! audioFile.existsAsFile())
        {
            result.errorMessage = "Audio file does not exist.";
            finish (std::move (result));
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

        {
            const std::lock_guard<std::mutex> lock (processMutex);
            activeProcess = &process;
        }

        if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            result.errorMessage = "Failed to start basic-pitch process.";
            finish (std::move (result));
            return;
        }

        if (shouldCancel)
        {
            process.kill();
            result.errorMessage = "Transcription cancelled.";
            finish (std::move (result));
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.05f, "Starting basic-pitch transcription...");
            });
        }

        auto accumulatedOutput = juce::String();
        auto lastReportedProgress = -1.0f;
        const auto startMs = juce::Time::getMillisecondCounterHiRes();

        while (process.isRunning())
        {
            if (shouldCancel)
            {
                process.kill();
                break;
            }

            accumulatedOutput += process.readAllProcessOutput();

            if (shouldCancel)
            {
                process.kill();
                break;
            }

            if (onProgress != nullptr)
            {
                const auto elapsedSec = (juce::Time::getMillisecondCounterHiRes() - startMs) / 1000.0;
                const auto parsed = parsePercentProgress (accumulatedOutput);
                const auto progress = parsed >= 0.0f
                                          ? juce::jlimit (0.05f, 0.95f, 0.05f + parsed * 0.9f)
                                          : estimateRunningProgress (elapsedSec, 45.0);
                const auto message = "Transcribing notes with basic-pitch... "
                                     + juce::String (static_cast<int> (progress * 100.0f)) + "%";

                if (progress > lastReportedProgress + 0.005f)
                {
                    lastReportedProgress = progress;
                    juce::MessageManager::callAsync ([onProgress, progress, message]
                    {
                        onProgress (progress, message);
                    });
                }
            }

            juce::Thread::sleep (100);
        }

        accumulatedOutput += process.readAllProcessOutput();
        clearActiveProcess();

        if (shouldCancel)
        {
            result.errorMessage = "Transcription cancelled.";
            finish (std::move (result));
            return;
        }

        if (process.getExitCode() != 0)
        {
            const auto summary = extractProcessErrorSummary (accumulatedOutput);

            if (summary.isNotEmpty())
                result.errorMessage = "basic-pitch failed: " + summary;
            else
                result.errorMessage = "basic-pitch failed with exit code " + juce::String (process.getExitCode());

            finish (std::move (result));
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.97f, "Converting MIDI to tab...");
            });
        }

        const auto midiFile = findMidiFile (outputDirectory);

        if (! midiFile.existsAsFile())
        {
            result.errorMessage = "basic-pitch completed but no MIDI file was found.";
            finish (std::move (result));
            return;
        }

        juce::String convertError;

        if (! jamstudio::notation::MidiScoreConverter::convertFile (midiFile, result.score, convertError))
        {
            result.errorMessage = convertError;
            finish (std::move (result));
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
        finish (std::move (result));
    });
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