#include "LyricsTrack.h"

#include <algorithm>
#include <cmath>

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

juce::String LyricsTrack::sanitizeDisplayText (const juce::String& input)
{
    // Map common “smart” punctuation to plain ASCII the UI font always has.
    juce::String out = input;

    // Dashes / minus variants
    out = out.replaceCharacter (juce::juce_wchar (0x2014), '-'); // em dash
    out = out.replaceCharacter (juce::juce_wchar (0x2013), '-'); // en dash
    out = out.replaceCharacter (juce::juce_wchar (0x2012), '-'); // figure dash
    out = out.replaceCharacter (juce::juce_wchar (0x2010), '-'); // hyphen
    out = out.replaceCharacter (juce::juce_wchar (0x2011), '-'); // non-breaking hyphen
    out = out.replaceCharacter (juce::juce_wchar (0x2212), '-'); // minus
    out = out.replaceCharacter (juce::juce_wchar (0x00AD), '-'); // soft hyphen
    out = out.replaceCharacter (juce::juce_wchar (0x00AF), '-'); // macron (the "a with bar" look-alike when mis-rendered)
    out = out.replaceCharacter (juce::juce_wchar (0x02C9), '-'); // modifier letter macron

    // Quotes
    out = out.replaceCharacter (juce::juce_wchar (0x2018), '\'');
    out = out.replaceCharacter (juce::juce_wchar (0x2019), '\'');
    out = out.replaceCharacter (juce::juce_wchar (0x201A), '\'');
    out = out.replaceCharacter (juce::juce_wchar (0x201B), '\'');
    out = out.replaceCharacter (juce::juce_wchar (0x2032), '\'');
    out = out.replaceCharacter (juce::juce_wchar (0x201C), '"');
    out = out.replaceCharacter (juce::juce_wchar (0x201D), '"');
    out = out.replaceCharacter (juce::juce_wchar (0x201E), '"');
    out = out.replaceCharacter (juce::juce_wchar (0x00AB), '"');
    out = out.replaceCharacter (juce::juce_wchar (0x00BB), '"');

    // Ellipsis / dots / bullets
    out = out.replace (juce::String::charToString (juce::juce_wchar (0x2026)), "...");
    out = out.replaceCharacter (juce::juce_wchar (0x00B7), '-'); // middle dot
    out = out.replaceCharacter (juce::juce_wchar (0x2022), '*'); // bullet
    out = out.replaceCharacter (juce::juce_wchar (0x2023), '*');

    // Spaces
    out = out.replaceCharacter (juce::juce_wchar (0x00A0), ' '); // nbsp
    out = out.replaceCharacter (juce::juce_wchar (0x202F), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x2007), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x2009), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x200A), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x200B), ' '); // zero-width space
    out = out.replaceCharacter (juce::juce_wchar (0xFEFF), ' '); // bom

    // Music / misc that often glitch in default fonts
    out = out.replaceCharacter (juce::juce_wchar (0x266A), ' '); // eighth note
    out = out.replaceCharacter (juce::juce_wchar (0x266B), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x2669), ' ');
    out = out.replaceCharacter (juce::juce_wchar (0x00D7), 'x'); // multiply
    out = out.replaceCharacter (juce::juce_wchar (0x2020), ' '); // dagger
    out = out.replaceCharacter (juce::juce_wchar (0x2021), ' ');

    // Drop control chars and leftover non-printable (keep basic latin + latin-1 letters)
    juce::String cleaned;
    cleaned.preallocateBytes (static_cast<size_t> (out.length()) + 8);

    for (int i = 0; i < out.length(); ++i)
    {
        const auto c = out[i];
        const auto u = static_cast<juce::uint32> (c);

        if (c == '\n' || c == '\r' || c == '\t')
        {
            cleaned << ' ';
            continue;
        }

        // ASCII printable
        if (u >= 32 && u < 127)
        {
            cleaned << c;
            continue;
        }

        // Common Latin-1 letters (accents) — keep; most fonts have these
        if (u >= 0x00C0 && u <= 0x024F)
        {
            cleaned << c;
            continue;
        }

        // Replacement for anything else that often renders as garbage
        if (u >= 0x80)
            cleaned << ' ';
    }

    // Collapse runs of spaces
    while (cleaned.contains ("  "))
        cleaned = cleaned.replace ("  ", " ");

    return cleaned.trim();
}

void LyricsTrack::sanitizeAll()
{
    title = sanitizeDisplayText (title);

    for (auto& line : lines)
    {
        line.text = sanitizeDisplayText (line.text);

        for (auto& word : line.words)
            word.text = sanitizeDisplayText (word.text);
    }
}

void LyricsTrack::setTitle (const juce::String& newTitle)
{
    title = sanitizeDisplayText (newTitle);
}

void LyricsTrack::addLine (const double startSeconds, const juce::String& text)
{
    const auto cleaned = sanitizeDisplayText (text);

    if (cleaned.isEmpty())
        return;

    LyricLine line;
    line.startSeconds = startSeconds;
    line.text = cleaned;
    lines.push_back (std::move (line));
}

void LyricsTrack::addLine (LyricLine line)
{
    line.text = sanitizeDisplayText (line.text);

    for (auto& word : line.words)
        word.text = sanitizeDisplayText (word.text);

    if (line.text.isEmpty() && line.words.empty())
        return;

    // Rebuild line text from words if needed
    if (line.text.isEmpty() && ! line.words.empty())
    {
        juce::StringArray parts;

        for (const auto& w : line.words)
            if (w.text.isNotEmpty())
                parts.add (w.text);

        line.text = parts.joinIntoString (" ");
    }

    if (! line.words.empty())
        wordTimingsAvailable = true;

    lines.push_back (std::move (line));
}

void LyricsTrack::finalizeTiming()
{
    if (lines.empty())
        return;

    sanitizeAll();

    std::stable_sort (lines.begin(), lines.end(),
                      [] (const LyricLine& a, const LyricLine& b)
                      {
                          return a.startSeconds < b.startSeconds;
                      });

    for (size_t i = 0; i < lines.size(); ++i)
    {
        auto& line = lines[i];

        if (line.endSeconds <= line.startSeconds)
        {
            if (i + 1 < lines.size())
                line.endSeconds = lines[i + 1].startSeconds;
            else
                line.endSeconds = line.startSeconds + 8.0;
        }

        for (size_t w = 0; w < line.words.size(); ++w)
        {
            auto& word = line.words[w];

            if (word.endSeconds <= word.startSeconds)
            {
                if (w + 1 < line.words.size())
                    word.endSeconds = line.words[w + 1].startSeconds;
                else
                    word.endSeconds = line.endSeconds;
            }
        }
    }
}

void LyricsTrack::applyTimeOffset (const double offsetSeconds)
{
    if (std::abs (offsetSeconds) < 1.0e-9)
        return;

    for (auto& line : lines)
    {
        line.startSeconds = juce::jmax (0.0, line.startSeconds + offsetSeconds);
        line.endSeconds = juce::jmax (line.startSeconds, line.endSeconds + offsetSeconds);

        for (auto& word : line.words)
        {
            word.startSeconds = juce::jmax (0.0, word.startSeconds + offsetSeconds);
            word.endSeconds = juce::jmax (word.startSeconds, word.endSeconds + offsetSeconds);
        }
    }
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

    // Before the first lyric line - nothing active yet.
    if (seconds + 1.0e-6 < lines.front().startSeconds)
        return -1;

    auto active = -1;

    for (int i = 0; i < static_cast<int> (lines.size()); ++i)
    {
        if (lines[static_cast<size_t> (i)].startSeconds <= seconds + 1.0e-6)
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

    track.sanitizeAll();

    if (! track.isEmpty())
        track.finalizeTiming();

    return ! track.isEmpty();
}

} // namespace jamstudio::notation