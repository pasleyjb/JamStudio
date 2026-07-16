#pragma once

#include "../audio/TransportController.h"

namespace jamstudio::ui
{

/** Compact per-stem waveform with playback cursor (cached wave + light playhead). */
class StemMiniWaveform : public juce::Component,
                         public juce::ChangeListener,
                         public juce::Timer
{
public:
    StemMiniWaveform (juce::AudioFormatManager& formatManager,
                      juce::AudioThumbnailCache& cache,
                      jamstudio::audio::TransportController& transport,
                      const juce::File& sourceFile);
    ~StemMiniWaveform() override;

    void setSourceFile (const juce::File& file);
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
