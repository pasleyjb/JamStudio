#pragma once

#include "MusicXmlParser.h"
#include "Score.h"

namespace jamstudio::notation
{

/** Converts a MIDI file into the internal Score model with guitar tab positions. */
class MidiScoreConverter
{
public:
    [[nodiscard]] static bool convertFile (const juce::File& midiFile,
                                           Score& score,
                                           juce::String& errorMessage);

private:
    struct ParsedNote
    {
        int midiPitch = -1;
        double startBeat = 0.0;
        double durationBeats = 0.25;
    };

    static void assignGuitarTab (NoteEvent& note, TabAssignmentContext& context);
    [[nodiscard]] static double ticksToBeats (double ticks, int ticksPerQuarter) noexcept;
};

} // namespace jamstudio::notation