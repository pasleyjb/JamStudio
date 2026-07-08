#include "MidiScoreConverter.h"

#include <array>
#include <limits>
#include <map>

namespace jamstudio::notation
{

namespace
{
constexpr std::array<int, 6> openStringMidi { 40, 45, 50, 55, 59, 64 };
constexpr double defaultMeasureLengthBeats = 4.0;
} // namespace

double MidiScoreConverter::ticksToBeats (const double ticks, const int ticksPerQuarter) noexcept
{
    if (ticksPerQuarter <= 0)
        return 0.0;

    return ticks / static_cast<double> (ticksPerQuarter);
}

void MidiScoreConverter::assignGuitarTab (NoteEvent& note, TabAssignmentContext& context)
{
    if (note.isRest || note.midiPitch < 0)
        return;

    auto bestString = 0;
    auto bestFret = 0;
    auto bestScore = std::numeric_limits<int>::max();

    for (int stringIndex = 0; stringIndex < static_cast<int> (openStringMidi.size()); ++stringIndex)
    {
        const auto fret = note.midiPitch - openStringMidi[static_cast<size_t> (stringIndex)];

        if (fret < 0 || fret > 24)
            continue;

        auto score = std::abs (fret - context.lastFretPosition) * 2;

        if (context.lastFretOnString[static_cast<size_t> (stringIndex)] >= 0)
            score += std::abs (fret - context.lastFretOnString[static_cast<size_t> (stringIndex)]);

        if (fret > 17)
            score += 6;

        if (score < bestScore)
        {
            bestScore = score;
            bestString = stringIndex + 1;
            bestFret = fret;
        }
    }

    if (bestString > 0)
    {
        note.stringNumber = bestString;
        note.fret = bestFret;
        note.label = juce::String (bestFret);
        context.lastFretOnString[static_cast<size_t> (bestString - 1)] = bestFret;
        context.lastFretPosition = bestFret;
    }
}

bool MidiScoreConverter::convertFile (const juce::File& midiFile,
                                      Score& score,
                                      juce::String& errorMessage)
{
    if (! midiFile.existsAsFile())
    {
        errorMessage = "MIDI file does not exist.";
        return false;
    }

    juce::FileInputStream inputStream (midiFile);

    if (! inputStream.openedOk())
    {
        errorMessage = "Could not open MIDI file.";
        return false;
    }

    juce::MidiFile midi;
    midi.readFrom (inputStream);

    if (midi.getNumTracks() == 0)
    {
        errorMessage = "MIDI file contains no tracks.";
        return false;
    }

    const auto ticksPerQuarter = midi.getTimeFormat() > 0 ? midi.getTimeFormat() : 480;
    auto tempoBpm = 120.0;
    std::vector<ParsedNote> parsedNotes;

    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* track = midi.getTrack (trackIndex);
        std::map<int, double> activeNotes;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto* event = track->getEventPointer (i);
            const auto& message = event->message;
            const auto tickPosition = event->message.getTimeStamp();

            if (message.isTempoMetaEvent())
            {
                const auto secondsPerQuarter = message.getTempoSecondsPerQuarterNote();
                tempoBpm = secondsPerQuarter > 0.0 ? 60.0 / secondsPerQuarter : tempoBpm;
            }
            else if (message.isNoteOn() && message.getVelocity() > 0)
            {
                activeNotes[message.getNoteNumber()] = tickPosition;
            }
            else if (message.isNoteOff()
                     || (message.isNoteOn() && message.getVelocity() == 0))
            {
                const auto pitch = message.getNoteNumber();
                const auto startIt = activeNotes.find (pitch);

                if (startIt != activeNotes.end())
                {
                    ParsedNote note;
                    note.midiPitch = pitch;
                    note.startBeat = ticksToBeats (startIt->second, ticksPerQuarter);
                    const auto durationTicks = tickPosition - startIt->second;
                    note.durationBeats = juce::jmax (0.125, ticksToBeats (durationTicks, ticksPerQuarter));
                    parsedNotes.push_back (note);
                    activeNotes.erase (startIt);
                }
            }
        }
    }

    if (parsedNotes.empty())
    {
        errorMessage = "MIDI file contains no note events.";
        return false;
    }

    std::sort (parsedNotes.begin(), parsedNotes.end(),
               [] (const ParsedNote& a, const ParsedNote& b)
               {
                   return a.startBeat < b.startBeat;
               });

    score.clear();
    score.setTitle ("AI Transcription");
    score.setTempo (tempoBpm);
    score.setNotationMode (NotationMode::tab);

    TabAssignmentContext tabContext;
    const auto totalBeats = parsedNotes.back().startBeat + parsedNotes.back().durationBeats;
    const auto numMeasures = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / defaultMeasureLengthBeats)));

    std::vector<Measure> measures (static_cast<size_t> (numMeasures));

    for (int i = 0; i < numMeasures; ++i)
    {
        measures[static_cast<size_t> (i)].number = i + 1;
        measures[static_cast<size_t> (i)].lengthBeats = defaultMeasureLengthBeats;
    }

    for (const auto& parsed : parsedNotes)
    {
        const auto measureIndex = juce::jlimit (0, numMeasures - 1,
                                                static_cast<int> (parsed.startBeat / defaultMeasureLengthBeats));

        NoteEvent note;
        note.midiPitch = parsed.midiPitch;
        note.startBeat = parsed.startBeat;
        note.durationBeats = parsed.durationBeats;
        note.label = juce::MidiMessage::getMidiNoteName (note.midiPitch, true, true, 4);
        assignGuitarTab (note, tabContext);

        measures[static_cast<size_t> (measureIndex)].notes.push_back (std::move (note));
    }

    for (auto& measure : measures)
    {
        std::sort (measure.notes.begin(), measure.notes.end(),
                   [] (const NoteEvent& a, const NoteEvent& b)
                   {
                       return a.startBeat < b.startBeat;
                   });

        score.addMeasure (std::move (measure));
    }

    return true;
}

} // namespace jamstudio::notation