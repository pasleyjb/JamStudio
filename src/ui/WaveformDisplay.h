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
    ~WaveformDisplay() override;

    void setSourceFile (const juce::File& file);
    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

private:
    void invalidateWaveCache();
    void rebuildWaveCacheIfNeeded();
    [[nodiscard]] juce::Rectangle<int> waveBounds() const;
    [[nodiscard]] int playheadX() const;

    jamstudio::audio::TransportController& transportController;
    juce::AudioThumbnail thumbnail;
    juce::Image waveCache;
    int lastPlayheadX = -1;
    bool waveCacheDirty = true;
};

} // namespace jamstudio::ui
