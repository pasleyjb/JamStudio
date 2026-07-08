#include "WhisperTranscriber.h"

#include "CommandUtils.h"

namespace jamstudio::ai
{

WhisperTranscriber::WhisperTranscriber()
{
    whisperExecutable = findWorkingExecutable ({ "whisper", "python3 -m whisper", "python -m whisper" });
}

bool WhisperTranscriber::isAvailable() const
{
    return whisperExecutable.isNotEmpty();
}

void WhisperTranscriber::transcribeAsync (const juce::File& audioFile,
                                          std::function<void (TranscriptionResult)> onComplete,
                                          TranscriptionProgressCallback onProgress)
{
    shouldCancel = false;

    juce::Thread::launch ([this, audioFile, onComplete = std::move (onComplete), onProgress = std::move (onProgress)]
    {
        TranscriptionResult result;

        if (shouldCancel)
        {
            result.errorMessage = "Transcription cancelled.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (! isAvailable())
        {
            result.errorMessage = "Whisper is not installed. Install with: pip install openai-whisper";
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
            .getChildFile ("whisper")
            .getChildFile (audioFile.getFileNameWithoutExtension());

        outputDirectory.deleteRecursively();
        outputDirectory.createDirectory();

        juce::StringArray command;

        if (whisperExecutable.contains (" -m "))
        {
            command.addTokens (whisperExecutable, " ", "\"'");
            command.add (audioFile.getFullPathName());
            command.add ("--output_format");
            command.add ("json");
            command.add ("--word_timestamps");
            command.add ("True");
            command.add ("--output_dir");
            command.add (outputDirectory.getFullPathName());
        }
        else
        {
            command.add (whisperExecutable);
            command.add (audioFile.getFullPathName());
            command.add ("--output_format");
            command.add ("json");
            command.add ("--word_timestamps");
            command.add ("True");
            command.add ("--output_dir");
            command.add (outputDirectory.getFullPathName());
        }

        juce::ChildProcess process;

        if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            result.errorMessage = "Failed to start Whisper process.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.05f, "Transcribing vocals with Whisper...");
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
                    onProgress (0.5f, "Transcribing vocals with Whisper...");
                });
            }

            juce::Thread::sleep (500);
        }

        const auto processOutput = process.readAllProcessOutput();

        if (process.getExitCode() != 0)
        {
            const auto summary = extractProcessErrorSummary (processOutput);

            if (summary.isNotEmpty())
                result.errorMessage = "Whisper failed: " + summary;
            else
                result.errorMessage = "Whisper failed with exit code " + juce::String (process.getExitCode());

            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto jsonFile = outputDirectory.getChildFile (audioFile.getFileNameWithoutExtension() + ".json");

        if (! jsonFile.existsAsFile())
        {
            result.errorMessage = "Whisper completed but no JSON output was found.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        juce::String parseError;

        if (! parseWhisperJson (jsonFile, result.lyrics, parseError))
        {
            result.errorMessage = parseError;
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
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
        juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
    });
}

void WhisperTranscriber::cancel()
{
    shouldCancel = true;
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