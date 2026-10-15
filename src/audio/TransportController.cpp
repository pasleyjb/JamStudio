#include "TransportController.h"
#include "FFmpegAudioFormat.h"
#include "MixBus.h"

namespace jamstudio::audio
{

namespace
{
class GraphCallback : public juce::AudioIODeviceCallback
{
public:
    GraphCallback (juce::AudioSource& sourceIn, std::atomic<MixOutputTap*>& tapIn)
        : source (sourceIn), tap (tapIn)
    {
    }

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override
    {
        const auto rate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
        const auto block = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;

        if (rate > 0.0 && block > 0)
        {
            source.prepareToPlay (block, rate);
            mix.setSize (kMaxMixChannels, block, false, true, true);
        }
    }

    void audioDeviceStopped() override
    {
        source.releaseResources();
    }

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        juce::ignoreUnused (inputChannelData, numInputChannels);

        if (numSamples <= 0)
            return;

        if (mix.getNumChannels() < kMaxMixChannels || mix.getNumSamples() < numSamples)
            mix.setSize (kMaxMixChannels, numSamples, false, false, true);

        mix.clear();
        float* channels[kMaxMixChannels];

        for (int i = 0; i < kMaxMixChannels; ++i)
            channels[i] = mix.getWritePointer (i);

        juce::AudioBuffer<float> buffer (channels, kMaxMixChannels, numSamples);
        juce::AudioSourceChannelInfo info (&buffer, 0, numSamples);
        source.getNextAudioBlock (info);

        if (auto* sink = tap.load (std::memory_order_acquire))
        {
            sink->sendMix (mix.getArrayOfReadPointers(), kMaxMixChannels, numSamples,
                           outputChannelData, numOutputChannels);
            return;
        }

        if (outputChannelData == nullptr)
            return;

        const auto n = juce::jmin (numOutputChannels, kMaxMixChannels);

        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
                continue;

            if (ch < n)
                juce::FloatVectorOperations::copy (outputChannelData[ch], mix.getReadPointer (ch), numSamples);
            else
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
        }
    }

private:
    juce::AudioSource& source;
    std::atomic<MixOutputTap*>& tap;
    juce::AudioBuffer<float> mix;
};
} // namespace

TransportController::TransportController (juce::AudioDeviceManager& dm)
    : deviceManager (dm),
      stemMixer (formatManager),
      stageMedia (formatManager),
      multiBusMaster (stemMixer, metronome, stageMedia, takeRecorder, liveInputs)
{
    formatManager.registerBasicFormats();

    if (isFFmpegMediaAvailable())
        formatManager.registerFormat (new FFmpegAudioFormat(), false);

    graphCallback = std::make_unique<GraphCallback> (multiBusMaster, mixTap);
    metronome.setCountInFinishedCallback ([this] { onCountInFinished(); });
    // Latch Fast Track inputs before the mixer renders this block, so a take
    // lines the mic up with the stem samples from the same callback.
    stemMixer.setTakeRecorder (&takeRecorder);
    deviceManager.addAudioCallback (&takeRecorder);
    deviceManager.addAudioCallback (graphCallback.get());
}

TransportController::~TransportController()
{
    if (graphCallback != nullptr)
        deviceManager.removeAudioCallback (graphCallback.get());

    deviceManager.removeAudioCallback (&takeRecorder);
    graphCallback.reset();
    stemMixer.setTakeRecorder (nullptr);
    takeRecorder.stop();
    metronome.setCountInFinishedCallback ({});
    cancelCountIn();
}

void TransportController::setMixOutputTap (MixOutputTap* tap) noexcept
{
    mixTap.store (tap, std::memory_order_release);
}

void TransportController::setLinkStageMedia (const bool shouldLink) noexcept
{
    linkStageMedia.store (shouldLink, std::memory_order_relaxed);
}

void TransportController::setCountInEnabled (const bool shouldEnable) noexcept
{
    countInEnabled.store (shouldEnable, std::memory_order_relaxed);

    if (! shouldEnable)
        cancelCountIn();
}

void TransportController::setCountInBeats (const int beats) noexcept
{
    countInBeats.store (juce::jlimit (1, 16, beats), std::memory_order_relaxed);
}

void TransportController::syncStageMediaPlay()
{
    if (! linkStageMedia.load (std::memory_order_relaxed))
        return;

    if (stageMedia.hasMedia())
        stageMedia.play();
}

void TransportController::syncStageMediaPause()
{
    if (! linkStageMedia.load (std::memory_order_relaxed))
        return;

    stageMedia.pause();
}

void TransportController::syncStageMediaStop()
{
    if (! linkStageMedia.load (std::memory_order_relaxed))
        return;

    stageMedia.stop();
}

void TransportController::syncStageMediaPosition (const double seconds)
{
    if (! linkStageMedia.load (std::memory_order_relaxed))
        return;

    if (! stageMedia.hasMedia())
        return;

    const auto len = stageMedia.getLengthInSeconds();
    const auto pos = len > 0.0 ? juce::jlimit (0.0, len, seconds) : juce::jmax (0.0, seconds);
    stageMedia.setPosition (pos);
}

void TransportController::play()
{
    if (stemMixer.isPlaying())
    {
        syncStageMediaPlay();
        return;
    }

    if (countingIn.load (std::memory_order_relaxed))
        return;

    if (countInEnabled.load (std::memory_order_relaxed))
    {
        ++countInEpoch;
        pendingPlaybackAfterCountIn.store (true, std::memory_order_relaxed);
        countingIn.store (true, std::memory_order_relaxed);
        metronome.startCountIn (countInBeats.load (std::memory_order_relaxed));
        sendChangeMessage();
        return;
    }

    beginPlayback();
}

void TransportController::setBeforePlayback (std::function<bool()> callback)
{
    beforePlayback = std::move (callback);
}

void TransportController::beginPlayback()
{
    const auto hook = beforePlayback;

    if (hook != nullptr && ! hook())
    {
        sendChangeMessage();
        return;
    }

    stemMixer.play();
    syncStageMediaPlay();
    sendChangeMessage();
}

void TransportController::onCountInFinished()
{
    if (! pendingPlaybackAfterCountIn.exchange (false, std::memory_order_relaxed))
        return;

    countingIn.store (false, std::memory_order_relaxed);
    beginPlayback();
}

void TransportController::cancelCountIn()
{
    pendingPlaybackAfterCountIn.store (false, std::memory_order_relaxed);
    countingIn.store (false, std::memory_order_relaxed);
    metronome.cancelCountIn();
}

void TransportController::pause()
{
    cancelCountIn();
    stemMixer.pause();
    syncStageMediaPause();
    sendChangeMessage();
}

void TransportController::stop()
{
    cancelCountIn();
    stemMixer.stop();
    metronome.reset();
    syncStageMediaStop();
    sendChangeMessage();
}

void TransportController::togglePlayPause()
{
    if (isPlaying() || isCountingIn())
        pause();
    else
        play();
}

bool TransportController::isPlaying() const noexcept
{
    return stemMixer.isPlaying();
}

void TransportController::setPosition (const double seconds)
{
    cancelCountIn();
    stemMixer.setPosition (seconds);
    metronome.reset();
    syncStageMediaPosition (seconds);
    sendChangeMessage();
}

void TransportController::skipForward (const double seconds)
{
    setPosition (getPosition() + juce::jmax (0.1, seconds));
}

void TransportController::skipBack (const double seconds)
{
    setPosition (getPosition() - juce::jmax (0.1, seconds));
}

double TransportController::getPosition() const noexcept
{
    return stemMixer.getPosition();
}

double TransportController::getLengthInSeconds() const noexcept
{
    return stemMixer.getLengthInSeconds();
}

} // namespace jamstudio::audio
