#pragma once

#include "../audio/TransportController.h"

namespace jamstudio::ui
{

/** Compact per-stem waveform with playback cursor. */
class StemMiniWaveform : public juce::Component,
                         public juce::ChangeListener,
                         public juce::Timer
{
public:
    StemMiniWaveform (juce::AudioFormatManager& formatManager,
                      juce::AudioThumbnailCache& cache,
                      jamstudio::audio::TransportController& transport,
                      const juce::File& sourceFile);

    void setSourceFile (const juce::File& file);
    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    jamstudio::audio::TransportController& transportController;
    juce::AudioThumbnail thumbnail;
};

} // namespace jamstudio::ui