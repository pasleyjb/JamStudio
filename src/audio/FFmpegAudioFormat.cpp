#include "FFmpegAudioFormat.h"

#include <atomic>
#include <cmath>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/frame.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

namespace jamstudio::audio
{

namespace
{
constexpr int kOutChannels = 2;

// ---------- Audio reader ----------------------------------------------------

class FFmpegAudioFormatReader : public juce::AudioFormatReader
{
public:
    explicit FFmpegAudioFormatReader (const juce::File& file)
        : AudioFormatReader (nullptr, "FFmpeg")
    {
        inputName = file.getFullPathName();

        if (avformat_open_input (&format, file.getFullPathName().toRawUTF8(), nullptr, nullptr) < 0)
            return;

        if (avformat_find_stream_info (format, nullptr) < 0)
            return;

        const AVCodec* codec = nullptr;
        audioStream = av_find_best_stream (format, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);

        if (audioStream < 0 || codec == nullptr)
            return;

        auto* stream = format->streams[audioStream];
        codecCtx = avcodec_alloc_context3 (codec);

        if (codecCtx == nullptr)
            return;

        if (avcodec_parameters_to_context (codecCtx, stream->codecpar) < 0)
            return;

        if (avcodec_open2 (codecCtx, codec, nullptr) < 0)
            return;

        if (codecCtx->ch_layout.nb_channels <= 0)
            av_channel_layout_default (&codecCtx->ch_layout,
                                       juce::jmax (1, codecCtx->ch_layout.nb_channels > 0
                                                           ? codecCtx->ch_layout.nb_channels
                                                           : 2));

        if (codecCtx->ch_layout.nb_channels <= 0)
            av_channel_layout_default (&codecCtx->ch_layout, 2);

        packet = av_packet_alloc();
        frame = av_frame_alloc();

        if (packet == nullptr || frame == nullptr)
            return;

        AVChannelLayout outLayout {};
        av_channel_layout_default (&outLayout, kOutChannels);

        const int inRate = codecCtx->sample_rate > 0 ? codecCtx->sample_rate : 44100;

        if (swr_alloc_set_opts2 (&swr,
                                 &outLayout,
                                 AV_SAMPLE_FMT_FLT,
                                 inRate,
                                 &codecCtx->ch_layout,
                                 codecCtx->sample_fmt,
                                 inRate,
                                 0,
                                 nullptr)
            < 0)
        {
            av_channel_layout_uninit (&outLayout);
            return;
        }

        av_channel_layout_uninit (&outLayout);

        if (swr_init (swr) < 0)
            return;

        sampleRate = codecCtx->sample_rate > 0 ? static_cast<double> (codecCtx->sample_rate) : 44100.0;
        bitsPerSample = 32;
        usesFloatingPointData = true;
        numChannels = static_cast<unsigned int> (kOutChannels);

        if (stream->duration > 0 && stream->time_base.den > 0)
        {
            const auto seconds = av_q2d (stream->time_base) * static_cast<double> (stream->duration);
            lengthInSamples = static_cast<juce::int64> (seconds * sampleRate);
        }
        else if (format->duration > 0)
        {
            const auto seconds = static_cast<double> (format->duration) / static_cast<double> (AV_TIME_BASE);
            lengthInSamples = static_cast<juce::int64> (seconds * sampleRate);
        }
        else
        {
            lengthInSamples = static_cast<juce::int64> (sampleRate * 3600.0); // unknown: allow long scrub
        }

        timeBase = stream->time_base;
        opened = true;
        nextReadSample = 0;
        fifoWriteSample = 0;
    }

    ~FFmpegAudioFormatReader() override
    {
        if (swr != nullptr)
            swr_free (&swr);

        if (frame != nullptr)
            av_frame_free (&frame);

        if (packet != nullptr)
            av_packet_free (&packet);

        if (codecCtx != nullptr)
            avcodec_free_context (&codecCtx);

        if (format != nullptr)
            avformat_close_input (&format);
    }

    [[nodiscard]] bool isOpen() const noexcept { return opened; }

    bool readSamples (int* const* destSamples,
                      int numDestChannels,
                      int startOffsetInDestBuffer,
                      juce::int64 startSampleInFile,
                      int numSamples) override
    {
        if (! opened || destSamples == nullptr || numSamples <= 0)
            return false;

        clearSamplesBeyondAvailableLength (destSamples, numDestChannels, startOffsetInDestBuffer,
                                           startSampleInFile, numSamples, lengthInSamples);

        if (startSampleInFile != nextReadSample)
        {
            if (! seekToSample (startSampleInFile))
                return false;
        }

        int written = 0;

        while (written < numSamples)
        {
            if (! ensureFifoHas (1))
            {
                // EOF / decode stall — zero the rest
                for (int ch = 0; ch < numDestChannels; ++ch)
                    if (destSamples[ch] != nullptr)
                        juce::FloatVectorOperations::clear (
                            reinterpret_cast<float*> (destSamples[ch]) + startOffsetInDestBuffer + written,
                            numSamples - written);
                nextReadSample = startSampleInFile + numSamples;
                return true;
            }

            const auto available = static_cast<int> (fifo.size() / kOutChannels);
            const auto toCopy = juce::jmin (numSamples - written, available);

            for (int i = 0; i < toCopy; ++i)
            {
                for (int ch = 0; ch < numDestChannels; ++ch)
                {
                    if (destSamples[ch] == nullptr)
                        continue;

                    const auto srcCh = juce::jmin (ch, kOutChannels - 1);
                    const float sample = fifo[static_cast<size_t> (i * kOutChannels + srcCh)];
                    reinterpret_cast<float*> (destSamples[ch])[startOffsetInDestBuffer + written + i] = sample;
                }
            }

            fifo.erase (fifo.begin(), fifo.begin() + static_cast<std::ptrdiff_t> (toCopy * kOutChannels));
            written += toCopy;
            nextReadSample += toCopy;
            fifoWriteSample += toCopy;
        }

        return true;
    }

private:
    bool seekToSample (const juce::int64 sample)
    {
        if (format == nullptr || audioStream < 0)
            return false;

        const auto seconds = static_cast<double> (sample) / sampleRate;
        const auto ts = static_cast<int64_t> (seconds / av_q2d (timeBase));

        if (av_seek_frame (format, audioStream, ts, AVSEEK_FLAG_BACKWARD) < 0)
        {
            // try global seek
            const auto gts = static_cast<int64_t> (seconds * AV_TIME_BASE);
            if (av_seek_frame (format, -1, gts, AVSEEK_FLAG_BACKWARD) < 0)
                return false;
        }

        avcodec_flush_buffers (codecCtx);
        if (swr != nullptr)
            swr_convert (swr, nullptr, 0, nullptr, 0);

        fifo.clear();
        nextReadSample = sample;
        fifoWriteSample = sample;
        drained = false;
        return true;
    }

    bool ensureFifoHas (const int minFrames)
    {
        while (static_cast<int> (fifo.size() / kOutChannels) < minFrames)
        {
            if (drained)
                return static_cast<int> (fifo.size() / kOutChannels) > 0;

            if (! decodeMore())
            {
                drained = true;
                return static_cast<int> (fifo.size() / kOutChannels) > 0;
            }
        }

        return true;
    }

    bool decodeMore()
    {
        if (format == nullptr || codecCtx == nullptr)
            return false;

        while (true)
        {
            const int ret = av_read_frame (format, packet);

            if (ret < 0)
            {
                // flush decoder
                avcodec_send_packet (codecCtx, nullptr);
                return receiveAndConvert();
            }

            if (packet->stream_index != audioStream)
            {
                av_packet_unref (packet);
                continue;
            }

            const int send = avcodec_send_packet (codecCtx, packet);
            av_packet_unref (packet);

            if (send < 0 && send != AVERROR (EAGAIN))
                return false;

            if (receiveAndConvert())
                return true;
        }
    }

    bool receiveAndConvert()
    {
        bool gotAny = false;

        while (true)
        {
            const int rec = avcodec_receive_frame (codecCtx, frame);

            if (rec == AVERROR (EAGAIN) || rec == AVERROR_EOF)
                break;

            if (rec < 0)
                break;

            const int outSamples = swr_get_out_samples (swr, frame->nb_samples);
            if (outSamples <= 0)
            {
                av_frame_unref (frame);
                continue;
            }

            std::vector<float> temp (static_cast<size_t> (outSamples * kOutChannels));
            uint8_t* outPlanes[1] { reinterpret_cast<uint8_t*> (temp.data()) };

            const int converted = swr_convert (swr,
                                               outPlanes,
                                               outSamples,
                                               const_cast<const uint8_t**> (frame->extended_data),
                                               frame->nb_samples);

            av_frame_unref (frame);

            if (converted > 0)
            {
                fifo.insert (fifo.end(),
                             temp.begin(),
                             temp.begin() + static_cast<std::ptrdiff_t> (converted * kOutChannels));
                gotAny = true;
            }
        }

        return gotAny;
    }

    AVFormatContext* format = nullptr;
    AVCodecContext* codecCtx = nullptr;
    AVPacket* packet = nullptr;
    AVFrame* frame = nullptr;
    SwrContext* swr = nullptr;
    int audioStream = -1;
    AVRational timeBase { 1, 1 };
    bool opened = false;
    bool drained = false;
    juce::int64 nextReadSample = 0;
    juce::int64 fifoWriteSample = 0;
    std::vector<float> fifo;
    juce::String inputName;
};

// Max long edge for stage display frames (keeps UI light during performance).
constexpr int kMaxDisplayLongEdge = 960;
// Target stage video refresh (worker aims for this; UI may be lower).
constexpr int kTargetDisplayFps = 20;

void chooseDisplaySize (const int srcW, const int srcH, int& outW, int& outH)
{
    if (srcW <= 0 || srcH <= 0)
    {
        outW = outH = 0;
        return;
    }

    const int longEdge = juce::jmax (srcW, srcH);
    if (longEdge <= kMaxDisplayLongEdge)
    {
        outW = srcW;
        outH = srcH;
        return;
    }

    const double scale = static_cast<double> (kMaxDisplayLongEdge) / static_cast<double> (longEdge);
    outW = juce::jmax (2, static_cast<int> (srcW * scale) & ~1);
    outH = juce::jmax (2, static_cast<int> (srcH * scale) & ~1);
}

/** Nearest-neighbour scaled YUV420 -> ARGB (no libswscale). */
void yuv420ToArgbScaled (const AVFrame* src, juce::Image& dest, const int outW, const int outH)
{
    if (outW <= 0 || outH <= 0)
        return;

    if (! dest.isValid() || dest.getWidth() != outW || dest.getHeight() != outH
        || dest.getFormat() != juce::Image::ARGB)
        dest = juce::Image (juce::Image::ARGB, outW, outH, false);

    juce::Image::BitmapData bd (dest, juce::Image::BitmapData::writeOnly);
    const auto* yPlane = src->data[0];
    const auto* uPlane = src->data[1];
    const auto* vPlane = src->data[2];
    const int yStride = src->linesize[0];
    const int uStride = src->linesize[1];
    const int vStride = src->linesize[2];
    const int srcW = src->width;
    const int srcH = src->height;
    auto clip = [] (int v) { return static_cast<uint8_t> (juce::jlimit (0, 255, v)); };

    for (int y = 0; y < outH; ++y)
    {
        const int sy = juce::jlimit (0, srcH - 1, (y * srcH) / outH);
        auto* row = bd.getLinePointer (y);
        const auto* yRow = yPlane + sy * yStride;
        const auto* uRow = uPlane + (sy / 2) * uStride;
        const auto* vRow = vPlane + (sy / 2) * vStride;

        for (int x = 0; x < outW; ++x)
        {
            const int sx = juce::jlimit (0, srcW - 1, (x * srcW) / outW);
            const int C = static_cast<int> (yRow[sx]) - 16;
            const int D = static_cast<int> (uRow[sx / 2]) - 128;
            const int E = static_cast<int> (vRow[sx / 2]) - 128;
            const int R = clip ((298 * C + 409 * E + 128) >> 8);
            const int G = clip ((298 * C - 100 * D - 208 * E + 128) >> 8);
            const int B = clip ((298 * C + 516 * D + 128) >> 8);
            row[x * 4 + 0] = B;
            row[x * 4 + 1] = G;
            row[x * 4 + 2] = R;
            row[x * 4 + 3] = 255;
        }
    }
}

void nv12ToArgbScaled (const AVFrame* src, juce::Image& dest, const int outW, const int outH)
{
    if (outW <= 0 || outH <= 0)
        return;

    if (! dest.isValid() || dest.getWidth() != outW || dest.getHeight() != outH
        || dest.getFormat() != juce::Image::ARGB)
        dest = juce::Image (juce::Image::ARGB, outW, outH, false);

    juce::Image::BitmapData bd (dest, juce::Image::BitmapData::writeOnly);
    const auto* yPlane = src->data[0];
    const auto* uvPlane = src->data[1];
    const int yStride = src->linesize[0];
    const int uvStride = src->linesize[1];
    const int srcW = src->width;
    const int srcH = src->height;
    auto clip = [] (int v) { return static_cast<uint8_t> (juce::jlimit (0, 255, v)); };

    for (int y = 0; y < outH; ++y)
    {
        const int sy = juce::jlimit (0, srcH - 1, (y * srcH) / outH);
        auto* row = bd.getLinePointer (y);
        const auto* yRow = yPlane + sy * yStride;
        const auto* uvRow = uvPlane + (sy / 2) * uvStride;

        for (int x = 0; x < outW; ++x)
        {
            const int sx = juce::jlimit (0, srcW - 1, (x * srcW) / outW);
            const int C = static_cast<int> (yRow[sx]) - 16;
            const int D = static_cast<int> (uvRow[(sx & ~1) + 0]) - 128;
            const int E = static_cast<int> (uvRow[(sx & ~1) + 1]) - 128;
            const int R = clip ((298 * C + 409 * E + 128) >> 8);
            const int G = clip ((298 * C - 100 * D - 208 * E + 128) >> 8);
            const int B = clip ((298 * C + 516 * D + 128) >> 8);
            row[x * 4 + 0] = B;
            row[x * 4 + 1] = G;
            row[x * 4 + 2] = R;
            row[x * 4 + 3] = 255;
        }
    }
}

} // namespace

bool isFFmpegMediaAvailable() noexcept
{
    return true;
}

juce::String getFFmpegMediaSupportSummary()
{
    return "FFmpeg MPEG codecs enabled (MP3/AAC/MP4/MOV/MKV/MPEG/WEBM + stage video frames).";
}

FFmpegAudioFormat::FFmpegAudioFormat()
    : AudioFormat ("FFmpeg MPEG Media",
                   { ".mp3", ".mp2", ".mp4", ".m4a", ".m4v", ".aac", ".mov", ".mkv",
                     ".webm", ".mpeg", ".mpg", ".m2v", ".m2ts", ".mts", ".ts", ".vob",
                     ".avi", ".wmv", ".flv", ".3gp", ".opus", ".wma", ".ac3", ".eac3" })
{
}

juce::Array<int> FFmpegAudioFormat::getPossibleSampleRates()
{
    return { 8000, 11025, 16000, 22050, 32000, 44100, 48000, 88200, 96000 };
}

juce::Array<int> FFmpegAudioFormat::getPossibleBitDepths()
{
    return { 16, 24, 32 };
}

juce::AudioFormatReader* FFmpegAudioFormat::createReaderFor (juce::InputStream* sourceStream,
                                                             const bool deleteStreamIfOpeningFails)
{
    // FFmpeg path-based open is more reliable than streaming arbitrary InputStreams.
    // Try to resolve a FileInputStream back to a path.
    if (auto* fis = dynamic_cast<juce::FileInputStream*> (sourceStream))
    {
        auto* reader = createReaderForFile (fis->getFile());

        if (reader != nullptr)
        {
            if (deleteStreamIfOpeningFails)
                delete sourceStream;
            return reader;
        }
    }

    if (deleteStreamIfOpeningFails)
        delete sourceStream;

    return nullptr;
}

std::unique_ptr<juce::AudioFormatWriter> FFmpegAudioFormat::createWriterFor (
    std::unique_ptr<juce::OutputStream>&,
    const juce::AudioFormatWriterOptions&)
{
    return nullptr; // decode-only
}

juce::AudioFormatReader* FFmpegAudioFormat::createReaderForFile (const juce::File& file)
{
    if (! file.existsAsFile())
        return nullptr;

    auto reader = std::make_unique<FFmpegAudioFormatReader> (file);

    if (! reader->isOpen())
        return nullptr;

    return reader.release();
}

namespace
{
/** Zero-audio reader used for video-only stage clips. */
class SilentAudioFormatReader : public juce::AudioFormatReader
{
public:
    SilentAudioFormatReader (const double durationSeconds, const double rate)
        : AudioFormatReader (nullptr, "Silent")
    {
        sampleRate = rate > 0.0 ? rate : 48000.0;
        bitsPerSample = 32;
        usesFloatingPointData = true;
        numChannels = 2;
        lengthInSamples = juce::jmax<juce::int64> (
            1, static_cast<juce::int64> (juce::jmax (0.1, durationSeconds) * sampleRate));
    }

    bool readSamples (int* const* destSamples,
                      int numDestChannels,
                      int startOffsetInDestBuffer,
                      juce::int64 /*startSampleInFile*/,
                      int numSamples) override
    {
        if (destSamples == nullptr || numSamples <= 0)
            return false;

        for (int ch = 0; ch < numDestChannels; ++ch)
            if (destSamples[ch] != nullptr)
                juce::FloatVectorOperations::clear (
                    reinterpret_cast<float*> (destSamples[ch]) + startOffsetInDestBuffer,
                    numSamples);

        return true;
    }
};
} // namespace

juce::AudioFormatReader* FFmpegAudioFormat::createSilentReader (const double durationSeconds,
                                                                const double sampleRate)
{
    return new SilentAudioFormatReader (durationSeconds, sampleRate);
}

bool probeFFmpegMedia (const juce::File& file, FFmpegMediaInfo& info)
{
    info = {};

    if (! file.existsAsFile())
    {
        info.error = "File does not exist.";
        return false;
    }

    AVFormatContext* fmt = nullptr;

    if (avformat_open_input (&fmt, file.getFullPathName().toRawUTF8(), nullptr, nullptr) < 0)
    {
        info.error = "FFmpeg could not open this file.";
        return false;
    }

    if (avformat_find_stream_info (fmt, nullptr) < 0)
    {
        avformat_close_input (&fmt);
        info.error = "FFmpeg could not read stream info.";
        return false;
    }

    if (fmt->duration > 0)
        info.durationSeconds = static_cast<double> (fmt->duration) / static_cast<double> (AV_TIME_BASE);

    for (unsigned i = 0; i < fmt->nb_streams; ++i)
    {
        auto* stream = fmt->streams[i];
        if (stream == nullptr || stream->codecpar == nullptr)
            continue;

        if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            info.hasAudio = true;
            if (const AVCodec* c = avcodec_find_decoder (stream->codecpar->codec_id))
                info.audioCodec = c->name;

            if (stream->duration > 0 && stream->time_base.den > 0)
                info.durationSeconds = juce::jmax (info.durationSeconds,
                                                   av_q2d (stream->time_base)
                                                       * static_cast<double> (stream->duration));
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO
                 && (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) == 0)
        {
            info.hasVideo = true;
            if (const AVCodec* c = avcodec_find_decoder (stream->codecpar->codec_id))
                info.videoCodec = c->name;

            if (stream->duration > 0 && stream->time_base.den > 0)
                info.durationSeconds = juce::jmax (info.durationSeconds,
                                                   av_q2d (stream->time_base)
                                                       * static_cast<double> (stream->duration));
        }
    }

    avformat_close_input (&fmt);

    if (! info.hasAudio && ! info.hasVideo)
    {
        info.error = "No audio or video streams found.";
        return false;
    }

    return true;
}

// ---------- Video decoder (background thread) -------------------------------

FFmpegVideoDecoder::FFmpegVideoDecoder()
    : juce::Thread ("StageFxVideoDecoder")
{
}

FFmpegVideoDecoder::~FFmpegVideoDecoder()
{
    close();
}

void FFmpegVideoDecoder::releaseMedia()
{
    if (frame != nullptr)
    {
        av_frame_free (reinterpret_cast<AVFrame**> (&frame));
        frame = nullptr;
    }

    if (packet != nullptr)
    {
        av_packet_free (reinterpret_cast<AVPacket**> (&packet));
        packet = nullptr;
    }

    if (codecContext != nullptr)
    {
        avcodec_free_context (reinterpret_cast<AVCodecContext**> (&codecContext));
        codecContext = nullptr;
    }

    if (formatContext != nullptr)
    {
        avformat_close_input (reinterpret_cast<AVFormatContext**> (&formatContext));
        formatContext = nullptr;
    }

    videoStreamIndex = -1;
    durationSeconds = 0.0;
    timeBaseSeconds = 0.0;
    lastPtsSeconds = -1.0;
    frameWidth = 0;
    frameHeight = 0;
    displayWidth = 0;
    displayHeight = 0;
    workImage = {};

    {
        const juce::ScopedLock sl (imageLock);
        publishedImage = {};
    }
}

void FFmpegVideoDecoder::close()
{
    signalThreadShouldExit();
    stopThread (2000);
    openFlag.store (false, std::memory_order_release);
    releaseMedia();
    frameSerial.store (0, std::memory_order_relaxed);
}

bool FFmpegVideoDecoder::open (const juce::File& file, juce::String& errorMessage)
{
    close();

    AVFormatContext* fmt = nullptr;

    if (avformat_open_input (&fmt, file.getFullPathName().toRawUTF8(), nullptr, nullptr) < 0)
    {
        errorMessage = "FFmpeg could not open media file.";
        return false;
    }

    if (avformat_find_stream_info (fmt, nullptr) < 0)
    {
        avformat_close_input (&fmt);
        errorMessage = "FFmpeg could not read stream info.";
        return false;
    }

    const AVCodec* codec = nullptr;
    const int streamIndex = av_find_best_stream (fmt, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);

    if (streamIndex < 0 || codec == nullptr)
    {
        avformat_close_input (&fmt);
        errorMessage = "No video stream in this file (audio-only is OK for mixer).";
        return false;
    }

    auto* stream = fmt->streams[streamIndex];
    AVCodecContext* ctx = avcodec_alloc_context3 (codec);

    // Prefer low-latency software decode when possible.
    if (ctx != nullptr)
        ctx->thread_count = juce::jlimit (1, 2, juce::SystemStats::getNumCpus());

    if (ctx == nullptr
        || avcodec_parameters_to_context (ctx, stream->codecpar) < 0
        || avcodec_open2 (ctx, codec, nullptr) < 0)
    {
        if (ctx != nullptr)
            avcodec_free_context (&ctx);
        avformat_close_input (&fmt);
        errorMessage = "Could not open MPEG video codec (" + juce::String (codec->name) + ").";
        return false;
    }

    AVPacket* pkt = av_packet_alloc();
    AVFrame* fr = av_frame_alloc();

    if (pkt == nullptr || fr == nullptr)
    {
        av_packet_free (&pkt);
        av_frame_free (&fr);
        avcodec_free_context (&ctx);
        avformat_close_input (&fmt);
        errorMessage = "Out of memory opening video decoder.";
        return false;
    }

    formatContext = fmt;
    codecContext = ctx;
    packet = pkt;
    frame = fr;
    videoStreamIndex = streamIndex;
    timeBaseSeconds = av_q2d (stream->time_base);
    frameWidth = ctx->width;
    frameHeight = ctx->height;
    chooseDisplaySize (frameWidth, frameHeight, displayWidth, displayHeight);

    if (stream->duration > 0)
        durationSeconds = timeBaseSeconds * static_cast<double> (stream->duration);
    else if (fmt->duration > 0)
        durationSeconds = static_cast<double> (fmt->duration) / static_cast<double> (AV_TIME_BASE);

    lastPtsSeconds = -1.0;
    targetSeconds.store (0.0, std::memory_order_relaxed);
    openFlag.store (true, std::memory_order_release);
    startThread (juce::Thread::Priority::low);
    return true;
}

void FFmpegVideoDecoder::setTargetTime (const double seconds) noexcept
{
    targetSeconds.store (juce::jmax (0.0, seconds), std::memory_order_relaxed);
}

juce::Image FFmpegVideoDecoder::getLatestFrame() const
{
    const juce::ScopedLock sl (imageLock);
    return publishedImage; // Image is ref-counted — cheap share, not a full pixel copy
}

juce::Image FFmpegVideoDecoder::getFrameAt (const double seconds)
{
    setTargetTime (seconds);
    return getLatestFrame();
}

void FFmpegVideoDecoder::flushDecoder()
{
    if (auto* ctx = static_cast<AVCodecContext*> (codecContext))
        avcodec_flush_buffers (ctx);

    lastPtsSeconds = -1.0;
}

bool FFmpegVideoDecoder::seekTo (const double seconds)
{
    auto* fmt = static_cast<AVFormatContext*> (formatContext);
    if (fmt == nullptr || videoStreamIndex < 0)
        return false;

    auto* stream = fmt->streams[videoStreamIndex];
    const auto ts = static_cast<int64_t> (seconds / juce::jmax (1.0e-9, av_q2d (stream->time_base)));
    if (av_seek_frame (fmt, videoStreamIndex, ts, AVSEEK_FLAG_BACKWARD) < 0)
    {
        const auto gts = static_cast<int64_t> (seconds * AV_TIME_BASE);
        if (av_seek_frame (fmt, -1, gts, AVSEEK_FLAG_BACKWARD) < 0)
            return false;
    }

    flushDecoder();
    return true;
}

bool FFmpegVideoDecoder::pullOneFrame (double& outPts)
{
    auto* fmt = static_cast<AVFormatContext*> (formatContext);
    auto* ctx = static_cast<AVCodecContext*> (codecContext);
    auto* pkt = static_cast<AVPacket*> (packet);
    auto* fr = static_cast<AVFrame*> (frame);

    if (fmt == nullptr || ctx == nullptr || pkt == nullptr || fr == nullptr)
        return false;

    // First try already-queued frames.
    if (avcodec_receive_frame (ctx, fr) == 0)
    {
        if (fr->best_effort_timestamp != AV_NOPTS_VALUE)
            outPts = static_cast<double> (fr->best_effort_timestamp) * timeBaseSeconds;
        else if (fr->pts != AV_NOPTS_VALUE)
            outPts = static_cast<double> (fr->pts) * timeBaseSeconds;
        else
            outPts = lastPtsSeconds;
        return true;
    }

    // Read packets until we get a video frame (bounded work per step).
    for (int attempts = 0; attempts < 48; ++attempts)
    {
        const int read = av_read_frame (fmt, pkt);
        if (read < 0)
            return false;

        if (pkt->stream_index != videoStreamIndex)
        {
            av_packet_unref (pkt);
            continue;
        }

        const int send = avcodec_send_packet (ctx, pkt);
        av_packet_unref (pkt);

        if (send < 0 && send != AVERROR (EAGAIN))
            return false;

        if (avcodec_receive_frame (ctx, fr) == 0)
        {
            if (fr->best_effort_timestamp != AV_NOPTS_VALUE)
                outPts = static_cast<double> (fr->best_effort_timestamp) * timeBaseSeconds;
            else if (fr->pts != AV_NOPTS_VALUE)
                outPts = static_cast<double> (fr->pts) * timeBaseSeconds;
            else
                outPts = lastPtsSeconds;
            return true;
        }
    }

    return false;
}

bool FFmpegVideoDecoder::convertLatestFrameToDisplay()
{
    auto* fr = static_cast<AVFrame*> (frame);
    if (fr == nullptr || fr->data[0] == nullptr)
        return false;

    switch (fr->format)
    {
        case AV_PIX_FMT_YUV420P:
        case AV_PIX_FMT_YUVJ420P:
            yuv420ToArgbScaled (fr, workImage, displayWidth, displayHeight);
            break;

        case AV_PIX_FMT_NV12:
            nv12ToArgbScaled (fr, workImage, displayWidth, displayHeight);
            break;

        case AV_PIX_FMT_RGB24:
        case AV_PIX_FMT_BGR24:
        {
            if (! workImage.isValid() || workImage.getWidth() != displayWidth
                || workImage.getHeight() != displayHeight)
                workImage = juce::Image (juce::Image::ARGB, displayWidth, displayHeight, false);

            juce::Image::BitmapData bd (workImage, juce::Image::BitmapData::writeOnly);
            const bool bgr = fr->format == AV_PIX_FMT_BGR24;
            const int srcW = fr->width;
            const int srcH = fr->height;

            for (int y = 0; y < displayHeight; ++y)
            {
                const int sy = juce::jlimit (0, srcH - 1, (y * srcH) / displayHeight);
                auto* dst = bd.getLinePointer (y);
                const auto* src = fr->data[0] + sy * fr->linesize[0];
                for (int x = 0; x < displayWidth; ++x)
                {
                    const int sx = juce::jlimit (0, srcW - 1, (x * srcW) / displayWidth);
                    const auto r = bgr ? src[sx * 3 + 2] : src[sx * 3 + 0];
                    const auto g = src[sx * 3 + 1];
                    const auto b = bgr ? src[sx * 3 + 0] : src[sx * 3 + 2];
                    dst[x * 4 + 0] = b;
                    dst[x * 4 + 1] = g;
                    dst[x * 4 + 2] = r;
                    dst[x * 4 + 3] = 255;
                }
            }
            break;
        }

        default:
            return false;
    }

    if (! workImage.isValid())
        return false;

    {
        const juce::ScopedLock sl (imageLock);
        publishedImage = workImage.createCopy(); // publish independent buffer for UI
    }
    frameSerial.fetch_add (1, std::memory_order_relaxed);
    return true;
}

void FFmpegVideoDecoder::decodeWorkerStep()
{
    if (! openFlag.load (std::memory_order_acquire))
        return;

    const double target = targetSeconds.load (std::memory_order_relaxed);

    // Seek if playhead jumped.
    if (lastPtsSeconds >= 0.0
        && (target + 0.08 < lastPtsSeconds || target > lastPtsSeconds + 1.5))
    {
        seekTo (target);
    }

    // Already at/past target — idle.
    if (lastPtsSeconds >= 0.0 && lastPtsSeconds >= target - 0.02)
        return;

    // Catch up: decode frames but only convert the last one we need.
    double pts = lastPtsSeconds;
    bool got = false;
    AVFrame* keep = nullptr;

    // Decode until at/past target, converting only once at the end.
    int framesDecoded = 0;
    while (framesDecoded < 12)
    {
        double newPts = pts;
        if (! pullOneFrame (newPts))
            break;

        pts = newPts;
        lastPtsSeconds = pts;
        got = true;
        ++framesDecoded;

        if (pts >= target - 0.02)
            break;
    }

    if (got)
    {
        juce::ignoreUnused (keep);
        convertLatestFrameToDisplay();
        // Frame still held in `frame` after convert — unref for next pull
        if (auto* fr = static_cast<AVFrame*> (frame))
            av_frame_unref (fr);
    }
}

void FFmpegVideoDecoder::run()
{
    const auto framePeriodMs = juce::jmax (5, 1000 / kTargetDisplayFps);

    while (! threadShouldExit())
    {
        const auto start = juce::Time::getMillisecondCounterHiRes();

        if (openFlag.load (std::memory_order_acquire))
            decodeWorkerStep();

        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - start;
        const auto sleepMs = juce::jmax (1, framePeriodMs - static_cast<int> (elapsed));
        wait (sleepMs);
    }
}

} // namespace jamstudio::audio
