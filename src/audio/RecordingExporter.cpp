#include "RecordingExporter.h"

namespace jamstudio::audio
{

namespace
{
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

bool convertWithWriter (juce::AudioFormatManager& formatManager,
                        const juce::File& inputFile,
                        const juce::File& outputFile,
                        juce::AudioFormat& outputFormat,
                        juce::String& error)
{
    if (auto reader = std::unique_ptr<juce::AudioFormatReader> (formatManager.createReaderFor (inputFile)))
    {
        outputFile.deleteFile();

        if (auto outputStream = std::unique_ptr<juce::OutputStream> (outputFile.createOutputStream()))
        {
            const auto options = juce::AudioFormatWriterOptions{}
                                     .withSampleRate (reader->sampleRate)
                                     .withNumChannels (static_cast<int> (reader->numChannels))
                                     .withBitsPerSample (16);

            if (auto writer = outputFormat.createWriterFor (outputStream, options))
            {
                juce::ignoreUnused (outputStream);
                const int bufferSize = 8192;
                juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels), bufferSize);
                int64 samplesWritten = 0;

                while (samplesWritten < reader->lengthInSamples)
                {
                    const auto samplesToRead = static_cast<int> (juce::jmin<int64> (bufferSize,
                                                                                   reader->lengthInSamples - samplesWritten));

                    if (! reader->read (&buffer, 0, samplesToRead, samplesWritten, true, true))
                        break;

                    writer->writeFromAudioSampleBuffer (buffer, 0, samplesToRead);
                    samplesWritten += samplesToRead;
                }

                return true;
            }
        }
    }

    error = "Failed to convert recording.";
    return false;
}
} // namespace

RecordingExporter::RecordingExporter (juce::AudioFormatManager& manager)
    : formatManager (manager)
{
    lameAvailable = commandExists ("lame");
}

bool RecordingExporter::isMp3ExportAvailable() const
{
    return lameAvailable;
}

RecordingExportResult RecordingExporter::exportRecording (const juce::File& wavFile)
{
    RecordingExportResult result;
    result.wavFile = wavFile;

    if (! wavFile.existsAsFile())
    {
        result.errorMessage = "Recording file does not exist.";
        return result;
    }

    result.success = true;

    const auto baseName = wavFile.getFileNameWithoutExtension();
    const auto parent = wavFile.getParentDirectory();
    result.oggFile = parent.getChildFile (baseName + ".ogg");

    juce::String oggError;

    juce::OggVorbisAudioFormat oggFormat;

    if (convertWithWriter (formatManager, wavFile, result.oggFile, oggFormat, oggError))
    {
        result.success = true;
    }
    else
    {
        result.oggFile = juce::File();
    }

    if (lameAvailable)
    {
        result.mp3File = parent.getChildFile (baseName + ".mp3");
        juce::String mp3Error;

        if (! exportToMp3 (wavFile, result.mp3File, mp3Error))
        {
            result.mp3File = juce::File();
        }
    }

    return result;
}

bool RecordingExporter::exportToOgg (const juce::File& inputFile,
                                     const juce::File& outputFile,
                                     juce::String& error) const
{
    juce::OggVorbisAudioFormat oggFormat;
    return convertWithWriter (formatManager, inputFile, outputFile, oggFormat, error);
}

bool RecordingExporter::exportToMp3 (const juce::File& inputFile,
                                     const juce::File& outputFile,
                                     juce::String& error) const
{
    outputFile.deleteFile();

    juce::ChildProcess process;
    juce::StringArray command;
    command.add ("lame");
    command.add ("-b");
    command.add ("192");
    command.add (inputFile.getFullPathName());
    command.add (outputFile.getFullPathName());

    if (! process.start (command))
    {
        error = "Failed to start LAME encoder.";
        return false;
    }

    process.waitForProcessToFinish (120000);

    if (process.getExitCode() != 0 || ! outputFile.existsAsFile())
    {
        error = "LAME encoding failed.";
        return false;
    }

    return true;
}

} // namespace jamstudio::audio