#include "TakeRecorder.h"

#include "StemTrack.h"

#include <cmath>

namespace jamstudio::audio
{
namespace
{
// Sticky per thread. The device callback sets it; the mixer runs later on that
// same thread. A bounce on the message thread never sees it.
thread_local bool takeRecorderOnAudioThread = false;

juce::String legalStemBase (juce::String name)
{
    name = name.trim();

    if (name.endsWithIgnoreCase (".wav"))
        name = name.dropLastCharacters (4);

    name = juce::File::createLegalFileName (name);

    if (name.isEmpty())
        name = "Stem";

    return name;
}
} // namespace

struct TakeRecorder::Track
{
    TakeTrackRequest::Kind kind = TakeTrackRequest::Kind::mic;
    const StemTrack* stem = nullptr;
    juce::String name;
    juce::File file;
    int numChannels = 1;
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> writer;
    juce::int64 samplesWritten = 0;
};

struct TakeRecorder::Session
{
    juce::File folder;
    double sampleRate = 0.0;
    double startOffsetSeconds = 0.0;
    bool offsetCaptured = false;
    std::vector<Track> tracks;
};

TakeRecorder::TakeRecorder()
{
    constexpr int cap = 32768;
    micLatch.assign (static_cast<size_t> (cap), 0.0f);
    instLatch.assign (static_cast<size_t> (cap), 0.0f);
    silence.assign (static_cast<size_t> (cap), 0.0f);
    latchCapacity.store (cap, std::memory_order_relaxed);
    backgroundThread.startThread();
}

TakeRecorder::~TakeRecorder()
{
    stop();
}

bool TakeRecorder::addTrack (Session& destination,
                             const TakeTrackRequest& request,
                             juce::StringArray& usedNames,
                             const double rate)
{
    if (request.kind == TakeTrackRequest::Kind::stem && request.stem == nullptr)
        return true;

    auto base = legalStemBase (request.name);

    if (base.isEmpty())
        base = request.kind == TakeTrackRequest::Kind::instrument ? "Inst" : "Mic";

    auto candidate = base;
    auto suffix = 2;

    while (usedNames.contains (candidate))
        candidate = base + " " + juce::String (suffix++);

    usedNames.add (candidate);

    Track track;
    track.kind = request.kind;
    track.stem = request.kind == TakeTrackRequest::Kind::stem ? request.stem : nullptr;
    track.name = candidate;
    track.numChannels = request.kind == TakeTrackRequest::Kind::stem ? 2 : 1;
    track.file = destination.folder.getChildFile (candidate + ".wav");
    track.file.deleteFile();

    if (auto fileStream = std::unique_ptr<juce::OutputStream> (track.file.createOutputStream()))
    {
        juce::WavAudioFormat wavFormat;
        const auto options = juce::AudioFormatWriterOptions{}
                                 .withSampleRate (rate)
                                 .withNumChannels (track.numChannels)
                                 .withBitsPerSample (16);

        if (auto writer = wavFormat.createWriterFor (fileStream, options))
        {
            track.writer = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (
                writer.release(), backgroundThread, 65536);
            track.writer->setFlushInterval (juce::jmax (1, static_cast<int> (rate)));
            destination.tracks.push_back (std::move (track));
            return true;
        }
    }

    lastError = "Could not create " + track.file.getFileName() + ".";
    track.file.deleteFile();
    return false;
}

bool TakeRecorder::start (const juce::File& folder, const std::vector<TakeTrackRequest>& tracks)
{
    stop();
    lastError.clear();

    if (tracks.empty())
    {
        lastError = "Nothing is armed.";
        return false;
    }

    const auto rate = deviceSampleRate.load (std::memory_order_relaxed);

    if (rate <= 0.0)
    {
        lastError = "Could not start recording. The audio device is not running.";
        return false;
    }

    if (! folder.createDirectory())
    {
        lastError = "Could not create the take folder.";
        return false;
    }

    auto next = std::make_unique<Session>();
    next->folder = folder;
    next->sampleRate = rate;
    juce::StringArray usedNames;

    for (const auto& request : tracks)
    {
        if (! addTrack (*next, request, usedNames, rate))
        {
            juce::Array<juce::File> created;

            for (auto& track : next->tracks)
            {
                created.add (track.file);
                track.writer.reset();
            }

            for (const auto& createdFile : created)
                createdFile.deleteFile();

            next.reset();
            return false;
        }
    }

    if (next->tracks.empty())
    {
        lastError = "Nothing is armed.";
        return false;
    }

    const juce::ScopedLock lock (sessionLock);
    session = std::move (next);
    active.store (session.get(), std::memory_order_release);
    return true;
}

TakeResult TakeRecorder::stop()
{
    TakeResult result;

    {
        const juce::ScopedLock lock (sessionLock);
        active.store (nullptr, std::memory_order_release);
    }

    if (session == nullptr)
        return result;

    result.folder = session->folder;

    for (auto& track : session->tracks)
    {
        WrittenTake written;
        written.kind = track.kind;
        written.name = track.name;
        written.file = track.file;
        written.startOffsetSeconds = session->startOffsetSeconds;
        written.samplesWritten = track.samplesWritten;
        track.writer.reset();
        result.tracks.push_back (std::move (written));
    }

    session.reset();
    return result;
}

bool TakeRecorder::isRecording() const noexcept
{
    return active.load (std::memory_order_acquire) != nullptr;
}

bool TakeRecorder::isPrinting (const StemTrack* stem) const
{
    if (! takeRecorderOnAudioThread || stem == nullptr)
        return false;

    if (active.load (std::memory_order_acquire) == nullptr)
        return false;

    const juce::ScopedLock lock (sessionLock);
    const auto* current = active.load (std::memory_order_relaxed);

    if (current == nullptr)
        return false;

    for (const auto& track : current->tracks)
        if (track.kind == TakeTrackRequest::Kind::stem && track.stem == stem)
            return true;

    return false;
}

TakeRecorder::Track* TakeRecorder::findStem (Session& current, const StemTrack* stem) noexcept
{
    if (stem == nullptr)
        return nullptr;

    for (auto& track : current.tracks)
        if (track.kind == TakeTrackRequest::Kind::stem && track.stem == stem)
            return &track;

    return nullptr;
}

void TakeRecorder::writeTrack (Track& track, const float* const* data, const int numSamples)
{
    if (track.writer == nullptr || data == nullptr || numSamples <= 0)
        return;

    if (track.writer->write (data, numSamples))
        track.samplesWritten += numSamples;
}

int TakeRecorder::copyLatchedInputs (float* mic, float* instrument, const int numSamples) const noexcept
{
    if (! takeRecorderOnAudioThread || numSamples <= 0 || mic == nullptr || instrument == nullptr)
        return 0;

    const auto frames = juce::jmin (numSamples,
                                    latchedSamples,
                                    static_cast<int> (micLatch.size()),
                                    static_cast<int> (instLatch.size()));

    if (frames > 0)
    {
        juce::FloatVectorOperations::copy (mic, micLatch.data(), frames);
        juce::FloatVectorOperations::copy (instrument, instLatch.data(), frames);
    }

    if (frames < numSamples)
    {
        juce::FloatVectorOperations::clear (mic + frames, numSamples - frames);
        juce::FloatVectorOperations::clear (instrument + frames, numSamples - frames);
    }

    return frames;
}

void TakeRecorder::beginBlock (const double startSeconds, const int numSamples)
{
    if (! takeRecorderOnAudioThread || numSamples <= 0)
        return;

    blockOpen = false;

    const juce::ScopedLock lock (sessionLock);
    auto* current = active.load (std::memory_order_relaxed);

    if (current == nullptr)
        return;

    if (! current->offsetCaptured)
    {
        current->startOffsetSeconds = std::isfinite (startSeconds) ? juce::jmax (0.0, startSeconds) : 0.0;
        current->offsetCaptured = true;
    }

    const auto cap = latchCapacity.load (std::memory_order_relaxed);
    blockSamples = juce::jlimit (0, cap, numSamples);
    blockOpen = blockSamples > 0;
}

void TakeRecorder::writeStem (const StemTrack* stem, const juce::AudioBuffer<float>& dry, const int numSamples)
{
    if (! blockOpen || ! takeRecorderOnAudioThread)
        return;

    const juce::ScopedLock lock (sessionLock);
    auto* current = active.load (std::memory_order_relaxed);

    if (current == nullptr)
        return;

    auto* track = findStem (*current, stem);

    if (track == nullptr)
        return;

    const auto frames = juce::jmin (blockSamples, numSamples, dry.getNumSamples());
    const auto channels = dry.getNumChannels();

    if (frames <= 0 || channels <= 0 || silence.empty())
    {
        const auto quietFrames = juce::jmin (blockSamples, static_cast<int> (silence.size()));
        const float* quiet[2] = { silence.data(), silence.data() };
        writeTrack (*track, quiet, quietFrames);
        return;
    }

    const float* pointers[2] = {
        dry.getReadPointer (0),
        dry.getReadPointer (juce::jmin (1, channels - 1))
    };

    writeTrack (*track, pointers, frames);

    const auto remain = blockSamples - frames;

    if (remain > 0 && ! silence.empty())
    {
        const float* quiet[2] = { silence.data(), silence.data() };
        writeTrack (*track, quiet, remain);
    }
}

void TakeRecorder::writeStemSilence (const StemTrack* stem, const int numSamples)
{
    if (! blockOpen || ! takeRecorderOnAudioThread || silence.empty())
        return;

    const juce::ScopedLock lock (sessionLock);
    auto* current = active.load (std::memory_order_relaxed);

    if (current == nullptr)
        return;

    auto* track = findStem (*current, stem);

    if (track == nullptr)
        return;

    const auto frames = juce::jmin (blockSamples, numSamples, static_cast<int> (silence.size()));
    const float* quiet[2] = { silence.data(), silence.data() };
    writeTrack (*track, quiet, frames);
}

void TakeRecorder::commitBlock()
{
    if (! takeRecorderOnAudioThread)
        return;

    if (! blockOpen)
        return;

    blockOpen = false;

    const juce::ScopedLock lock (sessionLock);
    auto* current = active.load (std::memory_order_relaxed);

    if (current == nullptr || silence.empty())
        return;

    const auto frames = juce::jmin (blockSamples, static_cast<int> (silence.size()));
    const auto fromLatch = juce::jlimit (0, frames, latchedSamples);

    for (auto& track : current->tracks)
    {
        if (track.kind == TakeTrackRequest::Kind::stem)
            continue;

        const auto& source = track.kind == TakeTrackRequest::Kind::instrument ? instLatch : micLatch;

        if (fromLatch > 0 && static_cast<int> (source.size()) >= fromLatch)
        {
            const float* pointer = source.data();
            writeTrack (track, &pointer, fromLatch);
        }

        const auto remain = frames - fromLatch;

        if (remain > 0)
        {
            const float* pointer = silence.data();
            writeTrack (track, &pointer, remain);
        }
    }
}

void TakeRecorder::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    const auto rate = device != nullptr ? device->getCurrentSampleRate() : 0.0;
    const auto buffer = device != nullptr ? device->getCurrentBufferSizeSamples() : 0;
    deviceSampleRate.store (rate > 0.0 ? rate : 0.0, std::memory_order_relaxed);

    const auto cap = juce::jmax (32768, buffer * 2);

    if (latchCapacity.load (std::memory_order_relaxed) < cap)
    {
        micLatch.assign (static_cast<size_t> (cap), 0.0f);
        instLatch.assign (static_cast<size_t> (cap), 0.0f);
        silence.assign (static_cast<size_t> (cap), 0.0f);
        latchCapacity.store (cap, std::memory_order_relaxed);
    }
}

void TakeRecorder::audioDeviceStopped()
{
    deviceSampleRate.store (0.0, std::memory_order_relaxed);
    latchedSamples = 0;
    blockOpen = false;
}

void TakeRecorder::audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                                     const int numInputChannels,
                                                     float* const* outputChannelData,
                                                     const int numOutputChannels,
                                                     const int numSamples,
                                                     const juce::AudioIODeviceCallbackContext& context)
{
    juce::ignoreUnused (outputChannelData, numOutputChannels, context);
    takeRecorderOnAudioThread = true;

    const auto cap = latchCapacity.load (std::memory_order_relaxed);
    const auto frames = juce::jlimit (0, cap, numSamples);
    latchedSamples = frames;

    if (frames <= 0)
        return;

    const auto copyOrClear = [frames] (std::vector<float>& dest, const float* source)
    {
        if (source != nullptr)
            juce::FloatVectorOperations::copy (dest.data(), source, frames);
        else
            juce::FloatVectorOperations::clear (dest.data(), frames);
    };

    const float* mic = numInputChannels > 0 && inputChannelData != nullptr ? inputChannelData[0] : nullptr;
    const float* inst = numInputChannels > 1 && inputChannelData != nullptr ? inputChannelData[1] : nullptr;
    copyOrClear (micLatch, mic);
    copyOrClear (instLatch, inst);
}

} // namespace jamstudio::audio
