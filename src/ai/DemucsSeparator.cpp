#include "DemucsSeparator.h"

#include "CommandUtils.h"

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
} // namespace

DemucsSeparator::DemucsSeparator()
{
    demucsExecutable = findWorkingExecutable ({ "demucs", "python3 -m demucs", "python -m demucs" });
}

DemucsSeparator::~DemucsSeparator()
{
    cancel();
}

bool DemucsSeparator::isAvailable() const
{
    return demucsExecutable.isNotEmpty();
}

void DemucsSeparator::setModelName (const juce::String& name)
{
    if (name.isNotEmpty())
        modelName = name;
}

juce::String DemucsSeparator::getModelName() const
{
    return modelName;
}

void DemucsSeparator::setShifts (const int newShifts)
{
    shifts = juce::jlimit (0, 10, newShifts);
}

int DemucsSeparator::getShifts() const
{
    return shifts;
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

        {
            const std::lock_guard<std::mutex> lock (processMutex);
            activeProcess = &process;
        }

        if (! process.start (command, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        {
            {
                const std::lock_guard<std::mutex> lock (processMutex);
                activeProcess = nullptr;
            }
            result.errorMessage = "Failed to start Demucs process.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (shouldCancel)
        {
            process.kill();
            {
                const std::lock_guard<std::mutex> lock (processMutex);
                activeProcess = nullptr;
            }
            result.errorMessage = "Separation cancelled.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (onProgress != nullptr)
        {
            const auto startMessage = "Starting stem separation (" + modelName + ")...";
            juce::MessageManager::callAsync ([onProgress, startMessage]
            {
                onProgress (0.0f, startMessage);
            });
        }

        auto accumulatedOutput = juce::String();
        auto lastReportedProgress = -1.0f;

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

            juce::Thread::sleep (100);
        }

        accumulatedOutput += process.readAllProcessOutput();

        {
            const std::lock_guard<std::mutex> lock (processMutex);
            activeProcess = nullptr;
        }

        if (shouldCancel)
        {
            result.errorMessage = "Separation cancelled.";
            juce::MessageManager::callAsync ([onComplete, result] { onComplete (result); });
            return;
        }

        if (process.getExitCode() != 0)
        {
            const auto summary = extractProcessErrorSummary (accumulatedOutput);

            if (summary.isNotEmpty())
                result.errorMessage = "Demucs failed: " + summary;
            else
                result.errorMessage = "Demucs failed with exit code " + juce::String (process.getExitCode());

            if (accumulatedOutput.containsIgnoreCase ("torchcodec"))
            {
                result.errorMessage += " Install the missing dependency with: pipx inject demucs torchcodec";
            }

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
    const std::lock_guard<std::mutex> lock (processMutex);

    if (activeProcess != nullptr)
        activeProcess->kill();
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
        command.addTokens (demucsExecutable, " ", "\"'");
    else
        command.add (demucsExecutable);

    // htdemucs_6s produces dedicated guitar + piano stems (better for practice).
    command.add ("-n");
    command.add (modelName);

    if (shifts > 0)
    {
        command.add ("--shifts");
        command.add (juce::String (shifts));
    }

    command.add ("-o");
    command.add (outputDirectory.getFullPathName());
    command.add (inputFile.getFullPathName());

    return command;
}

} // namespace jamstudio::ai