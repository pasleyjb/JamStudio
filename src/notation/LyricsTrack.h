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

    void setTitle (const juce::String& newTitle) { title = newTitle; }
    void addLine (double startSeconds, const juce::String& text);
    void addLine (LyricLine line);

    [[nodiscard]] bool isEmpty() const noexcept { return lines.empty(); }
    [[nodiscard]] bool hasWordTimings() const noexcept { return wordTimingsAvailable; }
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    [[nodiscard]] int getNumLines() const noexcept { return static_cast<int> (lines.size()); }
    [[nodiscard]] const LyricLine* getLine (int index) const noexcept;
    [[nodiscard]] int getActiveLineIndex (double seconds) const noexcept;
    [[nodiscard]] int getActiveWordIndex (int lineIndex, double seconds) const noexcept;
    [[nodiscard]] double getLineEndSeconds (int index) const noexcept;

private:
    juce::String title;
    std::vector<LyricLine> lines;
    bool wordTimingsAvailable = false;
};

} // namespace jamstudio::notation