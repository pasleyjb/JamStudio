#include "Score.h"

#include <limits>

namespace jamstudio::notation
{

void ScorePart::clear()
{
    measures.clear();
    totalBeats = 0.0;
    id = {};
    name = {};
    notationMode = NotationMode::standard;
}

void ScorePart::addMeasure (Measure measure)
{
    measure.startBeat = totalBeats;
    totalBeats += measure.lengthBeats;
    measures.push_back (std::move (measure));
}

const Measure* ScorePart::getMeasure (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (measures.size())))
        return nullptr;

    return &measures[static_cast<size_t> (index)];
}

bool ScorePart::hasLyrics() const noexcept
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

void Score::clear()
{
    parts.clear();
    tempoEvents.clear();
    activePartIndex = 0;
    tempoBpm = 120.0;
    title = {};
    notationMode = NotationMode::standard;
}

void Score::setTempo (const double bpm)
{
    tempoBpm = juce::jmax (20.0, bpm);

    if (tempoEvents.empty())
        addTempoEvent (0.0, tempoBpm);
}

void Score::setNotationMode (const NotationMode mode)
{
    notationMode = mode;

    if (mode != NotationMode::hidden && ! parts.empty())
        parts[static_cast<size_t> (activePartIndex)].notationMode = mode;
}

void Score::setActivePartIndex (const int index)
{
    if (juce::isPositiveAndBelow (index, static_cast<int> (parts.size())))
        activePartIndex = index;
}

void Score::addPart (ScorePart part)
{
    if (part.name.isEmpty())
        part.name = "Part " + juce::String (parts.size() + 1);

    parts.push_back (std::move (part));
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

bool Score::isEmpty() const noexcept
{
    return parts.empty() || getActivePartInternal().isEmpty();
}

NotationMode Score::getNotationMode() const noexcept
{
    if (notationMode == NotationMode::hidden)
        return NotationMode::hidden;

    if (! parts.empty())
        return parts[static_cast<size_t> (activePartIndex)].notationMode;

    return notationMode;
}

int Score::getNumMeasures() const noexcept
{
    return getActivePartInternal().getNumMeasures();
}

const Measure* Score::getMeasure (const int index) const noexcept
{
    return getActivePartInternal().getMeasure (index);
}

double Score::getTotalBeats() const noexcept
{
    return getActivePartInternal().totalBeats;
}

const ScorePart* Score::getPart (const int index) const noexcept
{
    if (! juce::isPositiveAndBelow (index, static_cast<int> (parts.size())))
        return nullptr;

    return &parts[static_cast<size_t> (index)];
}

const ScorePart& Score::getActivePart() const noexcept
{
    return getActivePartInternal();
}

const ScorePart& Score::getActivePartInternal() const noexcept
{
    static const ScorePart emptyPart;

    if (parts.empty())
        return emptyPart;

    return parts[static_cast<size_t> (juce::jlimit (0, static_cast<int> (parts.size()) - 1, activePartIndex))];
}

juce::StringArray Score::getPartNames() const
{
    juce::StringArray names;

    for (const auto& part : parts)
        names.add (part.name);

    return names;
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
    return getActivePartInternal().hasLyrics();
}

const NoteEvent* Score::getActiveLyricNoteAtTime (const double seconds) const noexcept
{
    const auto beat = secondsToBeats (seconds);
    const NoteEvent* active = nullptr;

    for (const auto& measure : getActivePartInternal().measures)
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
    const auto& activePart = getActivePartInternal();

    if (activePart.measures.empty())
        return -1;

    const auto beat = secondsToBeats (seconds);

    for (int i = static_cast<int> (activePart.measures.size()) - 1; i >= 0; --i)
    {
        if (activePart.measures[static_cast<size_t> (i)].startBeat <= beat)
            return i;
    }

    return 0;
}

double Score::getXPositionForBeat (const double beat, const int measureWidth) const noexcept
{
    constexpr int leftMargin = 12;
    auto x = static_cast<double> (leftMargin);

    for (int i = 0; i < getNumMeasures(); ++i)
    {
        if (const auto* measure = getMeasure (i))
        {
            const auto measureEnd = measure->startBeat + measure->lengthBeats;

            if (beat < measureEnd || i == getNumMeasures() - 1)
            {
                const auto length = juce::jmax (0.01, measure->lengthBeats);
                const auto fraction = juce::jlimit (0.0, 1.0, (beat - measure->startBeat) / length);
                return x + fraction * static_cast<double> (measureWidth);
            }

            x += static_cast<double> (measureWidth);
        }
    }

    return x;
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

juce::var partToVar (const ScorePart& part)
{
    auto* obj = new juce::DynamicObject();
    obj->setProperty ("id", part.id);
    obj->setProperty ("name", part.name);
    obj->setProperty ("notationMode", part.notationMode == NotationMode::tab ? "tab" : "standard");

    juce::Array<juce::var> measureArray;

    for (const auto& measure : part.measures)
        measureArray.add (measureToVar (measure));

    obj->setProperty ("measures", measureArray);
    return juce::var (obj);
}

ScorePart varToPart (const juce::var& value)
{
    ScorePart part;

    if (const auto* obj = value.getDynamicObject())
    {
        part.id = obj->getProperty ("id").toString();
        part.name = obj->getProperty ("name").toString();

        const auto mode = obj->getProperty ("notationMode").toString();
        part.notationMode = mode.equalsIgnoreCase ("tab") ? NotationMode::tab : NotationMode::standard;
        part.totalBeats = 0.0;

        if (const auto* measureArray = obj->getProperty ("measures").getArray())
        {
            for (const auto& measureVar : *measureArray)
                part.addMeasure (varToMeasure (measureVar));
        }
    }

    return part;
}
} // namespace

juce::var Score::toVar() const
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("title", title);
    root->setProperty ("tempoBpm", tempoBpm);
    root->setProperty ("divisionsPerQuarter", divisionsPerQuarter);
    root->setProperty ("notationMode", getNotationMode() == NotationMode::tab ? "tab"
                        : (getNotationMode() == NotationMode::hidden ? "hidden" : "standard"));
    root->setProperty ("activePartIndex", activePartIndex);

    juce::Array<juce::var> partArray;

    for (const auto& part : parts)
        partArray.add (partToVar (part));

    root->setProperty ("parts", partArray);

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

    if (mode.equalsIgnoreCase ("tab"))
        score.setNotationMode (NotationMode::tab);
    else if (mode.equalsIgnoreCase ("hidden"))
        score.setNotationMode (NotationMode::hidden);
    else
        score.setNotationMode (NotationMode::standard);

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

    if (const auto* partArray = root->getProperty ("parts").getArray())
    {
        for (const auto& partVar : *partArray)
            score.addPart (varToPart (partVar));
    }
    else if (const auto* measureArray = root->getProperty ("measures").getArray())
    {
        ScorePart legacyPart;
        legacyPart.name = "Part 1";

        for (const auto& measureVar : *measureArray)
            legacyPart.addMeasure (varToMeasure (measureVar));

        score.addPart (std::move (legacyPart));
    }

    score.setActivePartIndex (static_cast<int> (root->getProperty ("activePartIndex")));
    return ! score.parts.empty();
}

} // namespace jamstudio::notation