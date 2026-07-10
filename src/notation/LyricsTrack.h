#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

struct LyricWord
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    juce::String text;
};

struct LyricLine
{
    double startSeconds = 0.0;
    double endSeconds = 0.0;
    juce::String text;
    std::vector<LyricWord> words;
};

/** Time-synced lyrics independent of MusicXML notation. */
class LyricsTrack
{
public:
    void clear();

    void setTitle (const juce::String& newTitle);
    void addLine (double startSeconds, const juce::String& text);
    void addLine (LyricLine line);

    /** Sort by start time and fill missing end times from the next line. */
    void finalizeTiming();

    /** Shift all timestamps (positive = later). Useful when web LRC is slightly off. */
    void applyTimeOffset (double offsetSeconds);

    /**
     * Strip fancy punctuation / mojibake so JUCE fonts don't show odd glyphs
     * (e.g. em-dash as "a with a bar"). Safe for online LRC, Whisper, and project load.
     */
    [[nodiscard]] static juce::String sanitizeDisplayText (const juce::String& input);

    /** Re-sanitize title + all lines/words (e.g. after loading old projects). */
    void sanitizeAll();

    [[nodiscard]] bool isEmpty() const noexcept { return lines.empty(); }
    [[nodiscard]] bool hasWordTimings() const noexcept { return wordTimingsAvailable; }
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    [[nodiscard]] int getNumLines() const noexcept { return static_cast<int> (lines.size()); }
    [[nodiscard]] const LyricLine* getLine (int index) const noexcept;
    [[nodiscard]] int getActiveLineIndex (double seconds) const noexcept;
    [[nodiscard]] int getActiveWordIndex (int lineIndex, double seconds) const noexcept;
    [[nodiscard]] double getLineEndSeconds (int index) const noexcept;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static bool fromVar (const juce::var& data, LyricsTrack& track);

private:
    juce::String title;
    std::vector<LyricLine> lines;
    bool wordTimingsAvailable = false;
};

} // namespace jamstudio::notation