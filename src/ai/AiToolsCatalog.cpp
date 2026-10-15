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
        "Stem separation with dedicated guitar (htdemucs_6s)",
        demucs.isAvailable(),
        "pipx install demucs && pipx inject demucs torchcodec",
        "Included in the Mac, Windows, and Linux downloads (htdemucs_6s). Install the command above only if that copy is missing."
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
        "uv-based install (Python 3.11 required)",
        "pipx fails on Python 3.14. Install uv, then: uv python install 3.11; uv venv ~/.local/share/jamstudio-venvs/basic-pitch --python 3.11; uv pip install --python ~/.local/share/jamstudio-venvs/basic-pitch basic-pitch 'setuptools<81'; ln -sf ~/.local/share/jamstudio-venvs/basic-pitch/bin/basic-pitch ~/.local/bin/"
    });

    return tools;
}

juce::String AiToolsCatalog::buildSetupMessage (const juce::Array<AiToolInfo>& tools)
{
    juce::StringArray lines;
    lines.add ("JamStudio uses optional Python tools for AI features.");
    lines.add ("On macOS and Linux, install with pipx (recommended).");
    lines.add ("macOS: brew install pipx ffmpeg, run pipx ensurepath, then restart JamStudio.");
    lines.add ("JamStudio looks on PATH plus Homebrew (/opt/homebrew/bin, /usr/local/bin) and ~/.local/bin.");
    lines.add ("");

    for (const auto& tool : tools)
    {
        lines.add (tool.name + " - " + (tool.available ? "Ready" : "Not installed"));
        lines.add ("  " + tool.purpose);

        if (! tool.available)
            lines.add ("  Install: " + tool.installCommand);

        if (tool.notes.isNotEmpty())
            lines.add ("  Note: " + tool.notes);

        lines.add ("");
    }

    lines.add ("After installing, restart JamStudio.");
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