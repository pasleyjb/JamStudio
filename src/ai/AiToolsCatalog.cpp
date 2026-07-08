#include "AiToolsCatalog.h"

namespace jamstudio::ai
{

juce::Array<AiToolInfo> AiToolsCatalog::getToolStatuses (const DemucsSeparator& demucs,
                                                       const WhisperTranscriber& whisper,
                                                       const BasicPitchTranscriber& basicPitch)
{
    juce::Array<AiToolInfo> tools;

    tools.add ({
        "demucs",
        "Demucs",
        "Stem separation (vocals, drums, bass, other)",
        demucs.isAvailable(),
        "pipx install demucs",
        "First run downloads PyTorch models and may take several minutes."
    });

    tools.add ({
        "whisper",
        "Whisper",
        "AI vocal transcription with word-level timing",
        whisper.isAvailable(),
        "pipx install openai-whisper",
        "Uses OpenAI Whisper via Python. GPU optional but faster."
    });

    tools.add ({
        "basic-pitch",
        "basic-pitch",
        "AI note and tab transcription from audio",
        basicPitch.isAvailable(),
        "pipx install basic-pitch",
        "Spotify basic-pitch converts melodic stems to MIDI/tab."
    });

    return tools;
}

juce::String AiToolsCatalog::buildSetupMessage (const juce::Array<AiToolInfo>& tools)
{
    juce::StringArray lines;
    lines.add ("JamStudio uses optional Python tools for AI features.");
    lines.add ("On Linux, install with pipx (recommended):");
    lines.add ("");

    for (const auto& tool : tools)
    {
        lines.add (tool.name + " — " + (tool.available ? "Ready" : "Not installed"));
        lines.add ("  " + tool.purpose);

        if (! tool.available)
            lines.add ("  Install: " + tool.installCommand);

        if (tool.notes.isNotEmpty())
            lines.add ("  Note: " + tool.notes);

        lines.add ("");
    }

    lines.add ("After installing, restart JamStudio.");
    lines.add ("Tools are detected from PATH and ~/.local/bin.");
    return lines.joinIntoString ("\n");
}

juce::String AiToolsCatalog::buildUnavailableHint (const juce::String& toolId,
                                                   const juce::Array<AiToolInfo>& tools)
{
    for (const auto& tool : tools)
    {
        if (tool.id == toolId)
        {
            return tool.name + " is not installed. Install with: " + tool.installCommand
                   + "  (Help > AI Tools Setup for details)";
        }
    }

    return "Required AI tool is not installed. See Help > AI Tools Setup.";
}

} // namespace jamstudio::ai