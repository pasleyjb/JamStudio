#include "Score.h"

#include <limits>

namespace jamstudio::notation
{

void Score::clear()
{
    measures.clear();
    tempoEvents.clear();
    totalBeats = 0.0;
    tempoBpm = 120.0;
    title = {};
}

void Score::setTempo (const double bpm)
{
    tempoBpm = juce::jmax (20.0, bpm);

    if (tempoEvents.empty())
        addTempoEvent (0.0, tempoBpm);
}

void Score::addMeasure (Measure measure)
{
    measure.startBeat = totalBeats;
    totalBeats += measure.lengthBeats;
    measures.push_back (std::move (measure));
}

void Score::addTempoEvent (const double beatPosition, const double bpm)
{
    if (bpm <= 0.0)
        return;

    tempoEvents.push_back ({ beatPosition, bpm });
    sortTempoEvents();

    if (beatPosition <= 0.0)
        tempoBpm = bpm;
}

void Score::sortTempoEvents()
{
    std::sort (tempoEvents.begin(), tempoEvents.end(),
               [] (const TempoEvent& a, const TempoEvent& b)
               {
                   return a.beatPosition < b.beatPosition;
               });
}

const Measure* Score::getMeasure (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (measures.size())))
        return nullptr;

    return &measures[static_cast<size_t> (index)];
}

double Score::getTempoAtBeat (const double beat) const noexcept
{
    if (tempoEvents.empty())
        return tempoBpm;

    auto tempo = tempoBpm;

    for (const auto& event : tempoEvents)
    {
        if (event.beatPosition <= beat)
            tempo = event.bpm;
        else
            break;
    }

    return tempo;
}

double Score::beatsToSeconds (const double beats) const noexcept
{
    if (tempoEvents.empty())
        return beats * 60.0 / tempoBpm;

    auto seconds = 0.0;
    auto previousBeat = 0.0;
    auto currentTempo = tempoEvents.front().bpm;

    for (const auto& event : tempoEvents)
    {
        if (event.beatPosition >= beats)
            break;

        seconds += (event.beatPosition - previousBeat) * 60.0 / currentTempo;
        previousBeat = event.beatPosition;
        currentTempo = event.bpm;
    }

    seconds += (beats - previousBeat) * 60.0 / currentTempo;
    return seconds;
}

double Score::secondsToBeats (const double seconds) const noexcept
{
    if (seconds <= 0.0)
        return 0.0;

    if (tempoEvents.empty())
        return seconds * tempoBpm / 60.0;

    auto remainingSeconds = seconds;
    auto beat = 0.0;
    auto currentTempo = tempoEvents.front().bpm;

    for (size_t i = 0; i < tempoEvents.size(); ++i)
    {
        const auto segmentStart = tempoEvents[i].beatPosition;
        const auto segmentEnd = (i + 1 < tempoEvents.size())
            ? tempoEvents[i + 1].beatPosition
            : std::numeric_limits<double>::max();
        const auto secondsPerBeat = 60.0 / currentTempo;
        const auto segmentBeats = segmentEnd - segmentStart;
        const auto segmentSeconds = segmentBeats * secondsPerBeat;

        if (remainingSeconds <= segmentSeconds || i + 1 >= tempoEvents.size())
        {
            beat = segmentStart + remainingSeconds / secondsPerBeat;
            return beat;
        }

        remainingSeconds -= segmentSeconds;
        beat = segmentEnd;
        currentTempo = tempoEvents[i + 1].bpm;
    }

    return beat;
}

bool Score::hasLyrics() const noexcept
{
    for (const auto& measure : measures)
    {
        for (const auto& note : measure.notes)
        {
            if (note.lyricText.isNotEmpty())
                return true;
        }
    }

    return false;
}

const NoteEvent* Score::getActiveLyricNoteAtTime (const double seconds) const noexcept
{
    const auto beat = secondsToBeats (seconds);
    const NoteEvent* active = nullptr;

    for (const auto& measure : measures)
    {
        for (const auto& note : measure.notes)
        {
            if (note.lyricText.isEmpty())
                continue;

            const auto endBeat = note.startBeat + note.durationBeats;

            if (beat >= note.startBeat && beat < endBeat)
                return &note;

            if (note.startBeat <= beat)
                active = &note;
        }
    }

    return active;
}

int Score::getMeasureIndexAtTime (const double seconds) const noexcept
{
    if (measures.empty())
        return -1;

    const auto beat = secondsToBeats (seconds);

    for (int i = static_cast<int> (measures.size()) - 1; i >= 0; --i)
    {
        if (measures[static_cast<size_t> (i)].startBeat <= beat)
            return i;
    }

    return 0;
}

namespace
{
juce::var noteToVar (const NoteEvent& note)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("midiPitch", note.midiPitch);
    obj->setProperty ("stringNumber", note.stringNumber);
    obj->setProperty ("fret", note.fret);
    obj->setProperty ("staffNumber", note.staffNumber);
    obj->setProperty ("startBeat", note.startBeat);
    obj->setProperty ("durationBeats", note.durationBeats);
    obj->setProperty ("isRest", note.isRest);
    obj->setProperty ("isGrace", note.isGrace);
    obj->setProperty ("isTuplet", note.isTuplet);
    obj->setProperty ("label", note.label);
    obj->setProperty ("lyricText", note.lyricText);
    obj->setProperty ("syllabic", note.syllabic);
    return juce::var (obj);
}

NoteEvent varToNote (const juce::var& value)
{
    NoteEvent note;

    if (const auto* obj = value.getDynamicObject())
    {
        note.midiPitch = static_cast<int> (obj->getProperty ("midiPitch"));
        note.stringNumber = static_cast<int> (obj->getProperty ("stringNumber"));
        note.fret = static_cast<int> (obj->getProperty ("fret"));
        note.staffNumber = static_cast<int> (obj->getProperty ("staffNumber"));
        note.startBeat = obj->getProperty ("startBeat");
        note.durationBeats = obj->getProperty ("durationBeats");
        note.isRest = static_cast<bool> (obj->getProperty ("isRest"));
        note.isGrace = static_cast<bool> (obj->getProperty ("isGrace"));
        note.isTuplet = static_cast<bool> (obj->getProperty ("isTuplet"));
        note.label = obj->getProperty ("label").toString();
        note.lyricText = obj->getProperty ("lyricText").toString();
        note.syllabic = obj->getProperty ("syllabic").toString();
    }

    return note;
}

juce::var measureToVar (const Measure& measure)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("number", measure.number);
    obj->setProperty ("startBeat", measure.startBeat);
    obj->setProperty ("lengthBeats", measure.lengthBeats);

    juce::Array<juce::var> notes;

    for (const auto& note : measure.notes)
        notes.add (noteToVar (note));

    obj->setProperty ("notes", notes);
    return juce::var (obj);
}

Measure varToMeasure (const juce::var& value)
{
    Measure measure;

    if (const auto* obj = value.getDynamicObject())
    {
        measure.number = static_cast<int> (obj->getProperty ("number"));
        measure.startBeat = obj->getProperty ("startBeat");
        measure.lengthBeats = obj->getProperty ("lengthBeats");

        if (const auto* notes = obj->getProperty ("notes").getArray())
        {
            for (const auto& noteVar : *notes)
                measure.notes.push_back (varToNote (noteVar));
        }
    }

    return measure;
}
} // namespace

juce::var Score::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("title", title);
    root->setProperty ("tempoBpm", tempoBpm);
    root->setProperty ("divisionsPerQuarter", divisionsPerQuarter);
    root->setProperty ("notationMode", notationMode == NotationMode::tab ? "tab" : "standard");

    juce::Array<juce::var> measureArray;

    for (const auto& measure : measures)
        measureArray.add (measureToVar (measure));

    root->setProperty ("measures", measureArray);

    juce::Array<juce::var> tempoArray;

    for (const auto& event : tempoEvents)
    {
        auto* tempoObj = new juce::DynamicObject();
        tempoObj->setProperty ("beat", event.beatPosition);
        tempoObj->setProperty ("bpm", event.bpm);
        tempoArray.add (juce::var (tempoObj));
    }

    root->setProperty ("tempoEvents", tempoArray);
    return juce::var (root);
}

bool Score::fromVar (const juce::var& data, Score& score)
{
    const auto* root = data.getDynamicObject();

    if (root == nullptr)
        return false;

    score.clear();
    score.setTitle (root->getProperty ("title").toString());
    score.setDivisionsPerQuarter (static_cast<int> (root->getProperty ("divisionsPerQuarter")));
    score.setTempo (root->getProperty ("tempoBpm"));

    const auto mode = root->getProperty ("notationMode").toString();
    score.setNotationMode (mode.equalsIgnoreCase ("tab") ? NotationMode::tab : NotationMode::standard);

    if (const auto* tempoArray = root->getProperty ("tempoEvents").getArray())
    {
        for (const auto& tempoVar : *tempoArray)
        {
            if (const auto* tempoObj = tempoVar.getDynamicObject())
            {
                score.addTempoEvent (tempoObj->getProperty ("beat"),
                                     tempoObj->getProperty ("bpm"));
            }
        }
    }

    if (const auto* measureArray = root->getProperty ("measures").getArray())
    {
        for (const auto& measureVar : *measureArray)
            score.addMeasure (varToMeasure (measureVar));
    }

    return ! score.isEmpty();
}

} // namespace jamstudio::notation