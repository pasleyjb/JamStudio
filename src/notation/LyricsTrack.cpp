#include "LyricsTrack.h"

namespace jamstudio::notation
{

void LyricsTrack::clear()
{
    lines.clear();
    title = {};
}

void LyricsTrack::addLine (const double startSeconds, const juce::String& text)
{
    if (text.trim().isEmpty())
        return;

    lines.push_back ({ startSeconds, text.trim() });
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

double LyricsTrack::getLineEndSeconds (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (lines.size())))
        return 0.0;

    if (index + 1 < static_cast<int> (lines.size()))
        return lines[static_cast<size_t> (index + 1)].startSeconds;

    return lines[static_cast<size_t> (index)].startSeconds + 8.0;
}

} // namespace jamstudio::notation