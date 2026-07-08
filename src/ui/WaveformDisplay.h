#pragma once

#include "../audio/TransportController.h"

namespace jamstudio::ui
{

/** Displays a waveform with a playback position cursor. */
class WaveformDisplay : public juce::Component,
                        public juce::ChangeListener,
                        public juce::Timer
{
public:
    WaveformDisplay (juce::AudioFormatManager& formatManager,
                     juce::AudioThumbnailCache& cache,
                     jamstudio::audio::TransportController& transport);

    void setSourceFile (const juce::File& file);
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    jamstudio::audio::TransportController& transportController;
    juce::AudioThumbnail thumbnail;
    juce::Rectangle<int> cursorArea;
};

} // namespace jamstudio::ui