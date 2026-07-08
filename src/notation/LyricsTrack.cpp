#include "LyricsTrack.h"

namespace jamstudio::notation
{

void LyricsTrack::clear()
{
    lines.clear();
    title = {};
    wordTimingsAvailable = false;
}

void LyricsTrack::addLine (const double startSeconds, const juce::String& text)
{
    if (text.trim().isEmpty())
        return;

    LyricLine line;
    line.startSeconds = startSeconds;
    line.text = text.trim();
    lines.push_back (std::move (line));
}

void LyricsTrack::addLine (LyricLine line)
{
    if (line.text.trim().isEmpty() && line.words.empty())
        return;

    if (! line.words.empty())
        wordTimingsAvailable = true;

    lines.push_back (std::move (line));
}

const LyricLine* LyricsTrack::getLine (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (lines.size())))
        return nullptr;

    return &lines[static_cast<size_t> (index)];
}

int LyricsTrack::getActiveLineIndex (const double seconds) const noexcept
{
    if (lines.empty())
        return -1;

    auto active = 0;

    for (int i = 0; i < static_cast<int> (lines.size()); ++i)
    {
        if (lines[static_cast<size_t> (i)].startSeconds <= seconds)
            active = i;
        else
            break;
    }

    return active;
}

int LyricsTrack::getActiveWordIndex (const int lineIndex, const double seconds) const noexcept
{
    if (const auto* line = getLine (lineIndex))
    {
        if (line->words.empty())
            return -1;

        for (int i = static_cast<int> (line->words.size()) - 1; i >= 0; --i)
        {
            if (line->words[static_cast<size_t> (i)].startSeconds <= seconds)
                return i;
        }
    }

    return -1;
}

double LyricsTrack::getLineEndSeconds (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (lines.size())))
        return 0.0;

    const auto& line = lines[static_cast<size_t> (index)];

    if (line.endSeconds > line.startSeconds)
        return line.endSeconds;

    if (index + 1 < static_cast<int> (lines.size()))
        return lines[static_cast<size_t> (index + 1)].startSeconds;

    return line.startSeconds + 8.0;
}

} // namespace jamstudio::notation