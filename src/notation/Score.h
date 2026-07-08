#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

enum class NotationMode
{
    hidden,
    standard,
    tab
};

struct NoteEvent
{
    int midiPitch = -1;
    int stringNumber = 0;
    int fret = -1;
    int staffNumber = 1;
    double startBeat = 0.0;
    double durationBeats = 1.0;
    bool isRest = false;
    bool isGrace = false;
    bool isTuplet = false;
    juce::String label;
    juce::String lyricText;
    juce::String syllabic;
};

struct Measure
{
    int number = 1;
    double startBeat = 0.0;
    double lengthBeats = 4.0;
    std::vector<NoteEvent> notes;
};

struct TempoEvent
{
    double beatPosition = 0.0;
    double bpm = 120.0;
};

/** One instrument or staff line inside a score. */
struct ScorePart
{
    juce::String id;
    juce::String name;
    NotationMode notationMode = NotationMode::standard;
    std::vector<Measure> measures;
    double totalBeats = 0.0;

    void clear();
    void addMeasure (Measure measure);
    [[nodiscard]] const Measure* getMeasure (int index) const noexcept;
    [[nodiscard]] int getNumMeasures() const noexcept { return static_cast<int> (measures.size()); }
    [[nodiscard]] bool isEmpty() const noexcept { return measures.empty(); }
    [[nodiscard]] bool hasLyrics() const noexcept;
};

/** Internal score representation with beat-based timing. */
class Score
{
public:
    void clear();

    void setTitle (const juce::String& newTitle) { title = newTitle; }
    void setTempo (double bpm);
    void setDivisionsPerQuarter (int divisions) { divisionsPerQuarter = juce::jmax (1, divisions); }
    void setNotationMode (NotationMode mode);
    void setActivePartIndex (int index);
    void addPart (ScorePart part);
    void addTempoEvent (double beatPosition, double bpm);

    [[nodiscard]] bool isEmpty() const noexcept;
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    [[nodiscard]] double getTempo() const noexcept { return tempoBpm; }
    [[nodiscard]] NotationMode getNotationMode() const noexcept;
    [[nodiscard]] bool isNotationVisible() const noexcept { return notationMode != NotationMode::hidden; }
    [[nodiscard]] int getNumMeasures() const noexcept;
    [[nodiscard]] const Measure* getMeasure (int index) const noexcept;
    [[nodiscard]] double getTotalBeats() const noexcept;
    [[nodiscard]] const std::vector<TempoEvent>& getTempoEvents() const noexcept { return tempoEvents; }
    [[nodiscard]] int getNumParts() const noexcept { return static_cast<int> (parts.size()); }
    [[nodiscard]] int getActivePartIndex() const noexcept { return activePartIndex; }
    [[nodiscard]] const ScorePart* getPart (int index) const noexcept;
    [[nodiscard]] const ScorePart& getActivePart() const noexcept;
    [[nodiscard]] juce::StringArray getPartNames() const;

    [[nodiscard]] double beatsToSeconds (double beats) const noexcept;
    [[nodiscard]] double secondsToBeats (double seconds) const noexcept;
    [[nodiscard]] int getMeasureIndexAtTime (double seconds) const noexcept;
    [[nodiscard]] double getTempoAtBeat (double beat) const noexcept;
    [[nodiscard]] bool hasLyrics() const noexcept;
    [[nodiscard]] const NoteEvent* getActiveLyricNoteAtTime (double seconds) const noexcept;
    [[nodiscard]] double getXPositionForBeat (double beat, int measureWidth) const noexcept;

    [[nodiscard]] juce::var toVar() const;
    [[nodiscard]] static bool fromVar (const juce::var& data, Score& score);

private:
    void sortTempoEvents();
    [[nodiscard]] const ScorePart& getActivePartInternal() const noexcept;

    juce::String title;
    double tempoBpm = 120.0;
    int divisionsPerQuarter = 1;
    NotationMode notationMode = NotationMode::standard;
    std::vector<ScorePart> parts;
    int activePartIndex = 0;
    std::vector<TempoEvent> tempoEvents;
};

} // namespace jamstudio::notation