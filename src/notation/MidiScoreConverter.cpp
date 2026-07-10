#include "MidiScoreConverter.h"

#include <array>
#include <cmath>
#include <limits>
#include <map>

namespace jamstudio::notation
{

namespace
{
constexpr std::array<int, 6> openStringMidi { 40, 45, 50, 55, 59, 64 }; // E2-E4 guitar
constexpr std::array<int, 4> bassOpenStringMidi { 28, 33, 38, 43 };     // E1-G2 bass
constexpr double defaultMeasureLengthBeats = 4.0;

juce::String defaultPartName (const int trackIndex, const int midiPitchHint)
{
    // Rough register guess for friendlier labels when MIDI has no track name.
    if (midiPitchHint > 0 && midiPitchHint < 40)
        return "Bass / Low " + juce::String (trackIndex + 1);

    if (midiPitchHint >= 60)
        return "Melody / High " + juce::String (trackIndex + 1);

    return "Part " + juce::String (trackIndex + 1);
}
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

    // Prefer guitar mapping; fall back to bass strings for low notes.
    auto bestString = 0;
    auto bestFret = 0;
    auto bestScore = std::numeric_limits<int>::max();
    const bool useBass = note.midiPitch < 40;

    const auto tryMap = [&] (const int* opens, const int numStrings, const int stringOffset)
    {
        for (int stringIndex = 0; stringIndex < numStrings; ++stringIndex)
        {
            const auto fret = note.midiPitch - opens[stringIndex];

            if (fret < 0 || fret > 24)
                continue;

            auto scoreValue = std::abs (fret - context.lastFretPosition) * 2;

            if (context.lastFretOnString[static_cast<size_t> (stringIndex)] >= 0)
                scoreValue += std::abs (fret - context.lastFretOnString[static_cast<size_t> (stringIndex)]);

            if (fret > 17)
                scoreValue += 6;

            if (scoreValue < bestScore)
            {
                bestScore = scoreValue;
                bestString = stringIndex + 1 + stringOffset;
                bestFret = fret;
            }
        }
    };

    if (useBass)
        tryMap (bassOpenStringMidi.data(), static_cast<int> (bassOpenStringMidi.size()), 0);
    else
        tryMap (openStringMidi.data(), static_cast<int> (openStringMidi.size()), 0);

    // If bass mapping failed, try guitar frets anyway.
    if (bestString <= 0)
        tryMap (openStringMidi.data(), static_cast<int> (openStringMidi.size()), 0);

    if (bestString > 0)
    {
        // Clamp string into 1..6 for the 6-line tab display.
        note.stringNumber = juce::jlimit (1, 6, bestString);
        note.fret = bestFret;
        note.label = juce::String (bestFret);
        const auto ctxIndex = juce::jlimit (0, 5, note.stringNumber - 1);
        context.lastFretOnString[static_cast<size_t> (ctxIndex)] = bestFret;
        context.lastFretPosition = bestFret;
    }
    else
    {
        // Unfrettable pitch - still show as a pitch label in tab view.
        note.stringNumber = 3;
        note.fret = -1;
        note.label = juce::MidiMessage::getMidiNoteName (note.midiPitch, true, true, 4);
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

    struct TrackNotes
    {
        int trackIndex = 0;
        juce::String name;
        std::vector<ParsedNote> notes;
    };

    std::vector<TrackNotes> tracks;

    for (int trackIndex = 0; trackIndex < midi.getNumTracks(); ++trackIndex)
    {
        const auto* track = midi.getTrack (trackIndex);
        std::map<int, double> activeNotes;
        TrackNotes trackNotes;
        trackNotes.trackIndex = trackIndex;
        trackNotes.name = "Part " + juce::String (trackIndex + 1);
        auto pitchSum = 0;
        auto pitchCount = 0;

        for (int i = 0; i < track->getNumEvents(); ++i)
        {
            const auto* event = track->getEventPointer (i);
            const auto& message = event->message;
            const auto tickPosition = event->message.getTimeStamp();

            if (message.isTrackNameEvent())
            {
                const auto name = message.getTextFromTextMetaEvent().trim();

                if (name.isNotEmpty())
                    trackNotes.name = name;
            }
            else if (message.isTempoMetaEvent())
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
                    trackNotes.notes.push_back (note);
                    pitchSum += pitch;
                    ++pitchCount;
                    activeNotes.erase (startIt);
                }
            }
        }

        if (! trackNotes.notes.empty())
        {
            if (trackNotes.name.startsWith ("Part ") && pitchCount > 0)
                trackNotes.name = defaultPartName (tracks.size(), pitchSum / pitchCount);

            tracks.push_back (std::move (trackNotes));
        }
    }

    if (tracks.empty())
    {
        errorMessage = "MIDI file contains no note events.";
        return false;
    }

    score.clear();
    score.setTitle ("AI Transcription");
    score.setTempo (tempoBpm);
    // Default to tab view, but every part can be switched to sheet by the user.
    score.setNotationMode (NotationMode::tab);

    for (const auto& trackNotes : tracks)
    {
        auto notes = trackNotes.notes;
        std::sort (notes.begin(), notes.end(),
                   [] (const ParsedNote& a, const ParsedNote& b)
                   {
                       return a.startBeat < b.startBeat;
                   });

        const auto totalBeats = notes.back().startBeat + notes.back().durationBeats;
        const auto numMeasures = juce::jmax (1, static_cast<int> (std::ceil (totalBeats / defaultMeasureLengthBeats)));

        std::vector<Measure> measures (static_cast<size_t> (numMeasures));

        for (int i = 0; i < numMeasures; ++i)
        {
            measures[static_cast<size_t> (i)].number = i + 1;
            measures[static_cast<size_t> (i)].lengthBeats = defaultMeasureLengthBeats;
        }

        TabAssignmentContext tabContext;

        for (const auto& parsed : notes)
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

        ScorePart part;
        part.name = trackNotes.name;
        // Offer both modes: default tab for fretted-friendly parts, sheet for high/melodic.
        part.notationMode = NotationMode::tab;

        for (auto& measure : measures)
        {
            std::sort (measure.notes.begin(), measure.notes.end(),
                       [] (const NoteEvent& a, const NoteEvent& b)
                       {
                           return a.startBeat < b.startBeat;
                       });

            part.addMeasure (std::move (measure));
        }

        score.addPart (std::move (part));
    }

    // Prefer a part named like guitar if present; otherwise first part.
    for (int i = 0; i < score.getNumParts(); ++i)
    {
        if (const auto* part = score.getPart (i))
        {
            if (part->name.containsIgnoreCase ("guitar")
                || part->name.containsIgnoreCase ("gtr"))
            {
                score.setActivePartIndex (i);
                break;
            }
        }
    }

    return true;
}

} // namespace jamstudio::notation
