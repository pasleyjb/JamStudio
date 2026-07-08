#include "TempoDetector.h"

#include <cmath>
#include <vector>

namespace jamstudio::audio
{

namespace
{
constexpr double minBpm = 60.0;
constexpr double maxBpm = 200.0;
constexpr double analysisSeconds = 30.0;
constexpr int envelopeSampleRate = 100;

void buildMonoEnvelope (const juce::AudioBuffer<float>& buffer,
                        const double sourceSampleRate,
                        std::vector<float>& envelope)
{
    const auto hop = juce::jmax (1, static_cast<int> (sourceSampleRate / static_cast<double> (envelopeSampleRate)));
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();
    envelope.clear();
    envelope.reserve (static_cast<size_t> (numSamples / hop + 1));

    for (int position = 0; position < numSamples; position += hop)
    {
        auto sum = 0.0f;
        const auto block = juce::jmin (hop, numSamples - position);

        for (int sample = 0; sample < block; ++sample)
        {
            auto mixed = 0.0f;

            for (int channel = 0; channel < numChannels; ++channel)
                mixed += buffer.getSample (channel, position + sample);

            sum += std::abs (mixed / static_cast<float> (numChannels));
        }

        envelope.push_back (sum / static_cast<float> (block));
    }
}

void emphasiseOnsets (std::vector<float>& envelope)
{
    if (envelope.size() < 2)
        return;

    std::vector<float> onset (envelope.size(), 0.0f);

    for (size_t i = 1; i < envelope.size(); ++i)
        onset[i] = juce::jmax (0.0f, envelope[i] - envelope[i - 1]);

    envelope = std::move (onset);
}

double estimateBpmFromEnvelope (const std::vector<float>& envelope)
{
    if (envelope.size() < static_cast<size_t> (envelopeSampleRate * 2))
        return 0.0;

    const auto minLag = static_cast<int> (std::floor (envelopeSampleRate * 60.0 / maxBpm));
    const auto maxLag = static_cast<int> (std::ceil (envelopeSampleRate * 60.0 / minBpm));

    auto bestLag = 0;
    auto bestScore = 0.0;

    for (int lag = minLag; lag <= maxLag; ++lag)
    {
        auto score = 0.0;

        for (size_t i = 0; i + static_cast<size_t> (lag) < envelope.size(); ++i)
            score += static_cast<double> (envelope[i] * envelope[i + static_cast<size_t> (lag)]);

        if (score > bestScore)
        {
            bestScore = score;
            bestLag = lag;
        }
    }

    if (bestLag <= 0 || bestScore <= 0.0)
        return 0.0;

    auto bpm = 60.0 * static_cast<double> (envelopeSampleRate) / static_cast<double> (bestLag);

    while (bpm < minBpm)
        bpm *= 2.0;

    while (bpm > maxBpm)
        bpm *= 0.5;

    return juce::jlimit (minBpm, maxBpm, bpm);
}
} // namespace

bool TempoDetector::detectFromFile (const juce::File& audioFile,
                                    juce::AudioFormatManager& formatManager,
                                    double& bpmOut)
{
    bpmOut = 0.0;

    if (! audioFile.existsAsFile())
        return false;

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (audioFile));

    if (reader == nullptr)
        return false;

    const auto samplesToRead = juce::jmin (reader->lengthInSamples,
                                           static_cast<int64> (reader->sampleRate * analysisSeconds));

    if (samplesToRead <= 0)
        return false;

    juce::AudioBuffer<float> buffer (static_cast<int> (reader->numChannels),
                                     static_cast<int> (samplesToRead));

    if (! reader->read (&buffer, 0, static_cast<int> (samplesToRead), 0, true, true))
        return false;

    std::vector<float> envelope;
    buildMonoEnvelope (buffer, reader->sampleRate, envelope);
    emphasiseOnsets (envelope);

    const auto detected = estimateBpmFromEnvelope (envelope);

    if (detected <= 0.0)
        return false;

    bpmOut = std::round (detected);
    return true;
}

} // namespace jamstudio::audio