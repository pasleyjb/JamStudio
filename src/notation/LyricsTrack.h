#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

struct LyricLine
{
    double startSeconds = 0.0;
    juce::String text;
};

/** Time-synced lyrics independent of MusicXML notation. */
class LyricsTrack
{
public:
    void clear();

    void setTitle (const juce::String& newTitle) { title = newTitle; }
    void addLine (double startSeconds, const juce::String& text);

    [[nodiscard]] bool isEmpty() const noexcept { return lines.empty(); }
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    [[nodiscard]] int getNumLines() const noexcept { return static_cast<int> (lines.size()); }
    [[nodiscard]] const LyricLine* getLine (int index) const noexcept;
    [[nodiscard]] int getActiveLineIndex (double seconds) const noexcept;
    [[nodiscard]] double getLineEndSeconds (int index) const noexcept;

private:
    juce::String title;
    std::vector<LyricLine> lines;
};

} // namespace jamstudio::notation