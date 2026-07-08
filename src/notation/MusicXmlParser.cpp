#include "MusicXmlParser.h"

#include "MusicXmlLoader.h"

#include <array>
#include <limits>
#include <map>

namespace jamstudio::notation
{

namespace
{
constexpr std::array<int, 6> openStringMidi { 40, 45, 50, 55, 59, 64 };

void parseTempoFromMeasure (const juce::XmlElement& measureElement, double& tempo, const double measureStartBeat)
{
    for (auto* child : measureElement.getChildIterator())
    {
        if (! child->hasTagName ("direction"))
            continue;

        if (const auto* directionType = child->getChildByName ("direction-type"))
        {
            if (const auto* metronome = directionType->getChildByName ("metronome"))
            {
                if (const auto* perMinute = metronome->getChildByName ("per-minute"))
                {
                    const auto bpm = perMinute->getAllSubText().getDoubleValue();

                    if (bpm > 0.0)
                        tempo = bpm;
                }
            }
        }

        if (const auto* sound = child->getChildByName ("sound"))
        {
            const auto tempoAttr = sound->getDoubleAttribute ("tempo");

            if (tempoAttr > 0.0)
                tempo = tempoAttr;
        }
    }

    juce::ignoreUnused (measureStartBeat);
}
} // namespace

int MusicXmlParser::stepToSemitoneOffset (const juce::String& step)
{
    const auto upper = step.toUpperCase();

    if (upper == "C") return 0;
    if (upper == "D") return 2;
    if (upper == "E") return 4;
    if (upper == "F") return 5;
    if (upper == "G") return 7;
    if (upper == "A") return 9;
    if (upper == "B") return 11;

    return 0;
}

int MusicXmlParser::pitchToMidi (const juce::XmlElement& pitch)
{
    const auto step = pitch.getChildByName ("step") != nullptr
        ? pitch.getChildByName ("step")->getAllSubText()
        : "C";
    const auto octave = pitch.getChildByName ("octave") != nullptr
        ? pitch.getChildByName ("octave")->getAllSubText().getIntValue()
        : 4;
    const auto alter = pitch.getChildByName ("alter") != nullptr
        ? pitch.getChildByName ("alter")->getAllSubText().getIntValue()
        : 0;

    return (octave + 1) * 12 + stepToSemitoneOffset (step) + alter;
}

double MusicXmlParser::divisionsToBeats (const double divisions, const int divisionsPerQuarter) noexcept
{
    return divisions * (4.0 / static_cast<double> (juce::jmax (1, divisionsPerQuarter)));
}

double MusicXmlParser::getNoteDurationBeats (const juce::XmlElement& note, const int divisionsPerQuarter)
{
    const auto rawDuration = note.getChildByName ("duration") != nullptr
        ? note.getChildByName ("duration")->getAllSubText().getDoubleValue()
        : static_cast<double> (divisionsPerQuarter);

    auto durationBeats = divisionsToBeats (rawDuration, divisionsPerQuarter);

    for (auto* dot : note.getChildWithTagNameIterator ("dot"))
    {
        juce::ignoreUnused (dot);
        durationBeats *= 1.5;
    }

    if (const auto* timeModification = note.getChildByName ("time-modification"))
    {
        const auto actualNotes = timeModification->getChildByName ("actual-notes") != nullptr
            ? timeModification->getChildByName ("actual-notes")->getAllSubText().getIntValue()
            : 1;
        const auto normalNotes = timeModification->getChildByName ("normal-notes") != nullptr
            ? timeModification->getChildByName ("normal-notes")->getAllSubText().getIntValue()
            : 1;

        if (actualNotes > 0 && normalNotes > 0)
            durationBeats *= static_cast<double> (normalNotes) / static_cast<double> (actualNotes);
    }

    return durationBeats;
}

bool MusicXmlParser::parseAttributes (const juce::XmlElement* attributes, ParseContext& context)
{
    if (attributes == nullptr)
        return false;

    if (attributes->getChildByName ("divisions") != nullptr)
        context.divisions = attributes->getChildByName ("divisions")->getAllSubText().getIntValue();

    if (const auto* time = attributes->getChildByName ("time"))
    {
        const auto beats = time->getChildByName ("beats") != nullptr
            ? time->getChildByName ("beats")->getAllSubText().getIntValue()
            : 4;
        const auto beatType = time->getChildByName ("beat-type") != nullptr
            ? time->getChildByName ("beat-type")->getAllSubText().getIntValue()
            : 4;

        context.measureLengthBeats = beats * (4.0 / static_cast<double> (juce::jmax (1, beatType)));
    }

    for (auto* clef : attributes->getChildWithTagNameIterator ("clef"))
    {
        if (clef->getStringAttribute ("sign").equalsIgnoreCase ("TAB"))
            context.hasTabClef = true;
    }

    return true;
}

void MusicXmlParser::parseLyricData (const juce::XmlElement& note, NoteEvent& noteEvent)
{
    for (auto* lyric : note.getChildWithTagNameIterator ("lyric"))
    {
        if (const auto* text = lyric->getChildByName ("text"))
        {
            noteEvent.lyricText = text->getAllSubText().trim();

            if (const auto* syllabic = lyric->getChildByName ("syllabic"))
                noteEvent.syllabic = syllabic->getAllSubText().trim();

            break;
        }
    }
}

bool MusicXmlParser::parseTabTechnicalData (const juce::XmlElement& note, NoteEvent& noteEvent)
{
    auto found = false;

    if (const auto* notations = note.getChildByName ("notations"))
    {
        if (const auto* technical = notations->getChildByName ("technical"))
        {
            if (const auto* stringElement = technical->getChildByName ("string"))
            {
                noteEvent.stringNumber = stringElement->getAllSubText().getIntValue();
                found = true;
            }

            if (const auto* fretElement = technical->getChildByName ("fret"))
            {
                noteEvent.fret = fretElement->getAllSubText().getIntValue();
                noteEvent.label = fretElement->getAllSubText();
                found = true;
            }
        }
    }

    if (const auto* play = note.getChildByName ("play"))
    {
        if (const auto* stringElement = play->getChildByName ("string"))
        {
            noteEvent.stringNumber = stringElement->getAllSubText().getIntValue();
            found = true;
        }

        if (const auto* fretElement = play->getChildByName ("fret"))
        {
            noteEvent.fret = fretElement->getAllSubText().getIntValue();
            noteEvent.label = fretElement->getAllSubText();
            found = true;
        }
    }

    return found;
}

void MusicXmlParser::assignGuitarTab (NoteEvent& note, TabAssignmentContext& context)
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

NoteEvent MusicXmlParser::parseNote (const juce::XmlElement& noteElement,
                                     ParseContext& context,
                                     const bool isChord)
{
    NoteEvent note;
    note.startBeat = context.measureBeatOffset + context.positionInMeasure;
    note.isGrace = noteElement.getChildByName ("grace") != nullptr;
    note.isTuplet = noteElement.getChildByName ("time-modification") != nullptr;

    if (noteElement.getChildByName ("staff") != nullptr)
        note.staffNumber = noteElement.getChildByName ("staff")->getAllSubText().getIntValue();

    if (noteElement.getChildByName ("rest") != nullptr)
    {
        note.isRest = true;
    }
    else if (const auto* pitch = noteElement.getChildByName ("pitch"))
    {
        note.midiPitch = pitchToMidi (*pitch);
        note.label = juce::MidiMessage::getMidiNoteName (note.midiPitch, true, true, 4);
    }
    else if (const auto* unpitched = noteElement.getChildByName ("unpitched"))
    {
        if (const auto* displayStep = unpitched->getChildByName ("display-step"))
        {
            note.label = displayStep->getAllSubText();

            if (const auto* displayOctave = unpitched->getChildByName ("display-octave"))
                note.label += displayOctave->getAllSubText();
        }
    }

    note.durationBeats = getNoteDurationBeats (noteElement, context.divisions);

    parseLyricData (noteElement, note);

    const auto hasTabData = parseTabTechnicalData (noteElement, note);

    if (context.hasTabClef && ! note.isRest && ! hasTabData)
        assignGuitarTab (note, context.tabContext);
    else if (hasTabData && note.stringNumber > 0)
        context.tabContext.lastFretOnString[static_cast<size_t> (note.stringNumber - 1)] = note.fret;

    auto hasTieStart = false;
    auto hasTieStop = false;

    for (auto* tie : noteElement.getChildWithTagNameIterator ("tie"))
    {
        const auto tieType = tie->getStringAttribute ("type");

        if (tieType.equalsIgnoreCase ("start"))
            hasTieStart = true;

        if (tieType.equalsIgnoreCase ("stop"))
            hasTieStop = true;
    }

    const auto shouldAdvancePosition = ! isChord && ! note.isGrace && ! (hasTieStop && ! hasTieStart);

    if (shouldAdvancePosition)
        context.positionInMeasure += note.durationBeats;

    return note;
}

bool MusicXmlParser::parseMeasure (const juce::XmlElement& measureElement,
                                   ScorePart& part,
                                   Score& score,
                                   ParseContext& context)
{
    Measure measure;
    measure.number = measureElement.getIntAttribute ("number", part.getNumMeasures() + 1);
    measure.startBeat = context.measureBeatOffset;
    measure.lengthBeats = context.measureLengthBeats;
    context.positionInMeasure = 0.0;

    parseTempoFromMeasure (measureElement, context.tempo, context.measureBeatOffset);

    if (const auto* attributes = measureElement.getChildByName ("attributes"))
        parseAttributes (attributes, context);

    measure.lengthBeats = context.measureLengthBeats;
    score.setDivisionsPerQuarter (context.divisions);
    part.notationMode = context.hasTabClef ? NotationMode::tab : NotationMode::standard;

    const auto previousTempo = score.getTempoEvents().empty()
        ? context.tempo
        : score.getTempoEvents().back().bpm;

    if (score.getTempoEvents().empty() || std::abs (previousTempo - context.tempo) > 0.01)
        score.addTempoEvent (context.measureBeatOffset, context.tempo);

    score.setTempo (context.tempo);

    for (auto* child : measureElement.getChildIterator())
    {
        if (child->hasTagName ("note"))
        {
            const auto isChord = child->getChildByName ("chord") != nullptr;
            measure.notes.push_back (parseNote (*child, context, isChord));
        }
        else if (child->hasTagName ("backup"))
        {
            const auto backupDuration = child->getChildByName ("duration") != nullptr
                ? child->getChildByName ("duration")->getAllSubText().getDoubleValue()
                : 0.0;

            context.positionInMeasure -= divisionsToBeats (backupDuration, context.divisions);
        }
        else if (child->hasTagName ("forward"))
        {
            const auto forwardDuration = child->getChildByName ("duration") != nullptr
                ? child->getChildByName ("duration")->getAllSubText().getDoubleValue()
                : 0.0;

            context.positionInMeasure += divisionsToBeats (forwardDuration, context.divisions);
        }
    }

    context.measureBeatOffset += measure.lengthBeats;
    part.addMeasure (std::move (measure));
    return true;
}

bool MusicXmlParser::parseFile (const juce::File& file, Score& score, juce::String& errorMessage)
{
    juce::String xmlText;

    if (! MusicXmlLoader::loadScoreXmlFromFile (file, xmlText, errorMessage))
        return false;

    return parseXml (xmlText, score, errorMessage);
}

bool MusicXmlParser::parseXml (const juce::String& xmlText, Score& score, juce::String& errorMessage)
{
    auto xml = juce::XmlDocument::parse (xmlText);

    if (xml == nullptr)
    {
        errorMessage = "Invalid MusicXML document.";
        return false;
    }

    auto* root = xml.get();

    if (root->hasTagName ("score-timewise"))
    {
        errorMessage = "score-timewise MusicXML is not supported yet.";
        return false;
    }

    if (! root->hasTagName ("score-partwise"))
    {
        errorMessage = "Expected a score-partwise MusicXML document.";
        return false;
    }

    score.clear();

    if (const auto* identification = root->getChildByName ("identification"))
    {
        if (const auto* movementTitle = identification->getChildByName ("movement-title"))
            score.setTitle (movementTitle->getAllSubText());
    }

    if (const auto* work = root->getChildByName ("work"))
    {
        if (const auto* workTitle = work->getChildByName ("work-title"))
            score.setTitle (workTitle->getAllSubText());
    }

    std::map<juce::String, juce::String> partNames;

    if (const auto* partList = root->getChildByName ("part-list"))
    {
        for (auto* scorePart : partList->getChildIterator())
        {
            if (! scorePart->hasTagName ("score-part"))
                continue;

            const auto partId = scorePart->getStringAttribute ("id");

            if (const auto* partName = scorePart->getChildByName ("part-name"))
                partNames[partId] = partName->getAllSubText().trim();
        }
    }

    auto foundPart = false;

    for (auto* partElement : root->getChildIterator())
    {
        if (! partElement->hasTagName ("part"))
            continue;

        ScorePart scorePart;
        scorePart.id = partElement->getStringAttribute ("id");
        scorePart.name = partNames.count (scorePart.id) > 0
            ? partNames[scorePart.id]
            : (scorePart.id.isNotEmpty() ? scorePart.id : "Part");

        ParseContext context;

        for (auto* measureElement : partElement->getChildIterator())
        {
            if (measureElement->hasTagName ("measure"))
                parseMeasure (*measureElement, scorePart, score, context);
        }

        if (! scorePart.isEmpty())
        {
            score.addPart (std::move (scorePart));
            foundPart = true;
        }
    }

    if (! foundPart)
    {
        errorMessage = "No part data found in MusicXML.";
        return false;
    }

    if (score.isEmpty())
    {
        errorMessage = "No measures were found in the MusicXML file.";
        return false;
    }

    if (const auto* firstPart = score.getPart (0))
        score.setNotationMode (firstPart->notationMode);

    return true;
}

} // namespace jamstudio::notation