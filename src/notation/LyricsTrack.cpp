#include "LyricsTrack.h"

namespace jamstudio::notation
{

namespace
{
juce::var wordToVar (const LyricWord& word)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("text", word.text);
    obj->setProperty ("start", word.startSeconds);
    obj->setProperty ("end", word.endSeconds);
    return juce::var (obj);
}

LyricWord varToWord (const juce::var& value)
{
    LyricWord word;

    if (const auto* obj = value.getDynamicObject())
    {
        word.text = obj->getProperty ("text").toString();
        word.startSeconds = obj->getProperty ("start");
        word.endSeconds = obj->getProperty ("end");
    }

    return word;
}

juce::var lineToVar (const LyricLine& line)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("text", line.text);
    obj->setProperty ("start", line.startSeconds);
    obj->setProperty ("end", line.endSeconds);

    juce::Array<juce::var> words;

    for (const auto& word : line.words)
        words.add (wordToVar (word));

    obj->setProperty ("words", words);
    return juce::var (obj);
}

LyricLine varToLine (const juce::var& value)
{
    LyricLine line;

    if (const auto* obj = value.getDynamicObject())
    {
        line.text = obj->getProperty ("text").toString();
        line.startSeconds = obj->getProperty ("start");
        line.endSeconds = obj->getProperty ("end");

        if (const auto* words = obj->getProperty ("words").getArray())
        {
            for (const auto& wordVar : *words)
                line.words.push_back (varToWord (wordVar));
        }
    }

    return line;
}
} // namespace

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

juce::var LyricsTrack::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("title", title);
    root->setProperty ("wordTimings", wordTimingsAvailable);

    juce::Array<juce::var> lineArray;

    for (const auto& line : lines)
        lineArray.add (lineToVar (line));

    root->setProperty ("lines", lineArray);
    return juce::var (root);
}

bool LyricsTrack::fromVar (const juce::var& data, LyricsTrack& track)
{
    const auto* root = data.getDynamicObject();

    if (root == nullptr)
        return false;

    track.clear();
    track.title = root->getProperty ("title").toString();
    track.wordTimingsAvailable = static_cast<bool> (root->getProperty ("wordTimings"));

    if (const auto* lineArray = root->getProperty ("lines").getArray())
    {
        for (const auto& lineVar : *lineArray)
            track.addLine (varToLine (lineVar));
    }

    return ! track.isEmpty();
}

} // namespace jamstudio::notation