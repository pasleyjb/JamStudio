#pragma once

#include "Score.h"

namespace jamstudio::notation
{

struct TabAssignmentContext
{
    std::array<int, 6> lastFretOnString { -1, -1, -1, -1, -1, -1 };
    int lastFretPosition = 5;
};

/** Parses MusicXML into the internal Score model. */
class MusicXmlParser
{
public:
    [[nodiscard]] static bool parseFile (const juce::File& file, Score& score, juce::String& errorMessage);
    [[nodiscard]] static bool parseXml (const juce::String& xmlText, Score& score, juce::String& errorMessage);

private:
    struct ParseContext
    {
        int divisions = 1;
        double measureLengthBeats = 4.0;
        double measureBeatOffset = 0.0;
        double positionInMeasure = 0.0;
        double tempo = 120.0;
        bool hasTabClef = false;
        TabAssignmentContext tabContext;
    };

    [[nodiscard]] static int stepToSemitoneOffset (const juce::String& step);
    [[nodiscard]] static int pitchToMidi (const juce::XmlElement& pitch);
    [[nodiscard]] static double divisionsToBeats (double divisions, int divisionsPerQuarter) noexcept;
    [[nodiscard]] static double getNoteDurationBeats (const juce::XmlElement& note, int divisionsPerQuarter);
    [[nodiscard]] static bool parseAttributes (const juce::XmlElement* attributes, ParseContext& context);
    [[nodiscard]] static bool parseTabTechnicalData (const juce::XmlElement& note, NoteEvent& noteEvent);
    static void parseLyricData (const juce::XmlElement& note, NoteEvent& noteEvent);
    static void assignGuitarTab (NoteEvent& note, TabAssignmentContext& context);
    [[nodiscard]] static bool parseMeasure (const juce::XmlElement& measureElement, Score& score, ParseContext& context);
    [[nodiscard]] static NoteEvent parseNote (const juce::XmlElement& noteElement, ParseContext& context, bool isChord);
};

} // namespace jamstudio::notation