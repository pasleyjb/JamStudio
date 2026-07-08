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

} // namespace jamstudio::notation