#pragma once

#include <JuceHeader.h>

#include <atomic>
#include <vector>

namespace jamstudio::audio
{

class StemTrack;

/** One track requested when a take opens. The arm list is snapshotted by the caller. */
struct TakeTrackRequest
{
    enum class Kind
    {
        mic,        // Fast Track input 1, mono
        instrument, // Fast Track input 2, mono
        stem        // dry print of a loaded stem, stereo
    };

    Kind kind = Kind::mic;
    StemTrack* stem = nullptr;
    juce::String name;
};

/** A file written by one take. Sample 0 of the file is `startOffsetSeconds` on the song. */
struct WrittenTake
{
    TakeTrackRequest::Kind kind = TakeTrackRequest::Kind::mic;
    juce::String name;
    juce::File file;
    double startOffsetSeconds = 0.0;
    juce::int64 samplesWritten = 0;
};

struct TakeResult
{
    juce::File folder;
    std::vector<WrittenTake> tracks;
};

/**
 * Records Fast Track inputs and armed stem prints on the playback clock.
 *
 * Register this callback BEFORE the transport AudioSourcePlayer. The device
 * callback latches inputs, then the mixer writes those samples with the stem
 * drys from the same block. Offline bounces call the mixer off the audio
 * thread and are ignored.
 */
class TakeRecorder : public juce::AudioIODeviceCallback
{
public:
    TakeRecorder();
    ~TakeRecorder() override;

    TakeRecorder (const TakeRecorder&) = delete;
    TakeRecorder& operator= (const TakeRecorder&) = delete;

    /** Message thread. Opens 16-bit WAV writers at the device rate. */
    bool start (const juce::File& folder, const std::vector<TakeTrackRequest>& tracks);
    /** Message thread. Flushes writers and returns what was captured. */
    TakeResult stop();

    [[nodiscard]] bool isRecording() const noexcept;
    [[nodiscard]] juce::String getLastError() const { return lastError; }

    /** True while this stem has an open writer for the current take. Audio thread. */
    [[nodiscard]] bool isPrinting (const StemTrack* stem) const;

    /**
     * Audio thread. Copies the mic and instrument latched at the start of this
     * device callback. Returns 0 off the audio thread so a bounce stays silent.
     */
    int copyLatchedInputs (float* mic, float* instrument, int numSamples) const noexcept;

    /** Audio thread, from StemMixer, only while the transport is playing. */
    void beginBlock (double startSeconds, int numSamples);
    void writeStem (const StemTrack* stem, const juce::AudioBuffer<float>& dry, int numSamples);
    void writeStemSilence (const StemTrack* stem, int numSamples);
    void commitBlock();

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;

private:
    struct Track;
    struct Session;

    bool addTrack (Session& destination, const TakeTrackRequest& request, juce::StringArray& usedNames, double rate);
    void writeTrack (Track& track, const float* const* data, int numSamples);
    Track* findStem (Session& current, const StemTrack* stem) noexcept;

    juce::TimeSliceThread backgroundThread { "JamStudio Take Recorder" };
    mutable juce::CriticalSection sessionLock;
    std::atomic<Session*> active { nullptr };
    std::unique_ptr<Session> session;
    juce::String lastError;

    std::atomic<double> deviceSampleRate { 0.0 };
    std::atomic<int> latchCapacity { 0 };
    std::vector<float> micLatch;
    std::vector<float> instLatch;
    std::vector<float> silence;
    int latchedSamples = 0;

    int blockSamples = 0;
    bool blockOpen = false;
};

} // namespace jamstudio::audio
