#include "ScoreLyricsExtractor.h"

#include <vector>

namespace jamstudio::notation
{

namespace
{
struct LyricSyllable
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    juce::String text;
    juce::String syllabic;
};

bool syllableEndsWord (const juce::String& syllabic)
{
    return syllabic.equalsIgnoreCase ("end") || syllabic.equalsIgnoreCase ("single");
}
} // namespace

LyricsTrack ScoreLyricsExtractor::fromScore (const Score& score)
{
    LyricsTrack track;
    track.setTitle (score.getTitle());

    std::vector<LyricSyllable> syllables;

    for (int m = 0; m < score.getNumMeasures(); ++m)
    {
        if (const auto* measure = score.getMeasure (m))
        {
            for (const auto& note : measure->notes)
            {
                if (note.lyricText.isEmpty())
                    continue;

                LyricSyllable syllable;
                syllable.startSeconds = score.beatsToSeconds (note.startBeat);
                syllable.endSeconds = score.beatsToSeconds (note.startBeat + note.durationBeats);
                syllable.text = note.lyricText;
                syllable.syllabic = note.syllabic;
                syllables.push_back (std::move (syllable));
            }
        }
    }

    if (syllables.empty())
        return track;

    std::sort (syllables.begin(), syllables.end(),
               [] (const LyricSyllable& a, const LyricSyllable& b)
               {
                   return a.startSeconds < b.startSeconds;
               });

    LyricLine currentLine;
    juce::String currentWord;

    auto flushWord = [&]
    {
        if (currentWord.isNotEmpty())
        {
            if (currentLine.text.isNotEmpty())
                currentLine.text += " ";

            currentLine.text += currentWord;
            currentWord = {};
        }
    };

    auto flushLine = [&track, &currentLine]
    {
        if (currentLine.text.trim().isNotEmpty())
            track.addLine (currentLine);

        currentLine = {};
    };

    for (size_t i = 0; i < syllables.size(); ++i)
    {
        const auto& syllable = syllables[i];

        if (currentLine.text.isEmpty())
            currentLine.startSeconds = syllable.startSeconds;

        currentLine.endSeconds = syllable.endSeconds;
        currentWord += syllable.text;

        const auto gapBreak = i + 1 < syllables.size()
            && (syllables[i + 1].startSeconds - syllable.endSeconds) > 1.5;

        if (syllableEndsWord (syllable.syllabic))
        {
            flushWord();

            if (gapBreak)
                flushLine();
        }
        else if (gapBreak)
        {
            flushWord();
            flushLine();
        }
    }

    flushWord();
    flushLine();

    return track;
}

} // namespace jamstudio::notation