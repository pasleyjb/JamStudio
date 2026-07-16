#include "MultiBusMaster.h"

namespace jamstudio::audio
{

namespace
{
juce::File monitorSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("output-monitor.json");
}
} // namespace

MultiBusMaster::MultiBusMaster (StemMixer& stems, Metronome& metro, StageMediaPlayer& stage)
    : stemMixer (stems),
      metronome (metro),
      stageMedia (stage)
{
    for (auto& m : busMeter)
        m.store (0.0f, std::memory_order_relaxed);
    loadSettings();
}

void MultiBusMaster::loadSettings()
{
    const auto file = monitorSettingsFile();
    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* o = parsed.getDynamicObject())
    {
        setOutputMonitorSelect (outputMonitorSelectFromString (
            o->getProperty ("monitorSelect").toString()));
        if (o->hasProperty ("stereoFoldListen"))
            setStereoFoldListen (static_cast<bool> (o->getProperty ("stereoFoldListen")));

        if (auto* names = o->getProperty ("busNames").getArray())
        {
            const juce::ScopedLock sl (labelLock);
            for (int b = 0; b < kNumMixBuses && b < names->size(); ++b)
            {
                const auto n = names->getUnchecked (b).toString().trim();
                busDisplayNames[static_cast<size_t> (b)] =
                    (n.isEmpty() || n == mixBusName (static_cast<MixBus> (b)))
                        ? juce::String()
                        : n;
            }
        }
    }
}

void MultiBusMaster::saveSettings() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("monitorSelect", outputMonitorSelectToString (getOutputMonitorSelect()));
    o->setProperty ("stereoFoldListen", isStereoFoldListen());

    juce::Array<juce::var> names;
    {
        const juce::ScopedLock sl (labelLock);
        for (int b = 0; b < kNumMixBuses; ++b)
        {
            const auto& custom = busDisplayNames[static_cast<size_t> (b)];
            names.add (custom.isNotEmpty() ? custom : mixBusName (static_cast<MixBus> (b)));
        }
    }
    o->setProperty ("busNames", juce::var (names));

    const auto file = monitorSettingsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

void MultiBusMaster::setBusDisplayName (const MixBus bus, juce::String name)
{
    const auto i = static_cast<int> (bus);
    if (! juce::isPositiveAndBelow (i, kNumMixBuses))
        return;

    name = name.trim();
    // Treat default short names as "no custom label"
    if (name.equalsIgnoreCase (mixBusName (bus))
        || name.equalsIgnoreCase (mixBusLongName (bus)))
        name = {};

    {
        const juce::ScopedLock sl (labelLock);
        busDisplayNames[static_cast<size_t> (i)] = std::move (name);
    }
    saveSettings();
}

juce::String MultiBusMaster::getBusDisplayName (const MixBus bus) const
{
    const auto i = static_cast<int> (bus);
    if (! juce::isPositiveAndBelow (i, kNumMixBuses))
        return "Bus";

    {
        const juce::ScopedLock sl (labelLock);
        const auto& custom = busDisplayNames[static_cast<size_t> (i)];
        if (custom.isNotEmpty())
            return custom;
    }
    return mixBusName (bus);
}

juce::String MultiBusMaster::getBusLongDisplayName (const MixBus bus) const
{
    const auto i = static_cast<int> (bus);
    if (! juce::isPositiveAndBelow (i, kNumMixBuses))
        return "Bus";

    juce::String custom;
    {
        const juce::ScopedLock sl (labelLock);
        custom = busDisplayNames[static_cast<size_t> (i)];
    }

    if (custom.isEmpty())
        return mixBusLongName (bus);

    // e.g. "Jay IEM (Monitor / IEM 1)" so routing role stays obvious
    return custom + " (" + mixBusLongName (bus) + ")";
}

juce::String MultiBusMaster::getOutputMonitorSelectDisplayName (const OutputMonitorSelect s) const
{
    if (s == OutputMonitorSelect::sumAll)
        return "Sum all buses";
    if (s == OutputMonitorSelect::foh)
        return getBusDisplayName (MixBus::foh) + " (" + mixBusHardwareOuts (MixBus::foh) + ")";
    const auto bus = static_cast<MixBus> (static_cast<int> (s));
    return getBusDisplayName (bus) + " (" + mixBusHardwareOuts (bus) + ")";
}

void MultiBusMaster::setClickBusSend (const MixBus bus, const float gain) noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        clickSend[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, gain);
}

float MultiBusMaster::getClickBusSend (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    return juce::isPositiveAndBelow (i, kNumMixBuses) ? clickSend[static_cast<size_t> (i)] : 0.0f;
}

void MultiBusMaster::setStageBusSend (const MixBus bus, const float gain) noexcept
{
    const auto i = static_cast<int> (bus);
    if (juce::isPositiveAndBelow (i, kNumMixBuses))
        stageSend[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, gain);
}

float MultiBusMaster::getStageBusSend (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    return juce::isPositiveAndBelow (i, kNumMixBuses) ? stageSend[static_cast<size_t> (i)] : 0.0f;
}

void MultiBusMaster::setOutputMonitorSelect (const OutputMonitorSelect select) noexcept
{
    monitorSelect.store (static_cast<int> (select), std::memory_order_relaxed);
    // Persist off the audio thread — best-effort; UI also calls saveSettings.
}

OutputMonitorSelect MultiBusMaster::getOutputMonitorSelect() const noexcept
{
    return static_cast<OutputMonitorSelect> (monitorSelect.load (std::memory_order_relaxed));
}

void MultiBusMaster::setStereoFoldListen (const bool shouldFold) noexcept
{
    stereoFoldListen.store (shouldFold, std::memory_order_relaxed);
}

bool MultiBusMaster::isStereoFoldListen() const noexcept
{
    return stereoFoldListen.load (std::memory_order_relaxed);
}

float MultiBusMaster::getBusMeterLevel (const MixBus bus) const noexcept
{
    const auto i = static_cast<int> (bus);
    return juce::isPositiveAndBelow (i, kNumMixBuses)
               ? busMeter[static_cast<size_t> (i)].load (std::memory_order_relaxed)
               : 0.0f;
}

void MultiBusMaster::prepareToPlay (const int samplesPerBlockExpected, const double sampleRate)
{
    stemMixer.prepareToPlay (samplesPerBlockExpected, sampleRate);
    metronome.prepareToPlay (samplesPerBlockExpected, sampleRate);
    stageMedia.prepareToPlay (samplesPerBlockExpected, sampleRate);
    const auto n = juce::jmax (samplesPerBlockExpected, 512);
    auxScratch.setSize (2, n, false, true, true);
    busScratch.setSize (kMaxMixChannels, n, false, true, true);
}

void MultiBusMaster::releaseResources()
{
    stemMixer.releaseResources();
    metronome.releaseResources();
    stageMedia.releaseResources();
    auxScratch.setSize (0, 0);
    busScratch.setSize (0, 0);
}

void MultiBusMaster::addSourceToBuses (const juce::AudioBuffer<float>& source,
                                       const int startSample,
                                       const int numSamples,
                                       juce::AudioBuffer<float>& dest,
                                       const int destStart,
                                       const std::array<float, kNumMixBuses>& sends)
{
    // Always target the full bus matrix when dest is wide enough.
    const auto outCh = dest.getNumChannels();
    const auto availableBuses = juce::jlimit (1, kNumMixBuses, outCh / kChannelsPerBus);
    const auto srcCh = source.getNumChannels();

    for (int b = 0; b < availableBuses; ++b)
    {
        const auto gain = sends[static_cast<size_t> (b)];
        if (gain <= 0.0001f)
            continue;

        const int base = mixBusOutputOffset (static_cast<MixBus> (b));
        if (base < outCh)
            dest.addFrom (base, destStart, source, 0, startSample, numSamples, gain);
        if (base + 1 < outCh)
            dest.addFrom (base + 1, destStart, source,
                          juce::jmin (1, srcCh - 1), startSample, numSamples, gain);
    }
}

void MultiBusMaster::updateBusMeters (const juce::AudioBuffer<float>& busBuffer,
                                      const int numSamples) noexcept
{
    for (int b = 0; b < kNumMixBuses; ++b)
    {
        const int base = mixBusOutputOffset (static_cast<MixBus> (b));
        float peak = 0.0f;
        if (base < busBuffer.getNumChannels())
            peak = juce::jmax (peak, busBuffer.getMagnitude (base, 0, numSamples));
        if (base + 1 < busBuffer.getNumChannels())
            peak = juce::jmax (peak, busBuffer.getMagnitude (base + 1, 0, numSamples));

        auto& m = busMeter[static_cast<size_t> (b)];
        const auto prev = m.load (std::memory_order_relaxed);
        m.store (peak >= prev ? peak : prev * 0.85f, std::memory_order_relaxed);
    }
}

void MultiBusMaster::foldMonitorToStereo (const juce::AudioBuffer<float>& busBuffer,
                                          juce::AudioBuffer<float>& dest,
                                          const int destStart,
                                          const int numSamples) const
{
    const auto outCh = dest.getNumChannels();
    if (outCh < 1)
        return;

    const auto select = static_cast<OutputMonitorSelect> (
        monitorSelect.load (std::memory_order_relaxed));

    // Clear device buffer first.
    for (int c = 0; c < outCh; ++c)
        dest.clear (c, destStart, numSamples);

    auto copyPair = [&] (const int busBase)
    {
        if (busBase < busBuffer.getNumChannels())
            dest.copyFrom (0, destStart, busBuffer, busBase, 0, numSamples);
        if (outCh > 1 && busBase + 1 < busBuffer.getNumChannels())
            dest.copyFrom (1, destStart, busBuffer, busBase + 1, 0, numSamples);
        else if (outCh > 1 && busBase < busBuffer.getNumChannels())
            dest.copyFrom (1, destStart, busBuffer, busBase, 0, numSamples); // mono → both
    };

    if (select == OutputMonitorSelect::sumAll)
    {
        for (int b = 0; b < kNumMixBuses; ++b)
        {
            const int base = mixBusOutputOffset (static_cast<MixBus> (b));
            if (base < busBuffer.getNumChannels())
                dest.addFrom (0, destStart, busBuffer, base, 0, numSamples, 0.7f);
            if (outCh > 1 && base + 1 < busBuffer.getNumChannels())
                dest.addFrom (1, destStart, busBuffer, base + 1, 0, numSamples, 0.7f);
        }
        return;
    }

    auto busIndex = static_cast<int> (select);
    if (busIndex < 0 || busIndex >= kNumMixBuses)
        busIndex = 0;
    copyPair (mixBusOutputOffset (static_cast<MixBus> (busIndex)));
}

void MultiBusMaster::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    const auto numSamples = bufferToFill.numSamples;
    auto* deviceBuf = bufferToFill.buffer;
    const auto outCh = deviceBuf->getNumChannels();
    const auto start = bufferToFill.startSample;

    if (busScratch.getNumSamples() < numSamples || busScratch.getNumChannels() < kMaxMixChannels)
        busScratch.setSize (kMaxMixChannels, numSamples, false, false, true);

    // ---- Full FOH + Mon1–5 matrix (independent of device width) ----
    busScratch.clear();
    {
        juce::AudioSourceChannelInfo busInfo (&busScratch, 0, numSamples);
        stemMixer.getNextAudioBlock (busInfo);
    }

    if (auxScratch.getNumSamples() < numSamples)
        auxScratch.setSize (2, numSamples, false, false, true);

    {
        juce::AudioSourceChannelInfo info (&auxScratch, 0, numSamples);
        metronome.getNextAudioBlock (info);
        addSourceToBuses (auxScratch, 0, numSamples, busScratch, 0, clickSend);
    }

    {
        juce::AudioSourceChannelInfo info (&auxScratch, 0, numSamples);
        stageMedia.getNextAudioBlock (info);
        addSourceToBuses (auxScratch, 0, numSamples, busScratch, 0, stageSend);
    }

    updateBusMeters (busScratch, numSamples);

    // ---- Device mapping ----
    // Fold when user asks, or when the device is only stereo (can't carry multi mon).
    const bool fold = stereoFoldListen.load (std::memory_order_relaxed) || outCh < 4;

    if (! fold)
    {
        // Matrix to hardware: as many stereo pairs as the device exposes (FOH first).
        const int copyCh = juce::jmin (kMaxMixChannels, outCh);
        for (int c = 0; c < copyCh; ++c)
            deviceBuf->copyFrom (c, start, busScratch, c, 0, numSamples);
        for (int c = copyCh; c < outCh; ++c)
            deviceBuf->clear (c, start, numSamples);
        return;
    }

    // Stereo / PC / virtual interface: only the selected bus (or sum) reaches speakers
    foldMonitorToStereo (busScratch, *deviceBuf, start, numSamples);
}

} // namespace jamstudio::audio
