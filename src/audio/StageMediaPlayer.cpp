#include "StageMediaPlayer.h"

#include <cmath>

namespace jamstudio::audio
{

StageMediaPlayer::StageMediaPlayer (juce::AudioFormatManager& formats)
    : formatManager (formats)
{
}

void StageMediaPlayer::clearMediaUnlocked()
{
    transportSource.stop();
    transportSource.setSource (nullptr);
    readerSource.reset();
    videoDecoder.reset();
    slides.clear();
    slideshowMode = false;
    currentSlideIndex = 0;
    currentFile = juce::File();
    codecInfo.clear();
    loadedViaFFmpeg = false;
    meterLevel = 0.0f;
}

juce::AudioFormatReader* StageMediaPlayer::openReader (const juce::File& file, juce::String& errorMessage)
{
    if (isFFmpegMediaAvailable())
    {
        if (auto* ffmpegReader = FFmpegAudioFormat::createReaderForFile (file))
        {
            loadedViaFFmpeg = true;
            codecInfo = "FFmpeg: " + file.getFileExtension().toUpperCase().trimCharactersAtStart (".");
            return ffmpegReader;
        }
    }

    loadedViaFFmpeg = false;

    if (auto* juceReader = formatManager.createReaderFor (file))
    {
        codecInfo = "Native: " + file.getFileExtension().toUpperCase().trimCharactersAtStart (".");
        return juceReader;
    }

    errorMessage = "Could not open audio track.";
    return nullptr;
}

juce::Image StageMediaPlayer::loadSlideImage (const juce::File& file) const
{
    auto img = juce::ImageFileFormat::loadFrom (file);

    if (! img.isValid())
        return {};

    // Cap slide size for smooth stage playback.
    constexpr int maxEdge = 960;
    const int longEdge = juce::jmax (img.getWidth(), img.getHeight());

    if (longEdge > maxEdge)
    {
        const double scale = static_cast<double> (maxEdge) / static_cast<double> (longEdge);
        const int w = juce::jmax (2, static_cast<int> (img.getWidth() * scale));
        const int h = juce::jmax (2, static_cast<int> (img.getHeight() * scale));
        juce::Image scaled (juce::Image::ARGB, w, h, true);
        juce::Graphics g (scaled);
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImage (img, scaled.getBounds().toFloat());
        return scaled;
    }

    return img;
}

bool StageMediaPlayer::loadFile (const juce::File& file, juce::String& errorMessage)
{
    const std::lock_guard lock (loadMutex);

    if (! file.existsAsFile())
    {
        errorMessage = "File does not exist.";
        return false;
    }

    clearMediaUnlocked();

    FFmpegMediaInfo probe;
    const bool probed = isFFmpegMediaAvailable() && probeFFmpegMedia (file, probe);

    juce::String audioError;
    auto* reader = openReader (file, audioError);

    if (reader == nullptr && probed && probe.hasVideo && ! probe.hasAudio)
    {
        const auto duration = probe.durationSeconds > 0.05 ? probe.durationSeconds : 60.0;
        reader = FFmpegAudioFormat::createSilentReader (duration, 48000.0);
        loadedViaFFmpeg = true;
        codecInfo = "FFmpeg: video-only"
                    + (probe.videoCodec.isNotEmpty() ? (" " + probe.videoCodec) : juce::String());
    }

    if (reader == nullptr && probed && probe.hasVideo)
    {
        const auto duration = probe.durationSeconds > 0.05 ? probe.durationSeconds : 60.0;
        reader = FFmpegAudioFormat::createSilentReader (duration, 48000.0);
        loadedViaFFmpeg = true;
        codecInfo = "FFmpeg: video (silent audio)"
                    + (probe.videoCodec.isNotEmpty() ? (" " + probe.videoCodec) : juce::String());
    }

    if (reader == nullptr)
    {
        errorMessage = audioError.isNotEmpty()
                           ? audioError
                           : "Could not open media (unsupported format or missing codec).";
        return false;
    }

    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    readerSource->setLooping (looping.load (std::memory_order_relaxed));
    transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    transportSource.prepareToPlay (blockSize, deviceSampleRate);
    transportSource.setPosition (0.0);
    currentFile = file;

    if (isFFmpegMediaAvailable() && (! probed || probe.hasVideo))
    {
        auto decoder = std::make_unique<FFmpegVideoDecoder>();
        juce::String videoError;

        if (decoder->open (file, videoError))
        {
            videoDecoder = std::move (decoder);
            if (! codecInfo.containsIgnoreCase ("video"))
                codecInfo += " + video";
        }
    }

    sendChangeMessage();
    return true;
}

bool StageMediaPlayer::loadSlideshow (const juce::Array<juce::File>& images,
                                      const double secondsPerSlide,
                                      juce::String& errorMessage)
{
    const std::lock_guard lock (loadMutex);
    clearMediaUnlocked();

    if (images.isEmpty())
    {
        errorMessage = "No slideshow images selected.";
        return false;
    }

    slideSeconds = juce::jlimit (0.5, 120.0, secondsPerSlide);
    slides.reserve (static_cast<size_t> (images.size()));

    for (const auto& f : images)
    {
        if (! f.existsAsFile())
            continue;

        auto img = loadSlideImage (f);
        if (img.isValid())
            slides.push_back (std::move (img));
    }

    if (slides.empty())
    {
        errorMessage = "Could not load any slideshow images (try JPG/PNG).";
        return false;
    }

    const auto duration = static_cast<double> (slides.size()) * slideSeconds;
    auto* reader = FFmpegAudioFormat::createSilentReader (duration, 48000.0);
    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    readerSource->setLooping (looping.load (std::memory_order_relaxed));
    transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);
    transportSource.prepareToPlay (blockSize, deviceSampleRate);
    transportSource.setPosition (0.0);

    slideshowMode = true;
    currentFile = images.getFirst();
    codecInfo = "Slideshow: " + juce::String (static_cast<int> (slides.size()))
                + " slides @ " + juce::String (slideSeconds, 1) + "s";
    currentSlideIndex = 0;
    sendChangeMessage();
    return true;
}

void StageMediaPlayer::clear()
{
    const std::lock_guard lock (loadMutex);
    clearMediaUnlocked();
    sendChangeMessage();
}

void StageMediaPlayer::play()
{
    if (readerSource == nullptr)
        return;

    transportSource.start();
    sendChangeMessage();
}

void StageMediaPlayer::pause()
{
    transportSource.stop();
    sendChangeMessage();
}

void StageMediaPlayer::stop()
{
    transportSource.stop();
    transportSource.setPosition (0.0);
    currentSlideIndex = 0;
    meterLevel = 0.0f;
    sendChangeMessage();
}

void StageMediaPlayer::togglePlayPause()
{
    if (isPlaying())
        pause();
    else
        play();
}

bool StageMediaPlayer::isPlaying() const noexcept
{
    return transportSource.isPlaying();
}

bool StageMediaPlayer::hasVideo() const noexcept
{
    return slideshowMode || (videoDecoder != nullptr && videoDecoder->isOpen());
}

void StageMediaPlayer::setVolume (const float newVolume) noexcept
{
    volume.store (juce::jlimit (0.0f, 1.0f, newVolume));
}

void StageMediaPlayer::setLooping (const bool shouldLoop) noexcept
{
    looping.store (shouldLoop, std::memory_order_relaxed);

    if (readerSource != nullptr)
        readerSource->setLooping (shouldLoop);
}

juce::String StageMediaPlayer::getDisplayName() const
{
    if (slideshowMode)
        return "Slideshow (" + juce::String (static_cast<int> (slides.size())) + " slides)";

    if (currentFile.existsAsFile())
        return currentFile.getFileName();

    return "No media loaded";
}

juce::String StageMediaPlayer::getCodecInfo() const
{
    return codecInfo;
}

void StageMediaPlayer::setPosition (const double seconds)
{
    transportSource.setPosition (juce::jmax (0.0, seconds));
    sendChangeMessage();
}

double StageMediaPlayer::getPosition() const
{
    return transportSource.getCurrentPosition();
}

double StageMediaPlayer::getLengthInSeconds() const
{
    return transportSource.getLengthInSeconds();
}

juce::Image StageMediaPlayer::getVideoFrame()
{
    if (slideshowMode)
    {
        if (slides.empty())
            return {};

        const auto pos = getPosition();
        const auto idx = juce::jlimit (0, static_cast<int> (slides.size()) - 1,
                                       static_cast<int> (pos / juce::jmax (0.5, slideSeconds)));
        currentSlideIndex.store (idx, std::memory_order_relaxed);
        return slides[static_cast<size_t> (idx)];
    }

    if (videoDecoder == nullptr || ! videoDecoder->isOpen())
        return {};

    videoDecoder->setTargetTime (getPosition());
    return videoDecoder->getLatestFrame();
}

uint32_t StageMediaPlayer::getVideoFrameSerial() const noexcept
{
    if (slideshowMode)
        return static_cast<uint32_t> (currentSlideIndex.load (std::memory_order_relaxed) + 1);

    if (videoDecoder == nullptr || ! videoDecoder->isOpen())
        return 0;

    return videoDecoder->getFrameSerial();
}

void StageMediaPlayer::prepareToPlay (const int samplesPerBlockExpected, const double sampleRate)
{
    blockSize = samplesPerBlockExpected;
    deviceSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    transportSource.prepareToPlay (samplesPerBlockExpected, deviceSampleRate);
}

void StageMediaPlayer::releaseResources()
{
    transportSource.releaseResources();
}

void StageMediaPlayer::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    bufferToFill.clearActiveBufferRegion();

    if (readerSource == nullptr || ! transportSource.isPlaying())
    {
        meterLevel.store (meterLevel.load() * 0.9f);
        return;
    }

    transportSource.getNextAudioBlock (bufferToFill);

    const auto gain = volume.load();
    bufferToFill.buffer->applyGain (bufferToFill.startSample, bufferToFill.numSamples, gain);

    float peak = 0.0f;
    for (int ch = 0; ch < bufferToFill.buffer->getNumChannels(); ++ch)
    {
        const auto* data = bufferToFill.buffer->getReadPointer (ch, bufferToFill.startSample);
        for (int i = 0; i < bufferToFill.numSamples; ++i)
            peak = juce::jmax (peak, std::abs (data[i]));
    }

    const auto prev = meterLevel.load();
    meterLevel.store (peak >= prev ? peak : prev * 0.86f);

    if (transportSource.hasStreamFinished())
    {
        if (looping.load (std::memory_order_relaxed))
        {
            transportSource.setPosition (0.0);
            currentSlideIndex = 0;
            if (videoDecoder != nullptr)
                videoDecoder->setTargetTime (0.0);
            transportSource.start();
        }
        else
        {
            transportSource.stop();
            transportSource.setPosition (0.0);
            currentSlideIndex = 0;
            sendChangeMessage();
        }
    }
}

} // namespace jamstudio::audio
