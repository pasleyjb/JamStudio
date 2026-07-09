#include "WhisperTranscriber.h"

#include "CommandUtils.h"

namespace jamstudio::ai
{

namespace
{
juce::String progressMessageFromOutput (const juce::String& output, const float progress, const double elapsedSec)
{
    const auto lower = output.toLowerCase();
    const auto pct = juce::String (static_cast<int> (progress * 100.0f)) + "%";

    if (lower.contains ("downloading") || lower.contains ("mib/s") || lower.contains ("kib/s"))
        return "Downloading Whisper model... " + pct;

    if (lower.contains ("detecting language"))
        return "Detecting language...";

    if (lower.contains ("detected language") || lower.contains ("[") /* segment lines */)
        return "Transcribing lyrics... " + pct;

    if (elapsedSec < 8.0)
        return "Loading Whisper model (first run can take a minute)... " + pct;

    if (elapsedSec < 25.0)
        return "Preparing audio for transcription... " + pct;

    return "Transcribing vocals with Whisper... " + pct + "  (CPU can take several minutes)";
}

juce::File findWhisperJson (const juce::File& outputDirectory, const juce::File& audioFile)
{
    const auto preferred = outputDirectory.getChildFile (audioFile.getFileNameWithoutExtension() + ".json");

    if (preferred.existsAsFile())
        return preferred;

    for (const auto& entry : juce::RangedDirectoryIterator (outputDirectory, true, "*.json", juce::File::findFiles))
    {
        // Ignore our own log file name collisions
        if (! entry.getFile().getFileName().endsWithIgnoreCase (".json"))
            continue;

        return entry.getFile();
    }

    return {};
}

juce::String shellQuote (const juce::String& value)
{
    // Safe single-quote wrapping for /bin/bash -lc
    return "'" + value.replace ("'", "'\"'\"'") + "'";
}
} // namespace

WhisperTranscriber::WhisperTranscriber()
{
    whisperExecutable = findWorkingExecutable ({ "whisper", "python3 -m whisper", "python -m whisper" });
}

WhisperTranscriber::~WhisperTranscriber()
{
    cancel();
}

bool WhisperTranscriber::isAvailable() const
{
    return whisperExecutable.isNotEmpty();
}

void WhisperTranscriber::clearActiveProcess()
{
    const std::lock_guard<std::mutex> lock (processMutex);
    activeProcess = nullptr;
}

void WhisperTranscriber::cancel()
{
    shouldCancel = true;

    const std::lock_guard<std::mutex> lock (processMutex);

    if (activeProcess != nullptr)
        activeProcess->kill();
}

void WhisperTranscriber::transcribeAsync (const juce::File& audioFile,
                                          std::function<void (TranscriptionResult)> onComplete,
                                          TranscriptionProgressCallback onProgress)
{
    shouldCancel = false;

    juce::Thread::launch ([this, audioFile, onComplete = std::move (onComplete), onProgress = std::move (onProgress)]
    {
        TranscriptionResult result;
        auto finish = [&] (TranscriptionResult r)
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
            result.errorMessage = "Whisper is not installed. Install with: pip install openai-whisper";
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
            .getChildFile ("whisper")
            .getChildFile (audioFile.getFileNameWithoutExtension());

        outputDirectory.deleteRecursively();
        outputDirectory.createDirectory();

        const auto logFile = outputDirectory.getChildFile ("whisper-run.log");

        // Redirect child output to a log file so the UI thread never blocks on pipe reads.
        // Default openai-whisper model is "turbo" (~1GB) and is very slow on CPU — use "base"
        // for responsive practice-app lyrics (still good enough for song words).
        juce::String shellCmd;
        shellCmd << "export PYTHONUNBUFFERED=1; "
                 << "exec " << shellQuote (whisperExecutable) << " "
                 << shellQuote (audioFile.getFullPathName())
                 << " --model base"
                 << " --device cpu"
                 << " --output_format json"
                 << " --word_timestamps True"
                 << " --fp16 False"
                 << " --verbose True"
                 << " --output_dir " << shellQuote (outputDirectory.getFullPathName())
                 << " > " << shellQuote (logFile.getFullPathName()) << " 2>&1";

        juce::StringArray command;
        command.add ("/bin/bash");
        command.add ("-lc");
        command.add (shellCmd);

        juce::ChildProcess process;

        {
            const std::lock_guard<std::mutex> lock (processMutex);
            activeProcess = &process;
        }

        if (! process.start (command))
        {
            result.errorMessage = "Failed to start Whisper process.";
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
                onProgress (0.04f, "Starting Whisper (base model, CPU)...");
            });
        }

        auto lastReportedProgress = -1.0f;
        const auto startMs = juce::Time::getMillisecondCounterHiRes();

        while (process.isRunning())
        {
            if (shouldCancel)
            {
                process.kill();
                break;
            }

            // Non-blocking: poll log file instead of fread on the child pipe.
            const auto accumulatedOutput = logFile.existsAsFile() ? logFile.loadFileAsString() : juce::String();
            const auto elapsedSec = (juce::Time::getMillisecondCounterHiRes() - startMs) / 1000.0;

            if (onProgress != nullptr)
            {
                const auto parsed = parsePercentProgress (accumulatedOutput);
                // Time-based progress always moves; % from tqdm boosts when available.
                // Never let a stale "0%" keep the bar glued at 5%.
                const auto fromTool = parsed >= 0.0f ? (0.08f + parsed * 0.82f) : 0.0f;
                const auto fromTime = estimateRunningProgress (elapsedSec, 75.0);
                const auto progress = juce::jlimit (0.04f, 0.94f, juce::jmax (fromTool, fromTime));
                const auto message = progressMessageFromOutput (accumulatedOutput, progress, elapsedSec);

                if (progress > lastReportedProgress + 0.004f)
                {
                    lastReportedProgress = progress;
                    juce::MessageManager::callAsync ([onProgress, progress, message]
                    {
                        onProgress (progress, message);
                    });
                }
            }

            juce::Thread::sleep (200);
        }

        // Wait briefly for process bookkeeping / final log flush
        process.waitForProcessToFinish (2000);
        clearActiveProcess();

        const auto accumulatedOutput = logFile.existsAsFile() ? logFile.loadFileAsString() : juce::String();

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
                result.errorMessage = "Whisper failed: " + summary;
            else
                result.errorMessage = "Whisper failed with exit code " + juce::String (process.getExitCode());

            finish (std::move (result));
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.97f, "Reading Whisper lyrics...");
            });
        }

        const auto jsonFile = findWhisperJson (outputDirectory, audioFile);

        if (! jsonFile.existsAsFile())
        {
            result.errorMessage = "Whisper completed but no JSON output was found."
                                  + (accumulatedOutput.isNotEmpty()
                                         ? (" Log: " + extractProcessErrorSummary (accumulatedOutput))
                                         : juce::String());
            finish (std::move (result));
            return;
        }

        juce::String parseError;

        if (! parseWhisperJson (jsonFile, result.lyrics, parseError))
        {
            result.errorMessage = parseError;
            finish (std::move (result));
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (1.0f, "Vocal transcription complete.");
            });
        }

        result.success = true;
        finish (std::move (result));
    });
}

bool WhisperTranscriber::parseWhisperJson (const juce::File& jsonFile,
                                           jamstudio::notation::LyricsTrack& lyrics,
                                           juce::String& error) const
{
    const auto jsonText = jsonFile.loadFileAsString();

    if (jsonText.isEmpty())
    {
        error = "Whisper JSON output is empty.";
        return false;
    }

    const auto parsed = juce::JSON::parse (jsonText);

    if (parsed.isVoid())
    {
        error = "Failed to parse Whisper JSON output.";
        return false;
    }

    if (const auto* root = parsed.getDynamicObject())
    {
        if (const auto* segments = root->getProperty ("segments").getArray())
        {
            lyrics.clear();
            lyrics.setTitle ("AI Transcription");

            for (const auto& segmentVar : *segments)
            {
                if (const auto* segment = segmentVar.getDynamicObject())
                {
                    jamstudio::notation::LyricLine line;
                    line.startSeconds = segment->getProperty ("start");
                    line.endSeconds = segment->getProperty ("end");
                    line.text = segment->getProperty ("text").toString().trim();

                    if (const auto* words = segment->getProperty ("words").getArray())
                    {
                        for (const auto& wordVar : *words)
                        {
                            if (const auto* wordObj = wordVar.getDynamicObject())
                            {
                                jamstudio::notation::LyricWord word;
                                word.text = wordObj->getProperty ("word").toString().trim();
                                word.startSeconds = wordObj->getProperty ("start");
                                word.endSeconds = wordObj->getProperty ("end");

                                if (word.text.isNotEmpty())
                                    line.words.push_back (std::move (word));
                            }
                        }
                    }

                    if (line.text.isEmpty() && ! line.words.empty())
                    {
                        juce::StringArray parts;

                        for (const auto& word : line.words)
                            parts.add (word.text);

                        line.text = parts.joinIntoString (" ");
                    }

                    if (line.text.isNotEmpty())
                        lyrics.addLine (std::move (line));
                }
            }

            if (lyrics.isEmpty())
            {
                error = "Whisper found no lyrics in the audio.";
                return false;
            }

            return true;
        }
    }

    error = "Whisper JSON output has no segments.";
    return false;
}

} // namespace jamstudio::ai
