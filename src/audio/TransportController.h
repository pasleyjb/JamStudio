#pragma once

#include "LiveInputs.h"
#include "Metronome.h"
#include "MultiBusMaster.h"
#include "StageMediaPlayer.h"
#include "StemMixer.h"
#include "TakeRecorder.h"

#include <atomic>
#include <memory>

namespace jamstudio::audio
{

/**
 * Owns the master audio graph: multi-bus stems + click + stage media.
 * Main transport drives setlist stems and stage video together.
 */
class TransportController : public juce::ChangeBroadcaster
{
public:
    explicit TransportController (juce::AudioDeviceManager& deviceManager);
    ~TransportController() override;

    TransportController (const TransportController&) = delete;
    TransportController& operator= (const TransportController&) = delete;

    StemMixer& getStemMixer() noexcept { return stemMixer; }
    LiveInputs& getLiveInputs() noexcept { return liveInputs; }
    TakeRecorder& getTakeRecorder() noexcept { return takeRecorder; }
    Metronome& getMetronome() noexcept { return metronome; }
    StageMediaPlayer& getStageMedia() noexcept { return stageMedia; }
    MultiBusMaster& getMultiBusMaster() noexcept { return multiBusMaster; }
    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    void play();
    void pause();
    void stop();
    void togglePlayPause();
    [[nodiscard]] bool isPlaying() const noexcept;

    /** Message-thread hook run once playback is actually about to start (after a count-in).
        Return false to cancel. Cleared by the caller when it should only fire once. */
    void setBeforePlayback (std::function<bool()> callback);

    void skipForward (double seconds = 5.0);
    void skipBack (double seconds = 5.0);

    void setLinkStageMedia (bool shouldLink) noexcept;
    [[nodiscard]] bool isStageMediaLinked() const noexcept { return linkStageMedia.load (std::memory_order_relaxed); }

    void setCountInEnabled (bool shouldEnable) noexcept;
    [[nodiscard]] bool isCountInEnabled() const noexcept { return countInEnabled.load (std::memory_order_relaxed); }
    [[nodiscard]] bool isCountingIn() const noexcept { return countingIn.load (std::memory_order_relaxed); }
    void setCountInBeats (int beats) noexcept;
    [[nodiscard]] int getCountInBeats() const noexcept { return countInBeats.load (std::memory_order_relaxed); }

    void setPosition (double seconds);
    [[nodiscard]] double getPosition() const noexcept;
    [[nodiscard]] double getLengthInSeconds() const noexcept;

private:
    void beginPlayback();
    void cancelCountIn();
    void onCountInFinished();
    void syncStageMediaPlay();
    void syncStageMediaPause();
    void syncStageMediaStop();
    void syncStageMediaPosition (double seconds);

    juce::AudioDeviceManager& deviceManager;
    // Renders the mix without copying the live input onto the outputs.
    // JUCE's AudioSourcePlayer does that copy, which howls when the Mac mic
    // and speakers (or a loopback driver) are both open.
    std::unique_ptr<juce::AudioIODeviceCallback> graphCallback;
    juce::AudioFormatManager formatManager;
    StemMixer stemMixer;
    Metronome metronome;
    StageMediaPlayer stageMedia;
    TakeRecorder takeRecorder;
    LiveInputs liveInputs;
    MultiBusMaster multiBusMaster;

    std::atomic<bool> linkStageMedia { true };
    std::atomic<bool> countInEnabled { false };
    std::atomic<bool> countingIn { false };
    std::atomic<bool> pendingPlaybackAfterCountIn { false };
    std::atomic<int> countInBeats { 4 };
    std::atomic<uint32_t> countInEpoch { 0 };
    std::function<bool()> beforePlayback;
};

} // namespace jamstudio::audio
