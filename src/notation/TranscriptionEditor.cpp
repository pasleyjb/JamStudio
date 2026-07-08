#include "TranscriptionEditor.h"

#include <array>

namespace jamstudio::notation
{

namespace
{
constexpr std::array<int, 6> openStringMidi { 40, 45, 50, 55, 59, 64 };

int midiPitchFromTab (const int stringNumber, const int fret)
{
    if (stringNumber < 1 || stringNumber > 6 || fret < 0 || fret > 24)
        return -1;

    return openStringMidi[static_cast<size_t> (stringNumber - 1)] + fret;
}

bool parseTabToken (const juce::String& token, int& fret, int& stringNumber)
{
    const auto slash = token.indexOfChar ('/');

    if (slash <= 0)
        return false;

    fret = token.substring (0, slash).getIntValue();
    stringNumber = token.substring (slash + 1).getIntValue();
    return stringNumber >= 1 && stringNumber <= 6 && fret >= 0 && fret <= 24;
}
} // namespace

void TranscriptionEditor::applyLyricLineTexts (LyricsTrack& track,
                                               const juce::String& title,
                                               const juce::StringArray& lineTexts)
{
    LyricsTrack updated;
    updated.setTitle (title);

    for (int i = 0; i < lineTexts.size() && i < track.getNumLines(); ++i)
    {
        const auto newText = lineTexts[i].trim();

        if (newText.isEmpty())
            continue;

        auto line = *track.getLine (i);
        line.text = newText;

        if (! line.words.empty())
        {
            juce::StringArray tokens;
            tokens.addTokens (newText, " ", {});

            if (tokens.size() == static_cast<int> (line.words.size()))
            {
                for (int w = 0; w < tokens.size(); ++w)
                    line.words[static_cast<size_t> (w)].text = tokens[w];
            }
            else
            {
                line.words.clear();
            }
        }

        updated.addLine (std::move (line));
    }

    track = std::move (updated);
}

juce::String TranscriptionEditor::scoreToEditableTabText (const Score& score)
{
    juce::String out;
    out << "# title: " << score.getTitle() << "\n";
    out << "# tempo: " << juce::String (score.getTempo(), 1) << "\n";

    for (int m = 0; m < score.getNumMeasures(); ++m)
    {
        if (const auto* measure = score.getMeasure (m))
        {
            out << "M" << measure->number << ":";

            for (const auto& note : measure->notes)
            {
                if (note.isRest)
                    continue;

                if (note.stringNumber > 0 && note.fret >= 0)
                    out << " " << note.fret << "/" << note.stringNumber;
            }

            out << "\n";
        }
    }

    return out;
}

bool TranscriptionEditor::scoreFromEditableTabText (const juce::String& text,
                                                    const Score& original,
                                                    Score& out,
                                                    juce::String& errorMessage)
{
    auto title = original.getTitle();
    auto tempo = original.getTempo();
    std::vector<std::vector<std::pair<int, int>>> measureTokens;

    const auto lines = juce::StringArray::fromLines (text);

    for (const auto& line : lines)
    {
        const auto trimmed = line.trim();

        if (trimmed.isEmpty() || trimmed.startsWithChar ('#'))
        {
            if (trimmed.startsWithIgnoreCase ("# title:"))
                title = trimmed.fromFirstOccurrenceOf (":", false, false).trim();
            else if (trimmed.startsWithIgnoreCase ("# tempo:"))
                tempo = trimmed.fromFirstOccurrenceOf (":", false, false).trim().getDoubleValue();

            continue;
        }

        if (! trimmed.startsWithIgnoreCase ("M"))
        {
            errorMessage = "Expected measure line starting with M, got: " + trimmed;
            return false;
        }

        const auto colon = trimmed.indexOfChar (':');

        if (colon < 0)
        {
            errorMessage = "Invalid measure line: " + trimmed;
            return false;
        }

        std::vector<std::pair<int, int>> tokens;
        juce::StringArray parts;
        parts.addTokens (trimmed.substring (colon + 1).trim(), " ", {});

        for (const auto& part : parts)
        {
            int fret = 0;
            int stringNumber = 0;

            if (! parseTabToken (part, fret, stringNumber))
            {
                errorMessage = "Invalid tab token '" + part + "'. Use fret/string (e.g. 3/2).";
                return false;
            }

            tokens.emplace_back (fret, stringNumber);
        }

        measureTokens.push_back (std::move (tokens));
    }

    if (measureTokens.empty())
    {
        errorMessage = "No measure lines found. Use M1: 3/2 5/1 format.";
        return false;
    }

    out.clear();
    out.setTitle (title);
    out.setTempo (tempo);
    out.setNotationMode (NotationMode::tab);

    ScorePart tabPart;
    tabPart.name = original.getActivePart().name.isNotEmpty() ? original.getActivePart().name : "Guitar";
    tabPart.notationMode = NotationMode::tab;

    for (size_t m = 0; m < measureTokens.size(); ++m)
    {
        Measure measure;
        measure.number = static_cast<int> (m) + 1;
        measure.lengthBeats = 4.0;

        const Measure* originalMeasure = nullptr;

        if (juce::isPositiveAndBelow (static_cast<int> (m), original.getNumMeasures()))
            originalMeasure = original.getMeasure (static_cast<int> (m));

        const auto& tokens = measureTokens[m];

        for (size_t n = 0; n < tokens.size(); ++n)
        {
            const auto [fret, stringNumber] = tokens[n];
            NoteEvent note;
            note.fret = fret;
            note.stringNumber = stringNumber;
            note.midiPitch = midiPitchFromTab (stringNumber, fret);
            note.label = juce::String (fret);
            note.isRest = false;

            if (originalMeasure != nullptr
                && juce::isPositiveAndBelow (static_cast<int> (n), static_cast<int> (originalMeasure->notes.size())))
            {
                const auto& originalNote = originalMeasure->notes[static_cast<size_t> (n)];
                note.startBeat = originalNote.startBeat;
                note.durationBeats = originalNote.durationBeats;
            }
            else
            {
                note.startBeat = measure.startBeat + static_cast<double> (n) * 0.5;
                note.durationBeats = 0.25;
            }

            measure.notes.push_back (std::move (note));
        }

        tabPart.addMeasure (std::move (measure));
    }

    out.addPart (std::move (tabPart));

    errorMessage = {};
    return true;
}

} // namespace jamstudio::notation