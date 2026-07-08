#pragma once

#include <JuceHeader.h>

namespace jamstudio::notation
{

enum class NotationMode
{
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

/** Internal score representation with beat-based timing. */
class Score
{
public:
    void clear();

    void setTitle (const juce::String& newTitle) { title = newTitle; }
    void setTempo (double bpm);
    void setDivisionsPerQuarter (int divisions) { divisionsPerQuarter = juce::jmax (1, divisions); }
    void setNotationMode (NotationMode mode) { notationMode = mode; }
    void addMeasure (Measure measure);
    void addTempoEvent (double beatPosition, double bpm);

    [[nodiscard]] bool isEmpty() const noexcept { return measures.empty(); }
    [[nodiscard]] const juce::String& getTitle() const noexcept { return title; }
    [[nodiscard]] double getTempo() const noexcept { return tempoBpm; }
    [[nodiscard]] NotationMode getNotationMode() const noexcept { return notationMode; }
    [[nodiscard]] int getNumMeasures() const noexcept { return static_cast<int> (measures.size()); }
    [[nodiscard]] const Measure* getMeasure (int index) const noexcept;
    [[nodiscard]] double getTotalBeats() const noexcept { return totalBeats; }
    [[nodiscard]] const std::vector<TempoEvent>& getTempoEvents() const noexcept { return tempoEvents; }

    [[nodiscard]] double beatsToSeconds (double beats) const noexcept;
    [[nodiscard]] double secondsToBeats (double seconds) const noexcept;
    [[nodiscard]] int getMeasureIndexAtTime (double seconds) const noexcept;
    [[nodiscard]] double getTempoAtBeat (double beat) const noexcept;

private:
    void sortTempoEvents();

    juce::String title;
    double tempoBpm = 120.0;
    int divisionsPerQuarter = 1;
    NotationMode notationMode = NotationMode::standard;
    std::vector<Measure> measures;
    std::vector<TempoEvent> tempoEvents;
    double totalBeats = 0.0;
};

} // namespace jamstudio::notation