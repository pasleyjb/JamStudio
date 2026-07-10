#pragma once

#include "Metronome.h"
#include "MultiBusMaster.h"
#include "StageMediaPlayer.h"
#include "StemMixer.h"

#include <atomic>

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
    Metronome& getMetronome() noexcept { return metronome; }
    StageMediaPlayer& getStageMedia() noexcept { return stageMedia; }
    MultiBusMaster& getMultiBusMaster() noexcept { return multiBusMaster; }
    juce::AudioFormatManager& getFormatManager() noexcept { return formatManager; }

    void play();
    void pause();
    void stop();
    void togglePlayPause();
    [[nodiscard]] bool isPlaying() const noexcept;

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
    juce::AudioSourcePlayer audioSourcePlayer;
    juce::AudioFormatManager formatManager;
    StemMixer stemMixer;
    Metronome metronome;
    StageMediaPlayer stageMedia;
    MultiBusMaster multiBusMaster;

    std::atomic<bool> linkStageMedia { true };
    std::atomic<bool> countInEnabled { false };
    std::atomic<bool> countingIn { false };
    std::atomic<bool> pendingPlaybackAfterCountIn { false };
    std::atomic<int> countInBeats { 4 };
    std::atomic<uint32_t> countInEpoch { 0 };
};

} // namespace jamstudio::audio
