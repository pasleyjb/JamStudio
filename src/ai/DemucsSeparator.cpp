#include "DemucsSeparator.h"

namespace jamstudio::ai
{

namespace
{
float parseProgressFromOutput (const juce::String& output)
{
    auto lines = juce::StringArray::fromLines (output);

    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();

        if (trimmed.containsChar ('%'))
        {
            const auto percentIndex = trimmed.indexOfChar ('%');

            for (int i = percentIndex - 1; i >= 0; --i)
            {
                if (! juce::CharacterFunctions::isDigit (trimmed[i])
                     && trimmed[i] != '.'
                     && trimmed[i] != ' ')
                    break;

                if (i == 0 || ! juce::CharacterFunctions::isDigit (trimmed[i - 1]))
                {
                    const auto value = trimmed.substring (i, percentIndex).trim().getFloatValue();

                    if (value >= 0.0f && value <= 100.0f)
                        return value / 100.0f;
                }
            }
        }
    }

    return -1.0f;
}

bool commandExists (const juce::String& command)
{
    juce::ChildProcess process;
    juce::StringArray args;

   #if JUCE_WINDOWS
    args.add ("where");
    args.add (command);
   #else
    args.add ("sh");
    args.add ("-c");
    args.add ("command -v " + command);
   #endif

    if (! process.start (args, juce::ChildProcess::wantStdOut))
        return false;

    process.waitForProcessToFinish (5000);
    return process.getExitCode() == 0;
}
} // namespace

DemucsSeparator::DemucsSeparator()
{
    if (commandExists ("demucs"))
        demucsExecutable = "demucs";
    else if (commandExists ("python3"))
        demucsExecutable = "python3 -m demucs";
    else if (commandExists ("python"))
        demucsExecutable = "python -m demucs";
}

DemucsSeparator::~DemucsSeparator()
{
    cancel();
}

bool DemucsSeparator::isAvailable() const
{
    return demucsExecutable.isNotEmpty();
}

void DemucsSeparator::separateAsync (const juce::File& inputFile,
                                     std::function<void (SeparationResult)> onComplete,
                                     SeparationProgressCallback onProgress)
{
    shouldCancel = false;

    juce::Thread::launch ([this, inputFile, onComplete = std::move (onComplete), onProgress = std::move (onProgress)]
    {
        SeparationResult result;

        if (shouldCancel)
        {
            result.errorMessage = "Separation cancelled.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (! isAvailable())
        {
            result.errorMessage = "Demucs is not installed. Install with: pip install demucs";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (! inputFile.existsAsFile())
        {
            result.errorMessage = "Input file does not exist.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        const auto outputDirectory = getOutputDirectory (inputFile);
        outputDirectory.deleteRecursively();
        outputDirectory.createDirectory();

        juce::ChildProcess process;
        const auto command = buildCommand (inputFile, outputDirectory);

        if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            result.errorMessage = "Failed to start Demucs process.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (onProgress != nullptr)
        {
            juce::MessageManager::callAsync ([onProgress]
            {
                onProgress (0.0f, "Starting stem separation...");
            });
        }

        auto accumulatedOutput = juce::String();
        auto lastReportedProgress = -1.0f;

        while (process.isRunning())
        {
            if (shouldCancel)
            {
                process.kill();
                result.errorMessage = "Separation cancelled.";
                juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
                return;
            }

            accumulatedOutput += process.readAllProcessOutput();

            if (onProgress != nullptr)
            {
                const auto parsed = parseProgressFromOutput (accumulatedOutput);

                if (parsed >= 0.0f && parsed > lastReportedProgress)
                {
                    lastReportedProgress = parsed;
                    const auto progressCopy = parsed;
                    juce::MessageManager::callAsync ([onProgress, progressCopy]
                    {
                        onProgress (progressCopy, "Separating stems... " + juce::String (static_cast<int> (progressCopy * 100.0f)) + "%");
                    });
                }
            }

            juce::Thread::sleep (200);
        }

        accumulatedOutput += process.readAllProcessOutput();

        if (process.getExitCode() != 0)
        {
            result.errorMessage = "Demucs failed with exit code " + juce::String (process.getExitCode());
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        result.stemFiles = findStemFiles (outputDirectory);

        if (result.stemFiles.isEmpty())
        {
            result.errorMessage = "Demucs completed but no stem files were found.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        result.success = true;
        juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
    });
}

void DemucsSeparator::cancel()
{
    shouldCancel = true;
}

juce::File DemucsSeparator::getOutputDirectory (const juce::File& inputFile) const
{
    return juce::File::getSpecialLocation (juce::File::tempDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("stems")
        .getChildFile (inputFile.getFileNameWithoutExtension());
}

juce::Array<juce::File> DemucsSeparator::findStemFiles (const juce::File& outputDirectory) const
{
    juce::Array<juce::File> stems;

    if (! outputDirectory.isDirectory())
        return stems;

    for (const auto& entry : juce::RangedDirectoryIterator (outputDirectory, true, "*.wav", juce::File::findFiles))
        stems.add (entry.getFile());

    for (const auto& entry : juce::RangedDirectoryIterator (outputDirectory, true, "*.flac", juce::File::findFiles))
        stems.add (entry.getFile());

    stems.sort();
    return stems;
}

juce::StringArray DemucsSeparator::buildCommand (const juce::File& inputFile,
                                                 const juce::File& outputDirectory) const
{
    juce::StringArray command;

    if (demucsExecutable.contains (" -m "))
    {
        command.addTokens (demucsExecutable, " ", "\"'");
        command.add ("-o");
        command.add (outputDirectory.getFullPathName());
        command.add (inputFile.getFullPathName());
    }
    else
    {
        command.add (demucsExecutable);
        command.add ("-o");
        command.add (outputDirectory.getFullPathName());
        command.add (inputFile.getFullPathName());
    }

    return command;
}

} // namespace jamstudio::ai