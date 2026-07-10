#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cstdint>

namespace jamstudio::audio
{

/** True when JamStudio was built with libav / FFmpeg MPEG codec support. */
[[nodiscard]] bool isFFmpegMediaAvailable() noexcept;

/** Human-readable summary of FFmpeg codec support for UI. */
[[nodiscard]] juce::String getFFmpegMediaSupportSummary();

/**
 * AudioFormat backed by FFmpeg (libavformat/libavcodec/libswresample).
 * Opens MPEG audio/video containers for stage media and general import:
 * MP3, AAC/M4A, MP4, MOV, MKV, MPEG/MPG, WEBM, TS, AVI, etc.
 */
class FFmpegAudioFormat : public juce::AudioFormat
{
public:
    FFmpegAudioFormat();
    ~FFmpegAudioFormat() override = default;

    juce::Array<int> getPossibleSampleRates() override;
    juce::Array<int> getPossibleBitDepths() override;
    bool canDoStereo() override { return true; }
    bool canDoMono() override { return true; }
    bool isCompressed() override { return true; }

    juce::AudioFormatReader* createReaderFor (juce::InputStream* sourceStream,
                                              bool deleteStreamIfOpeningFails) override;

    std::unique_ptr<juce::AudioFormatWriter> createWriterFor (
        std::unique_ptr<juce::OutputStream>& streamToWriteTo,
        const juce::AudioFormatWriterOptions& options) override;

    using AudioFormat::createWriterFor;

    /** Open by filesystem path (preferred — FFmpeg needs a path or custom AVIO). */
    [[nodiscard]] static juce::AudioFormatReader* createReaderForFile (const juce::File& file);

    /**
     * Silent stereo float reader so video-only media can still drive transport position.
     * Caller owns the returned pointer.
     */
    [[nodiscard]] static juce::AudioFormatReader* createSilentReader (double durationSeconds,
                                                                      double sampleRate = 48000.0);
};

/** Lightweight FFmpeg probe of a media file (audio/video presence + duration). */
struct FFmpegMediaInfo
{
    bool hasAudio = false;
    bool hasVideo = false;
    double durationSeconds = 0.0;
    juce::String audioCodec;
    juce::String videoCodec;
    juce::String error;
};

[[nodiscard]] bool probeFFmpegMedia (const juce::File& file, FFmpegMediaInfo& info);

/**
 * Optional video track from the same media file as stage audio.
 * Decodes on a background thread so the UI never blocks on H.264.
 * Publishes downscaled ARGB frames for the Stage FX output window.
 */
class FFmpegVideoDecoder : private juce::Thread
{
public:
    FFmpegVideoDecoder();
    ~FFmpegVideoDecoder() override;

    FFmpegVideoDecoder (const FFmpegVideoDecoder&) = delete;
    FFmpegVideoDecoder& operator= (const FFmpegVideoDecoder&) = delete;

    [[nodiscard]] bool open (const juce::File& file, juce::String& errorMessage);
    void close();

    [[nodiscard]] bool isOpen() const noexcept { return openFlag.load (std::memory_order_acquire); }
    [[nodiscard]] double getDurationSeconds() const noexcept { return durationSeconds; }
    [[nodiscard]] int getWidth() const noexcept { return frameWidth; }
    [[nodiscard]] int getHeight() const noexcept { return frameHeight; }
    [[nodiscard]] uint32_t getFrameSerial() const noexcept { return frameSerial.load (std::memory_order_relaxed); }

    /** Non-blocking: tell the worker where playback is. */
    void setTargetTime (double seconds) noexcept;

    /**
     * Non-blocking: latest ready display frame (may be null until first decode).
     * Safe to call from the message thread.
     */
    juce::Image getLatestFrame() const;

    /** @deprecated Prefer setTargetTime + getLatestFrame (non-blocking). */
    juce::Image getFrameAt (double seconds);

private:
    void run() override;
    void decodeWorkerStep();
    bool seekTo (double seconds);
    bool pullOneFrame (double& outPts);
    bool convertLatestFrameToDisplay();
    void flushDecoder();
    void releaseMedia();

    // Media state (worker thread only while running)
    void* formatContext = nullptr;
    void* codecContext = nullptr;
    void* packet = nullptr;
    void* frame = nullptr;
    int videoStreamIndex = -1;
    double durationSeconds = 0.0;
    double timeBaseSeconds = 0.0;
    double lastPtsSeconds = -1.0;
    int frameWidth = 0;
    int frameHeight = 0;
    int displayWidth = 0;
    int displayHeight = 0;

    juce::Image workImage;   // written by worker
    juce::Image publishedImage; // readable by UI
    mutable juce::CriticalSection imageLock;

    std::atomic<bool> openFlag { false };
    std::atomic<double> targetSeconds { 0.0 };
    std::atomic<uint32_t> frameSerial { 0 };
};

} // namespace jamstudio::audio
