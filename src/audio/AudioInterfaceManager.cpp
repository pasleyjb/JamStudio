#include "AudioInterfaceManager.h"
#include "MixBus.h"

#include <limits>
#include <signal.h>

namespace jamstudio::audio
{

namespace
{
constexpr int kDefaultMaxIn = 2;
constexpr int kDefaultMaxOut = kMaxMixChannels;

juce::String makeFingerprint (const juce::Array<AudioDeviceChoice>& ins,
                              const juce::Array<AudioDeviceChoice>& outs)
{
    juce::String s;
    for (const auto& d : ins)
        s << "I:" << d.typeName << "|" << d.name << "|" << d.maxInputChannels << ";";
    for (const auto& d : outs)
        s << "O:" << d.typeName << "|" << d.name << "|" << d.maxOutputChannels << ";";
    return s;
}

void enableFirstNChannels (juce::BigInteger& bits, const int n)
{
    bits.clear();
    for (int i = 0; i < n; ++i)
        bits.setBit (i);
}

AudioDeviceChoice probeDevice (juce::AudioIODeviceType& type,
                               const juce::String& outputName,
                               const juce::String& inputName)
{
    AudioDeviceChoice c;
    c.typeName = type.getTypeName();
    c.name = outputName.isNotEmpty() ? outputName : inputName;

    std::unique_ptr<juce::AudioIODevice> dev (type.createDevice (outputName, inputName));
    if (dev == nullptr)
        return c;

    c.maxInputChannels = dev->getInputChannelNames().size();
    c.maxOutputChannels = dev->getOutputChannelNames().size();
    if (inputName.isNotEmpty() && c.maxInputChannels <= 0)
        c.maxInputChannels = 1; // some drivers under-report until open
    if (outputName.isNotEmpty() && c.maxOutputChannels <= 0)
        c.maxOutputChannels = 2;
    return c;
}

constexpr const char* kPactl = "/usr/bin/pactl";
constexpr const char* kBusSink = "jamstudio_buses";
constexpr const char* kBusDeviceName = "JamStudio Mix Buses";
constexpr const char* kAsoundBegin = "# --- jamstudio buses begin ---";
constexpr const char* kAsoundEnd = "# --- jamstudio buses end ---";

juce::String runPactl (const juce::StringArray& args)
{
    juce::ChildProcess proc;
    if (! proc.start (args, juce::ChildProcess::wantStdOut | juce::ChildProcess::wantStdErr))
        return {};

    if (! proc.waitForProcessToFinish (2000))
        proc.kill();

    return proc.readAllProcessOutput();
}

void ensureBusSink()
{
    const auto sinks = runPactl ({ kPactl, "list", "short", "sinks" });
    if (sinks.contains (kBusSink))
        return;

    runPactl ({ kPactl, "load-module", "module-null-sink",
                "sink_name=" + juce::String (kBusSink),
                "rate=48000",
                "channels=6",
                "channel_map=front-left,front-right,rear-left,rear-right,front-center,lfe",
                "sink_properties=device.description=JamStudio-Buses" });
}

void ensureBusRemap (const juce::String& modules,
                     const char* sourceName,
                     const char* masterMap,
                     const char* description)
{
    const auto sourceArg = "source_name=" + juce::String (sourceName);

    for (const auto& line : juce::StringArray::fromLines (modules))
    {
        if (! line.contains ("module-remap-source") || ! line.contains (sourceArg))
            continue;

        // remix defaults to yes, which folds every bus into this pair and
        // the speakers get a stacked, clipped copy of the whole mix.
        if (line.contains ("remix=no"))
            return;

        const auto id = line.upToFirstOccurrenceOf ("\t", false, false).trim();
        if (id.isNotEmpty())
            runPactl ({ kPactl, "unload-module", id });
    }

    runPactl ({ kPactl, "load-module", "module-remap-source",
                "source_name=" + juce::String (sourceName),
                "master=" + juce::String (kBusSink) + ".monitor",
                "master_channel_map=" + juce::String (masterMap),
                "channel_map=front-left,front-right",
                "remix=no",
                "source_properties=device.description=" + juce::String (description) });
}

void writeBusAsoundrc()
{
    const auto file = juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                          .getChildFile (".asoundrc");
    auto text = file.existsAsFile() ? file.loadFileAsString() : juce::String();
    const juce::String block =
        juce::String (kAsoundBegin) + "\n"
        "pcm.jamstudio_buses {\n"
        "    type pipewire\n"
        "    playback_node \"jamstudio_buses\"\n"
        "    hint {\n"
        "        show on\n"
        "        description \"JamStudio Mix Buses\"\n"
        "    }\n"
        "}\n"
        + juce::String (kAsoundEnd) + "\n";

    const auto begin = text.indexOf (kAsoundBegin);
    const auto end = text.indexOf (kAsoundEnd);
    if (begin >= 0 && end > begin)
        text = text.substring (0, begin) + block + text.substring (end + juce::String (kAsoundEnd).length());
    else if (text.isNotEmpty())
        text = text.trimEnd() + "\n\n" + block;
    else
        text = block;

    if (! text.endsWithChar ('\n'))
        text << '\n';

    file.replaceWithText (text);
}
} // namespace

struct DeviceFanout
{
    struct Slot
    {
        struct Callback : juce::AudioIODeviceCallback
        {
            Slot* slot = nullptr;

            void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
            void audioDeviceStopped() override {}

            void audioDeviceIOCallbackWithContext (const float* const*,
                                                   int,
                                                   float* const* outputChannelData,
                                                   int numOutputChannels,
                                                   int numSamples,
                                                   const juce::AudioIODeviceCallbackContext&) override
            {
                if (outputChannelData != nullptr)
                    for (int ch = 0; ch < numOutputChannels; ++ch)
                        if (outputChannelData[ch] != nullptr)
                            juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);

                if (slot == nullptr || ! slot->enabled.load (std::memory_order_acquire) || numSamples <= 0)
                    return;

                int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
                slot->fifo.prepareToRead (numSamples, start1, size1, start2, size2);

                auto copyOut = [&] (int dest, int src, int count)
                {
                    if (count <= 0 || outputChannelData == nullptr || outputChannelData[0] == nullptr)
                        return;

                    juce::FloatVectorOperations::copy (outputChannelData[0] + dest,
                                                       slot->left.data() + src,
                                                       count);

                    if (numOutputChannels > 1 && outputChannelData[1] != nullptr)
                        juce::FloatVectorOperations::copy (outputChannelData[1] + dest,
                                                           slot->right.data() + src,
                                                           count);
                };

                copyOut (0, start1, size1);
                copyOut (size1, start2, size2);
                slot->fifo.finishedRead (size1 + size2);
            }
        };

        std::atomic<bool> enabled { false };
        int bus = 0;
        juce::String deviceName;
        juce::AbstractFifo fifo { 65536 };
        std::vector<float> left = std::vector<float> (65536, 0.0f);
        std::vector<float> right = std::vector<float> (65536, 0.0f);
        Callback callback;
        std::unique_ptr<juce::AudioIODevice> device;

        Slot()
        {
            callback.slot = this;
        }

        void push (const float* leftIn, const float* rightIn, const int numSamples)
        {
            if (! enabled.load (std::memory_order_acquire) || numSamples <= 0)
                return;

            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToWrite (numSamples, start1, size1, start2, size2);

            auto copyIn = [&] (int dest, int src, int count)
            {
                if (count <= 0)
                    return;

                if (leftIn != nullptr)
                    juce::FloatVectorOperations::copy (left.data() + dest, leftIn + src, count);
                else
                    juce::FloatVectorOperations::clear (left.data() + dest, count);

                if (rightIn != nullptr)
                    juce::FloatVectorOperations::copy (right.data() + dest, rightIn + src, count);
                else
                    juce::FloatVectorOperations::clear (right.data() + dest, count);
            };

            copyIn (start1, 0, size1);
            copyIn (start2, size1, size2);
            fifo.finishedWrite (size1 + size2);
        }

        void close()
        {
            enabled.store (false, std::memory_order_release);

            if (device != nullptr)
            {
                device->stop();
                device->close();
                device.reset();
            }

            deviceName.clear();
            fifo.reset();
        }
    };

    std::array<Slot, 3> slots;
};

//==============================================================================
juce::File AudioInterfaceSettings::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("audio-interface.json");
}

void AudioInterfaceSettings::load()
{
    const auto file = settingsFile();
    if (! file.existsAsFile())
        return;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    if (auto* o = parsed.getDynamicObject())
    {
        mode = audioRoutingModeFromString (o->getProperty ("mode").toString());
        preferComputerSpeakersForMonitor = static_cast<bool> (
            o->getProperty ("preferComputerSpeakersForMonitor"));
        // Missing key defaults to true for plug-and-play.
        if (! o->hasProperty ("preferComputerSpeakersForMonitor"))
            preferComputerSpeakersForMonitor = true;

        deviceTypeName = o->getProperty ("deviceTypeName").toString();
        preferredInputName = o->getProperty ("preferredInputName").toString();
        preferredOutputName = o->getProperty ("preferredOutputName").toString();
        maxInputChannels = juce::jmax (1, static_cast<int> (o->getProperty ("maxInputChannels")));
        maxOutputChannels = juce::jmax (1, static_cast<int> (o->getProperty ("maxOutputChannels")));
        preferredSampleRate = static_cast<double> (o->getProperty ("preferredSampleRate"));
        preferredBufferSize = static_cast<int> (o->getProperty ("preferredBufferSize"));

        if (o->hasProperty ("fohSink") || o->hasProperty ("monitorASink") || o->hasProperty ("monitorBSink"))
        {
            busOutputsSpecified = true;
            fohSink = o->getProperty ("fohSink").toString();
            monitorASink = o->getProperty ("monitorASink").toString();
            monitorBSink = o->getProperty ("monitorBSink").toString();
        }

        if (maxInputChannels <= 0) maxInputChannels = kDefaultMaxIn;
        if (maxOutputChannels <= 0) maxOutputChannels = kDefaultMaxOut;
    }
}

void AudioInterfaceSettings::save() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("mode", audioRoutingModeToString (mode));
    o->setProperty ("preferComputerSpeakersForMonitor", preferComputerSpeakersForMonitor);
    o->setProperty ("deviceTypeName", deviceTypeName);
    o->setProperty ("preferredInputName", preferredInputName);
    o->setProperty ("preferredOutputName", preferredOutputName);
    o->setProperty ("maxInputChannels", maxInputChannels);
    o->setProperty ("maxOutputChannels", maxOutputChannels);
    o->setProperty ("preferredSampleRate", preferredSampleRate);
    o->setProperty ("preferredBufferSize", preferredBufferSize);
    o->setProperty ("fohSink", fohSink);
    o->setProperty ("monitorASink", monitorASink);
    o->setProperty ("monitorBSink", monitorBSink);

    const auto file = settingsFile();
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

//==============================================================================
AudioInterfaceManager::AudioInterfaceManager (juce::AudioDeviceManager& dm)
    : deviceManager (dm)
{
    settings.load();
}

AudioInterfaceManager::~AudioInterfaceManager()
{
    stopTimer();
    outputsHeld = false;
    syncBusLoopbacks();
    settings.save();
}

void AudioInterfaceManager::installBusPlaybackDevice()
{
   #if JUCE_LINUX
    writeBusAsoundrc();
    ensureBusSink();
   #endif
}

void AudioInterfaceManager::initialiseAtStartup()
{
    installBusPlaybackDevice();
    ensureBusGraph();
    outputsHeld = false;
    syncBusLoopbacks();
    assignDefaultBusOutputsIfNeeded();
    scanHardware();
    rescanAndApply();
    // Hot-plug rescans stay off. Re-opening every ALSA device on a timer races
    // PipeWire and can take the audio thread down. Rescan stays on the dialog button.
    startTimerHz (4);
}

void AudioInterfaceManager::scanHardware()
{
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type != nullptr)
            type->scanForDevices();
}

void AudioInterfaceManager::timerCallback()
{
    if (outputsHeld.load() || ! loopbacksOpen)
        return;

    // Drop headphone/speaker links about a second after transport stops.
    if (++silenceTicks >= 4)
        syncBusLoopbacks();
}

void AudioInterfaceManager::rescanAndApply()
{
    scanHardware();
    lastError = applySettings (settings);
    lastFingerprint = buildInventory().fingerprint;
    sendChangeMessage();
}

void AudioInterfaceManager::setSettings (const AudioInterfaceSettings& newSettings, const bool applyNow)
{
    settings = newSettings;
    settings.save();
    if (applyNow)
        rescanAndApply();
}

juce::String AudioInterfaceManager::applySettings (const AudioInterfaceSettings& newSettings)
{
    settings = newSettings;
    if (anyBusDestination() && settings.preferredSampleRate < 40000.0)
        settings.preferredSampleRate = 48000.0;
    settings.save();
    rememberBusStatus();

    const auto inv = buildInventory();
    lastFingerprint = inv.fingerprint;

    juce::String typeName = settings.deviceTypeName;
    juce::String inName;
    juce::String outName;
    int wantIn = juce::jmax (1, settings.maxInputChannels);
    int wantOut = juce::jmax (1, settings.maxOutputChannels);

    switch (settings.mode)
    {
        case AudioRoutingMode::manual:
        {
            inName = settings.preferredInputName;
            outName = settings.preferredOutputName;

            // Infer type from chosen names if not set.
            if (typeName.isEmpty())
            {
                for (const auto& d : inv.inputs)
                    if (d.name == inName) { typeName = d.typeName; break; }
                if (typeName.isEmpty())
                    for (const auto& d : inv.outputs)
                        if (d.name == outName) { typeName = d.typeName; break; }
            }
            break;
        }

        case AudioRoutingMode::sameDevice:
        {
            const auto pair = pickSameDevicePair (inv);
            typeName = pair.typeName;
            inName = pair.maxInputChannels > 0 ? pair.name : juce::String();
            outName = pair.maxOutputChannels > 0 ? pair.name : juce::String();
            // For stage multi-out on one box, open as many outs as the bus matrix needs.
            wantOut = juce::jmax (wantOut, juce::jmin (kMaxMixChannels, pair.maxOutputChannels));
            wantIn = juce::jmin (wantIn, juce::jmax (1, pair.maxInputChannels));
            break;
        }

        case AudioRoutingMode::plugAndPlay:
        default:
        {
            const auto bestIn = pickBestInput (inv);
            const auto bestOut = settings.preferComputerSpeakersForMonitor
                                     ? pickBestMonitorOutput (inv, bestIn)
                                     : pickSameDevicePair (inv);

            // Capture from the multi-IO box when present.
            if (bestIn.name.isNotEmpty())
            {
                typeName = bestIn.typeName;
                inName = bestIn.name;
                wantIn = juce::jmin (wantIn, juce::jmax (1, bestIn.maxInputChannels));
            }

            // Monitor: computer speakers by default, or same interface if preferred off.
            if (bestOut.name.isNotEmpty())
            {
                // Prefer staying on one device type when possible.
                if (typeName.isEmpty())
                    typeName = bestOut.typeName;
                outName = bestOut.name;
                wantOut = juce::jmin (wantOut, juce::jmax (1, bestOut.maxOutputChannels));
            }

            // If input and output are different physical devices, keep one device type.
            // ALSA and Core Audio both accept mixed input/output names under one type.
            if (bestIn.typeName.isNotEmpty() && bestOut.typeName.isNotEmpty()
                && bestIn.typeName == bestOut.typeName)
                typeName = bestIn.typeName;
            else if (bestOut.typeName.isNotEmpty())
                typeName = bestOut.typeName;
            else if (bestIn.typeName.isNotEmpty())
                typeName = bestIn.typeName;

            // When monitoring on computer speakers, only 2 outs exist — multi-bus folds to FOH.
            if (settings.preferComputerSpeakersForMonitor
                && looksLikeMultiIoInterface (bestIn.name, bestIn.maxInputChannels, bestIn.maxOutputChannels)
                && ! looksLikeMultiIoInterface (bestOut.name, bestOut.maxInputChannels, bestOut.maxOutputChannels))
            {
                wantOut = juce::jmin (wantOut, 2);
            }
            break;
        }
    }

    if (anyBusDestination())
    {
       #if JUCE_LINUX
        // The mix is rendered as 6 channels on the virtual bus, then each
        // stereo pair is linked to the sink chosen for that bus.
        installBusPlaybackDevice();
        ensureBusGraph();
        typeName = "ALSA";
        outName = kBusDeviceName;
        wantOut = kMaxMixChannels;
        for (const auto& d : inv.outputs)
        {
            if (d.name.containsIgnoreCase (kBusDeviceName))
            {
                outName = d.name;
                if (d.typeName.isNotEmpty())
                    typeName = d.typeName;
                break;
            }
        }
       #else
        // Mac and Windows have no PipeWire bus. Open the FOH device (or the
        // first chosen sink) and fan the other buses out to their own devices.
        auto pickOut = settings.fohSink;
        if (pickOut.isEmpty())
            pickOut = settings.monitorASink;
        if (pickOut.isEmpty())
            pickOut = settings.monitorBSink;

        if (pickOut.isNotEmpty())
        {
            outName = pickOut;
            auto sameDevice = true;
            auto named = 0;

            for (const auto* sink : { &settings.fohSink, &settings.monitorASink, &settings.monitorBSink })
            {
                if (sink->isEmpty())
                    continue;

                ++named;
                if (*sink != pickOut)
                    sameDevice = false;
            }

            for (const auto& d : inv.outputs)
            {
                if (d.name != pickOut)
                    continue;

                if (d.typeName.isNotEmpty())
                    typeName = d.typeName;

                const auto spread = sameDevice && named > 1 && d.maxOutputChannels >= kMaxMixChannels;
                wantOut = spread ? juce::jmin (kMaxMixChannels, d.maxOutputChannels)
                                 : juce::jmin (2, juce::jmax (1, d.maxOutputChannels));
                break;
            }
        }
       #endif
    }

    if (typeName.isEmpty() && deviceManager.getAvailableDeviceTypes().size() > 0)
        typeName = deviceManager.getAvailableDeviceTypes().getUnchecked (0)->getTypeName();

    const auto err = applyChoice (typeName, inName, outName, wantIn, wantOut);
    syncBusLoopbacks();
    return err;
}

AudioInterfaceManager::DeviceInventory AudioInterfaceManager::buildInventory() const
{
    DeviceInventory inv;

    for (auto* type : deviceManager.getAvailableDeviceTypes())
    {
        if (type == nullptr)
            continue;

        // Inputs
        for (const auto& name : type->getDeviceNames (true))
        {
            auto c = probeDevice (*type, {}, name);
            c.name = name;
            c.score = scoreAsCaptureInterface (name, c.maxInputChannels, c.maxOutputChannels);
            inv.inputs.add (c);
        }

        // Outputs
        for (const auto& name : type->getDeviceNames (false))
        {
            auto c = probeDevice (*type, name, {});
            c.name = name;
            c.score = scoreAsComputerMonitor (name, c.maxOutputChannels);
            inv.outputs.add (c);
        }
    }

    inv.fingerprint = makeFingerprint (inv.inputs, inv.outputs);
    return inv;
}

juce::Array<AudioDeviceChoice> AudioInterfaceManager::listInputDevices() const
{
    return buildInventory().inputs;
}

juce::Array<AudioDeviceChoice> AudioInterfaceManager::listOutputDevices() const
{
    return buildInventory().outputs;
}

juce::StringArray AudioInterfaceManager::listDeviceTypeNames() const
{
    juce::StringArray names;
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type != nullptr)
            names.add (type->getTypeName());
    return names;
}

AudioDeviceChoice AudioInterfaceManager::pickBestInput (const DeviceInventory& inv) const
{
    AudioDeviceChoice best;
    int bestScore = std::numeric_limits<int>::min();

    for (const auto& d : inv.inputs)
    {
        if (d.maxInputChannels <= 0 && d.name.isEmpty())
            continue;

        auto score = d.score;
        if (settings.preferredInputName.isNotEmpty() && d.name == settings.preferredInputName)
            score += 500;
        if (settings.deviceTypeName.isNotEmpty() && d.typeName == settings.deviceTypeName)
            score += 20;

        if (score > bestScore)
        {
            bestScore = score;
            best = d;
        }
    }

    return best;
}

AudioDeviceChoice AudioInterfaceManager::pickBestMonitorOutput (const DeviceInventory& inv,
                                                                const AudioDeviceChoice& chosenInput) const
{
    AudioDeviceChoice best;
    int bestScore = std::numeric_limits<int>::min();

    const bool haveInterface = looksLikeMultiIoInterface (chosenInput.name,
                                                          chosenInput.maxInputChannels,
                                                          chosenInput.maxOutputChannels);

    for (const auto& d : inv.outputs)
    {
        auto score = d.score;

        if (settings.preferredOutputName.isNotEmpty() && d.name == settings.preferredOutputName)
            score += 500;

        // Strongly prefer non-interface outputs when we already captured from an interface.
        if (haveInterface && looksLikeMultiIoInterface (d.name, d.maxInputChannels, d.maxOutputChannels))
            score -= 120;

        // Prefer the same backend as the input (Core Audio, ALSA, or WASAPI).
        if (chosenInput.typeName.isNotEmpty() && d.typeName == chosenInput.typeName)
            score += 25;

        if (score > bestScore)
        {
            bestScore = score;
            best = d;
        }
    }

    // Fallback: if scoring failed, use first output.
    if (best.name.isEmpty() && inv.outputs.size() > 0)
        best = inv.outputs.getReference (0);

    return best;
}

AudioDeviceChoice AudioInterfaceManager::pickSameDevicePair (const DeviceInventory& inv) const
{
    // Prefer a device name that exists as both input and output with most channels.
    AudioDeviceChoice best;
    int bestScore = std::numeric_limits<int>::min();

    for (const auto& out : inv.outputs)
    {
        for (const auto& in : inv.inputs)
        {
            if (in.name != out.name || in.typeName != out.typeName)
                continue;

            const auto total = in.maxInputChannels + out.maxOutputChannels;
            auto score = total * 10 + scoreAsCaptureInterface (in.name, in.maxInputChannels, out.maxOutputChannels);
            if (settings.preferredOutputName == out.name || settings.preferredInputName == in.name)
                score += 400;

            if (score > bestScore)
            {
                bestScore = score;
                best = out;
                best.maxInputChannels = in.maxInputChannels;
                best.maxOutputChannels = out.maxOutputChannels;
                best.score = score;
            }
        }
    }

    if (best.name.isEmpty())
    {
        // Fall back to best output + best input separately under same type.
        best = pickBestInput (inv);
        if (best.name.isEmpty() && inv.outputs.size() > 0)
            best = inv.outputs.getReference (0);
    }

    return best;
}

juce::String AudioInterfaceManager::applyChoice (const juce::String& typeName,
                                                 const juce::String& inputName,
                                                 const juce::String& outputName,
                                                 const int wantInCh,
                                                 const int wantOutCh)
{
    const juce::ScopedValueSetter<bool> guard (suppressRescan, true);

    if (typeName.isNotEmpty())
        deviceManager.setCurrentAudioDeviceType (typeName, true);

    // Probe channel counts for the chosen endpoints.
    int maxIn = wantInCh;
    int maxOut = wantOutCh;

    if (auto* type = deviceManager.getCurrentDeviceTypeObject())
    {
        if (auto* dev = type->createDevice (outputName, inputName))
        {
            std::unique_ptr<juce::AudioIODevice> holder (dev);
            maxIn = juce::jmax (0, holder->getInputChannelNames().size());
            maxOut = juce::jmax (0, holder->getOutputChannelNames().size());
        }
    }

    const int openIn = juce::jlimit (0, juce::jmax (0, maxIn), wantInCh);
    const int openOut = juce::jlimit (0, juce::jmax (0, maxOut), wantOutCh);

    // Ensure at least something useful when hardware exists.
    const int finalIn = openIn > 0 ? openIn : (maxIn > 0 ? juce::jmin (2, maxIn) : 0);
    const int finalOut = openOut > 0 ? openOut : (maxOut > 0 ? juce::jmin (2, maxOut) : 0);

    juce::AudioDeviceManager::AudioDeviceSetup setup = deviceManager.getAudioDeviceSetup();
    setup.inputDeviceName = inputName;
    setup.outputDeviceName = outputName;
    setup.useDefaultInputChannels = false;
    setup.useDefaultOutputChannels = false;
    enableFirstNChannels (setup.inputChannels, finalIn);
    enableFirstNChannels (setup.outputChannels, finalOut);

    if (settings.preferredSampleRate > 0.0)
        setup.sampleRate = settings.preferredSampleRate;
    if (settings.preferredBufferSize > 0)
        setup.bufferSize = settings.preferredBufferSize;

    auto err = deviceManager.setAudioDeviceSetup (setup, true);

    // A failed open of the mix bus used to fall through to the Pulse device.
    // That plugin is what folds the Fast Track clock into a stuck hot loop.
    if (err.isNotEmpty() && outputName.containsIgnoreCase (kBusDeviceName))
        err = deviceManager.setAudioDeviceSetup (setup, true);

    lastError = err;

    if (err.isNotEmpty() && ! outputName.containsIgnoreCase (kBusDeviceName))
    {
        deviceManager.initialiseWithDefaultDevices (juce::jmax (1, finalIn), juce::jmax (2, finalOut));
        lastError = err;
    }

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        juce::File::getSpecialLocation (juce::File::tempDirectory)
            .getChildFile ("jamstudio-route.log")
            .replaceWithText (
            "out=" + outputName
            + " opened=" + dev->getName()
            + " ch=" + juce::String (dev->getActiveOutputChannels().countNumberOfSetBits())
            + " err=" + lastError + "\n");
    }

    return lastError;
}

AudioRoutingSnapshot AudioInterfaceManager::getSnapshot() const
{
    AudioRoutingSnapshot s;
    s.deviceTypeName = deviceManager.getCurrentAudioDeviceType();
    s.error = lastError;

    const auto setup = deviceManager.getAudioDeviceSetup();
    s.inputName = setup.inputDeviceName;
    s.outputName = setup.outputDeviceName;
    s.sampleRate = setup.sampleRate;
    s.bufferSize = setup.bufferSize;
    s.activeInputChannels = setup.inputChannels.countNumberOfSetBits();
    s.activeOutputChannels = setup.outputChannels.countNumberOfSetBits();

    if (auto* dev = deviceManager.getCurrentAudioDevice())
    {
        s.sampleRate = dev->getCurrentSampleRate();
        s.bufferSize = dev->getCurrentBufferSizeSamples();
        s.activeInputChannels = dev->getActiveInputChannels().countNumberOfSetBits();
        s.activeOutputChannels = dev->getActiveOutputChannels().countNumberOfSetBits();
        if (s.inputName.isEmpty())
            s.inputName = dev->getName();
        if (s.outputName.isEmpty())
            s.outputName = dev->getName();
    }

    s.availableBuses = juce::jlimit (1, kNumMixBuses, s.activeOutputChannels / kChannelsPerBus);

    juce::String busLabel;
    if (s.availableBuses >= 3)
        busLabel = "FOH + Mon A + Mon B (outs 1-6)";
    else if (s.availableBuses == 2)
        busLabel = "FOH + Mon A (outs 1-4)";
    else
        busLabel = "FOH only (stereo monitor)";

    s.summary = "In: " + (s.inputName.isNotEmpty() ? s.inputName : juce::String ("(none)"))
                + " (" + juce::String (s.activeInputChannels) + " ch)  |  Out: "
                + (s.outputName.isNotEmpty() ? s.outputName : juce::String ("(none)"))
                + " (" + juce::String (s.activeOutputChannels) + " ch)  |  " + busLabel;

    if (s.sampleRate > 0.0)
        s.summary += "  @ " + juce::String (s.sampleRate / 1000.0, 1) + " kHz";

    if (busStatusText.isNotEmpty())
        s.summary += "\n" + busStatusText;

    if (s.error.isNotEmpty())
        s.summary += "  [warn: " + s.error + "]";

    return s;
}

juce::String AudioInterfaceManager::getStatusSummary() const
{
    return getSnapshot().summary;
}

static bool isHardwareInterfaceName (const juce::String& n)
{
    const char* brands[] = {
        "scarlett", "focusrite", "motu", "rme", "presonus", "behringer", "audient",
        "universal audio", "apollo", "komplete audio", "m-audio", "steinberg", "ur22",
        "ur44", "babyface", "fireface", "zoom uac", "ssl 2", "volta", "claw"
    };

    for (auto* b : brands)
        if (n.contains (b))
            return true;

    return false;
}

static bool isVirtualLoopbackName (const juce::String& n)
{
    const char* markers[] = {
        "zoom", "audiojingle", "mmaudio", "blackhole", "soundflower", "loopback",
        "virtual", "aggregate", "teams", "discord", "chrome", "skype", "ui sounds",
        "background music", "vb-audio", "cable output", "cable input"
    };

    for (auto* marker : markers)
        if (n.contains (marker))
            return true;

    return false;
}

int AudioInterfaceManager::scoreAsCaptureInterface (const juce::String& name,
                                                    const int maxIn,
                                                    const int maxOut)
{
    auto n = name.toLowerCase();
    int score = maxIn * 12;
    const auto hardware = isHardwareInterfaceName (n);

    if (hardware)
        score += 140;

    if (n.contains ("usb") || n.contains ("interface") || n.contains ("audio box"))
        score += 40;

    // Built-in mic is the right default on a laptop. Virtual meeting/loopback
    // drivers (Zoom, Audiojingle, MMAudio, …) outscore it on channel count and
    // then feed the speakers back into the input.
    if (n.contains ("microphone") || n.contains ("built-in") || n.contains ("built in"))
        score += 80;
    if (maxIn >= 4)
        score += 60;
    if (maxIn >= 8)
        score += 40;
    if (maxIn > maxOut)
        score += 25;

    // Deprioritise monitors / HDMI / loopback as capture sources.
    if (n.contains ("hdmi") || n.contains ("display")
        || n.contains ("monitor of") || n.contains ("null")
        || n.contains ("speaker"))
        score -= 250;

    if (! hardware && isVirtualLoopbackName (n))
        score -= 500;

    if (n.contains ("default") || n.contains ("pulse"))
        score -= 15; // fine fallback, not preferred over real interfaces

    if (n.contains ("jamstudio"))
        score -= 400;

    return score;
}

int AudioInterfaceManager::scoreAsComputerMonitor (const juce::String& name, const int maxOut)
{
    auto n = name.toLowerCase();
    int score = juce::jmin (maxOut, 2) * 8;

    if (n.contains ("pch") || n.contains ("hda") || n.contains ("realtek")
        || n.contains ("built-in") || n.contains ("built in") || n.contains ("internal")
        || n.contains ("speakers") || n.contains ("headphones")
        || n.contains ("macbook") || n.contains ("imac") || n.contains ("mac mini")
        || n.contains ("mac studio") || n.contains ("notebook")
        || n.contains ("analog"))
        score += 90;

    if (n.contains ("pulse") || n.contains ("default"))
        score += 45;

    if (n.contains ("hdmi") || n.contains ("displayport") || n.contains ("display")
        || n.contains (" tv"))
        score -= 40;

    if (n.contains ("jamstudio"))
        score -= 400;

    // Multi-IO boxes are poor default *monitors* when the user has no speakers on them.
    if (looksLikeMultiIoInterface (name, 0, maxOut) || maxOut >= 8)
        score -= 80;

    const char* brands[] = {
        "scarlett", "focusrite", "motu", "rme", "presonus", "behringer", "audient"
    };
    for (auto* b : brands)
        if (n.contains (b))
            score -= 100;

    return score;
}

bool AudioInterfaceManager::looksLikeMultiIoInterface (const juce::String& name,
                                                       const int maxIn,
                                                       const int maxOut)
{
    if (name.isEmpty())
        return false;

    auto n = name.toLowerCase();
    const char* brands[] = {
        "scarlett", "focusrite", "motu", "rme", "presonus", "behringer", "audient",
        "universal audio", "apollo", "komplete audio", "steinberg", "fireface", "babyface"
    };
    for (auto* b : brands)
        if (n.contains (b))
            return true;

    if (maxIn >= 4 || maxOut >= 6)
        return true;

    return n.contains ("interface") || (n.contains ("usb") && (maxIn >= 2 || maxOut >= 4));
}

bool AudioInterfaceManager::anyBusDestination() const noexcept
{
    return settings.fohSink.isNotEmpty()
        || settings.monitorASink.isNotEmpty()
        || settings.monitorBSink.isNotEmpty();
}

void AudioInterfaceManager::setBusDestinations (const juce::String& foh,
                                                const juce::String& monitorA,
                                                const juce::String& monitorB)
{
    settings.fohSink = foh;
    settings.monitorASink = monitorA;
    settings.monitorBSink = monitorB;
    settings.busOutputsSpecified = true;
    settings.save();
    rememberBusStatus();

    const auto outputName = getSnapshot().outputName;
    if (anyBusDestination() && outputName.containsIgnoreCase (kBusDeviceName))
        syncBusLoopbacks();
    else
        lastError = applySettings (settings);

    sendChangeMessage();
}

void AudioInterfaceManager::setMixOutputsHeld (const bool held)
{
    if (held == outputsHeld.load())
        return;

    outputsHeld.store (held);
    silenceTicks = 0;
    if (held)
        syncBusLoopbacks();
}

juce::Array<PlaybackSink> AudioInterfaceManager::listPlaybackSinks() const
{
   #if ! JUCE_LINUX
    juce::Array<PlaybackSink> sinks;

    for (const auto& device : listOutputDevices())
    {
        if (device.name.isEmpty())
            continue;

        PlaybackSink sink;
        sink.name = device.name;
        sink.description = device.name;
        sinks.add (sink);
    }

    return sinks;
   #else
    juce::Array<PlaybackSink> sinks;
    const auto text = runPactl ({ kPactl, "list", "sinks" });
    PlaybackSink current;

    for (const auto& raw : juce::StringArray::fromLines (text))
    {
        const auto line = raw.trim();
        if (line.startsWith ("Name:"))
        {
            current.name = line.fromFirstOccurrenceOf (":", false, false).trim();
        }
        else if (line.startsWith ("Description:"))
        {
            current.description = line.fromFirstOccurrenceOf (":", false, false).trim();
            if (current.name.isNotEmpty()
                && ! current.name.startsWith (kBusSink)
                && ! current.name.startsWith ("jamstudio_bus_"))
                sinks.add (current);
            current = {};
        }
    }

    return sinks;
   #endif
}

void AudioInterfaceManager::assignDefaultBusOutputsIfNeeded()
{
    if (settings.busOutputsSpecified)
        return;

   #if ! JUCE_LINUX
    const auto outs = listOutputDevices();
    AudioDeviceChoice best;
    auto bestScore = std::numeric_limits<int>::min();

    for (const auto& device : outs)
    {
        if (device.score > bestScore && device.name.isNotEmpty())
        {
            bestScore = device.score;
            best = device;
        }
    }

    settings.fohSink = best.name;

    for (const auto& device : outs)
    {
        if (device.name.isEmpty() || device.name == best.name)
            continue;

        if (device.name.containsIgnoreCase ("head")
            || device.name.containsIgnoreCase ("display")
            || device.name.containsIgnoreCase ("dock"))
        {
            settings.monitorASink = device.name;
            break;
        }
    }

    // One pair of speakers has to carry the monitor mix too, or the click
    // and the player's cue stay on a bus that is not connected.
    if (settings.monitorASink.isEmpty())
        settings.monitorASink = settings.fohSink;

    settings.monitorBSink.clear();
    settings.busOutputsSpecified = true;
    settings.save();
    return;
   #endif

    const auto sinks = listPlaybackSinks();
    const auto pick = [&sinks] (const auto pred)
    {
        for (const auto& sink : sinks)
            if (pred (sink))
                return sink.name;
        return juce::String();
    };

    settings.fohSink = pick ([] (const PlaybackSink& sink)
    {
        return sink.description.containsIgnoreCase ("Built-in")
            && sink.description.containsIgnoreCase ("Analog");
    });
    settings.monitorASink = pick ([] (const PlaybackSink& sink)
    {
        return sink.description.containsIgnoreCase ("Displaylink");
    });
    settings.monitorBSink.clear();
    settings.busOutputsSpecified = true;
    settings.save();
}

void AudioInterfaceManager::rememberBusStatus()
{
    if (! settings.busOutputsSpecified && ! anyBusDestination())
    {
        busStatusText.clear();
        return;
    }

    const auto sinks = listPlaybackSinks();
    const auto label = [&sinks] (const juce::String& name)
    {
        if (name.isEmpty())
            return juce::String ("Off");
        for (const auto& sink : sinks)
            if (sink.name == name)
                return sink.description;
        return name;
    };

    busStatusText = "FOH → " + label (settings.fohSink)
                    + "    Mon A → " + label (settings.monitorASink)
                    + "    Mon B → " + label (settings.monitorBSink);
}

void AudioInterfaceManager::ensureBusGraph()
{
   #if ! JUCE_LINUX
    return;
   #else
    ensureBusSink();
    const auto modules = runPactl ({ kPactl, "list", "short", "modules" });
    ensureBusRemap (modules, "jamstudio_bus_foh", "front-left,front-right", "JamStudio-FOH");
    ensureBusRemap (modules, "jamstudio_bus_mona", "rear-left,rear-right", "JamStudio-MonA");
    ensureBusRemap (modules, "jamstudio_bus_monb", "front-center,lfe", "JamStudio-MonB");
   #endif
}

void AudioInterfaceManager::stopBusCopy (const int index)
{
   #if ! JUCE_LINUX
    juce::ignoreUnused (index);
    return;
   #else
    auto& copy = busCopies[static_cast<size_t> (index)];

    if (copy.pid > 0)
        ::kill (copy.pid, SIGTERM);

    if (copy.process != nullptr)
    {
        if (copy.process->isRunning())
            copy.process->waitForProcessToFinish (300);
        if (copy.process->isRunning())
            copy.process->kill();
        copy.process.reset();
    }

    copy.pid = 0;
    copy.source.clear();
    copy.sink.clear();
   #endif
}

void AudioInterfaceManager::startBusCopy (const int index,
                                          const juce::String& source,
                                          const juce::String& sink)
{
   #if ! JUCE_LINUX
    juce::ignoreUnused (index, source, sink);
    return;
   #else
    stopBusCopy (index);

    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    const juce::File candidates[] = {
        exe.getParentDirectory().getChildFile ("bus-bridge.py"),
        exe.getParentDirectory().getParentDirectory().getChildFile ("share").getChildFile ("jamstudio").getChildFile ("bus-bridge.py"),
        juce::File::getSpecialLocation (juce::File::userHomeDirectory)
            .getChildFile (".local/share/jamstudio/bus-bridge.py")
    };
    juce::File script;
    for (const auto& candidate : candidates)
        if (candidate.existsAsFile())
        {
            script = candidate;
            break;
        }
    if (! script.existsAsFile())
    {
        busStatusText = "Missing bus-bridge.py";
        return;
    }

    auto process = std::make_unique<juce::ChildProcess>();
    juce::StringArray args;
    args.add ("/usr/bin/python3");
    args.add (script.getFullPathName());
    args.add ("--source");
    args.add (source);
    args.add ("--sink");
    args.add (sink);
    args.add ("--tag");
    args.add (source);

    if (! process->start (args))
    {
        busStatusText = "Could not start the copy to " + sink;
        return;
    }

    const auto pidFile = juce::File ("/tmp/jamstudio-bridge-" + source + ".pid");
    for (int i = 0; i < 20 && ! pidFile.existsAsFile(); ++i)
        juce::Thread::sleep (20);

    auto& copy = busCopies[static_cast<size_t> (index)];
    copy.source = source;
    copy.sink = sink;
    copy.pid = pidFile.existsAsFile() ? pidFile.loadFileAsString().trim().getIntValue() : 0;
    copy.process = std::move (process);
   #endif
}

void AudioInterfaceManager::syncBusLoopbacks()
{
   #if ! JUCE_LINUX
    syncDeviceFanout();
    return;
   #endif
    const struct Link
    {
        const char* source;
        const juce::String* sink;
    } links[] = {
        { "jamstudio_bus_foh", &settings.fohSink },
        { "jamstudio_bus_mona", &settings.monitorASink },
        { "jamstudio_bus_monb", &settings.monitorBSink }
    };

    // A Pulse loopback makes the dock or the headphones the mix clock, and
    // every speaker then buzzes. Drop any that are still loaded.
    const auto modules = runPactl ({ kPactl, "list", "short", "modules" });
    for (const auto& line : juce::StringArray::fromLines (modules))
    {
        if (! line.contains ("module-loopback"))
            continue;

        auto ours = false;
        for (const auto& link : links)
            if (line.contains ("source=" + juce::String (link.source)))
                ours = true;

        if (! ours)
            continue;

        const auto id = line.upToFirstOccurrenceOf ("\t", false, false).trim();
        if (id.isNotEmpty())
            runPactl ({ kPactl, "unload-module", id });
    }

    bool anyOpen = false;
    juce::StringArray claimedSinks;

    for (int i = 0; i < 3; ++i)
    {
        const auto& link = links[i];
        // A second pw-cat into one Bluetooth sink mutes that sink.
        auto want = outputsHeld.load() && link.sink->isNotEmpty() && ! claimedSinks.contains (*link.sink);

        if (want)
            claimedSinks.add (*link.sink);

        auto& copy = busCopies[static_cast<size_t> (i)];
        const auto same = copy.process != nullptr
                          && copy.process->isRunning()
                          && copy.source == link.source
                          && copy.sink == *link.sink;

        if (want && same)
        {
            anyOpen = true;
            continue;
        }

        if (! want)
        {
            if (copy.process != nullptr)
                stopBusCopy (i);
            continue;
        }

        startBusCopy (i, link.source, *link.sink);
        if (busCopies[static_cast<size_t> (i)].process != nullptr)
            anyOpen = true;
    }

    loopbacksOpen = anyOpen;
    if (! loopbacksOpen)
        silenceTicks = 0;
}

void AudioInterfaceManager::syncDeviceFanout()
{
    if (deviceFanout == nullptr)
        deviceFanout = std::make_unique<DeviceFanout>();

    const auto setup = deviceManager.getAudioDeviceSetup();
    const auto mainName = setup.outputDeviceName;
    auto mainOuts = 0;

    if (auto* dev = deviceManager.getCurrentAudioDevice())
        mainOuts = dev->getActiveOutputChannels().countNumberOfSetBits();

    const juce::String sinks[] = { settings.fohSink, settings.monitorASink, settings.monitorBSink };
    auto allOnMain = mainOuts >= kMaxMixChannels && mainName.isNotEmpty();
    auto anyNamed = false;

    for (const auto& sink : sinks)
    {
        if (sink.isEmpty())
            continue;

        anyNamed = true;

        if (sink != mainName)
            allOnMain = false;
    }

    mainGetsAllBuses.store (allOnMain && anyNamed, std::memory_order_release);

    std::uint8_t mask = 0;

    if (! (allOnMain && anyNamed))
    {
        for (int bus = 0; bus < 3; ++bus)
            if (sinks[bus].isNotEmpty() && sinks[bus] == mainName)
                mask = static_cast<std::uint8_t> (mask | (1u << bus));

        if (mask == 0 && mainName.isNotEmpty())
            mask = 1;
    }

    mainBusMask.store (mask, std::memory_order_release);

    juce::StringArray claimed;
    claimed.add (mainName);
    loopbacksOpen = false;

    for (int bus = 0; bus < 3; ++bus)
    {
        auto& slot = deviceFanout->slots[static_cast<size_t> (bus)];
        const auto& sink = sinks[bus];
        const auto want = outputsHeld.load() && sink.isNotEmpty() && sink != mainName && ! claimed.contains (sink);

        if (want)
            claimed.add (sink);

        if (! want)
        {
            slot.close();
            continue;
        }

        if (slot.device != nullptr && slot.deviceName == sink && slot.device->isOpen())
        {
            slot.bus = bus;
            slot.enabled.store (true, std::memory_order_release);
            loopbacksOpen = true;
            continue;
        }

        slot.close();

        juce::AudioIODeviceType* type = nullptr;

        for (auto* candidate : deviceManager.getAvailableDeviceTypes())
        {
            if (candidate != nullptr && candidate->getDeviceNames (false).contains (sink))
            {
                type = candidate;
                break;
            }
        }

        if (type == nullptr)
            continue;

        std::unique_ptr<juce::AudioIODevice> opened (type->createDevice (sink, {}));

        if (opened == nullptr)
            continue;

        juce::BigInteger outs;
        outs.setBit (0);

        if (opened->getOutputChannelNames().size() > 1)
            outs.setBit (1);

        const auto rate = setup.sampleRate > 0.0 ? setup.sampleRate : 48000.0;
        const auto buffer = setup.bufferSize > 0 ? setup.bufferSize : 512;

        if (opened->open ({}, outs, rate, buffer).isNotEmpty())
            continue;

        slot.deviceName = sink;
        slot.bus = bus;
        opened->start (&slot.callback);
        slot.device = std::move (opened);
        slot.enabled.store (true, std::memory_order_release);
        loopbacksOpen = true;
    }

    if (! loopbacksOpen)
        silenceTicks = 0;
}

void AudioInterfaceManager::sendMix (const float* const* mixChannels,
                                     const int numMixChannels,
                                     const int numSamples,
                                     float* const* deviceOut,
                                     const int numDeviceOut) noexcept
{
    auto silenceDevice = [&]
    {
        if (deviceOut == nullptr)
            return;

        for (int ch = 0; ch < numDeviceOut; ++ch)
            if (deviceOut[ch] != nullptr)
                juce::FloatVectorOperations::clear (deviceOut[ch], numSamples);
    };

    if (! outputsHeld.load (std::memory_order_acquire) || numSamples <= 0)
    {
        silenceDevice();
        return;
    }

    const auto copyStraight = [&]
    {
        const auto count = juce::jmin (numMixChannels, numDeviceOut);

        for (int ch = 0; ch < numDeviceOut; ++ch)
        {
            if (deviceOut == nullptr || deviceOut[ch] == nullptr)
                continue;

            if (ch < count && mixChannels != nullptr && mixChannels[ch] != nullptr)
                juce::FloatVectorOperations::copy (deviceOut[ch], mixChannels[ch], numSamples);
            else
                juce::FloatVectorOperations::clear (deviceOut[ch], numSamples);
        }
    };

   #if JUCE_LINUX
    copyStraight();
   #else
    if (mainGetsAllBuses.load (std::memory_order_acquire))
    {
        copyStraight();
    }
    else
    {
        silenceDevice();
        const auto mask = mainBusMask.load (std::memory_order_acquire);

        for (int bus = 0; bus < 3; ++bus)
        {
            if ((mask & static_cast<std::uint8_t> (1u << bus)) == 0 || mixChannels == nullptr || deviceOut == nullptr)
                continue;

            const auto base = bus * 2;

            if (numDeviceOut > 0 && deviceOut[0] != nullptr && base < numMixChannels && mixChannels[base] != nullptr)
                juce::FloatVectorOperations::add (deviceOut[0], mixChannels[base], numSamples);

            if (numDeviceOut > 1 && deviceOut[1] != nullptr && base + 1 < numMixChannels && mixChannels[base + 1] != nullptr)
                juce::FloatVectorOperations::add (deviceOut[1], mixChannels[base + 1], numSamples);
        }
    }

    if (deviceFanout != nullptr)
    {
        for (auto& slot : deviceFanout->slots)
        {
            if (! slot.enabled.load (std::memory_order_acquire))
                continue;

            const auto base = slot.bus * 2;
            const float* left = base < numMixChannels && mixChannels != nullptr ? mixChannels[base] : nullptr;
            const float* right = base + 1 < numMixChannels && mixChannels != nullptr ? mixChannels[base + 1] : nullptr;
            slot.push (left, right, numSamples);
        }
    }
   #endif
}

} // namespace jamstudio::audio
