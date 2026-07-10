#pragma once

#include "FFmpegAudioFormat.h"

#include <JuceHeader.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace jamstudio::audio
{

/**
 * Independent stage / video-board media player mixed into the master graph.
 * Plays video/audio via FFmpeg, or image slideshows timed by a silent clock.
 * Transport is separate from the song stem transport.
 */
class StageMediaPlayer : public juce::AudioSource,
                         public juce::ChangeBroadcaster
{
public:
    explicit StageMediaPlayer (juce::AudioFormatManager& formatManager);

    [[nodiscard]] bool loadFile (const juce::File& file, juce::String& errorMessage);
    /** Image slideshow (jpg/png/…). Duration = images.size() * secondsPerSlide. */
    [[nodiscard]] bool loadSlideshow (const juce::Array<juce::File>& images,
                                      double secondsPerSlide,
                                      juce::String& errorMessage);
    void clear();

    void play();
    void pause();
    void stop();
    void togglePlayPause();

    void setVolume (float volume) noexcept;
    [[nodiscard]] float getVolume() const noexcept { return volume.load(); }

    /** When true, stage media restarts from the beginning at end-of-file. */
    void setLooping (bool shouldLoop) noexcept;
    [[nodiscard]] bool isLooping() const noexcept { return looping.load (std::memory_order_relaxed); }

    [[nodiscard]] bool isPlaying() const noexcept;
    [[nodiscard]] bool hasMedia() const noexcept { return readerSource != nullptr; }
    [[nodiscard]] bool hasVideo() const noexcept;
    [[nodiscard]] bool isSlideshow() const noexcept { return slideshowMode; }
    [[nodiscard]] juce::File getFile() const { return currentFile; }
    [[nodiscard]] juce::String getDisplayName() const;
    [[nodiscard]] juce::String getCodecInfo() const;

    void setPosition (double seconds);
    [[nodiscard]] double getPosition() const;
    [[nodiscard]] double getLengthInSeconds() const;

    /** 0..1 envelope for meters / stage FX. */
    [[nodiscard]] float getMeterLevel() const noexcept { return meterLevel.load(); }

    /**
     * Latest stage frame (decoded video or current slideshow image).
     * Non-blocking for video; slideshow indexes by playhead.
     */
    juce::Image getVideoFrame();
    [[nodiscard]] uint32_t getVideoFrameSerial() const noexcept;

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;

private:
    juce::AudioFormatReader* openReader (const juce::File& file, juce::String& errorMessage);
    void clearMediaUnlocked();
    juce::Image loadSlideImage (const juce::File& file) const;

    juce::AudioFormatManager& formatManager;
    juce::AudioTransportSource transportSource;
    std::unique_ptr<juce::AudioFormatReaderSource> readerSource;
    std::unique_ptr<FFmpegVideoDecoder> videoDecoder;
    juce::File currentFile;
    juce::String codecInfo;
    bool loadedViaFFmpeg = false;
    bool slideshowMode = false;
    double slideSeconds = 5.0;
    std::vector<juce::Image> slides;
    std::atomic<int> currentSlideIndex { 0 };

    std::atomic<float> volume { 0.85f };
    std::atomic<float> meterLevel { 0.0f };
    std::atomic<bool> looping { false };
    double deviceSampleRate = 44100.0;
    int blockSize = 512;
    std::mutex loadMutex;
};

} // namespace jamstudio::audio
