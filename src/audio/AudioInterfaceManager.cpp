#include "AudioInterfaceManager.h"
#include "MixBus.h"

#include <limits>

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
} // namespace

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

        // Opening huge channel counts (e.g. 18/20) often breaks PipeWire ALSA duplex
        // into "capture only" with silent playback. Keep opens modest.
        if (maxInputChannels <= 0 || maxInputChannels > 8)
            maxInputChannels = kDefaultMaxIn;
        if (maxOutputChannels <= 0 || maxOutputChannels > kMaxMixChannels)
            maxOutputChannels = kDefaultMaxOut;

        // Drop sticky exclusive-hw preferences that commonly open capture-only under PW.
        if (preferredOutputName.containsIgnoreCase ("Direct hardware")
            || preferredOutputName.containsIgnoreCase ("hw:"))
            preferredOutputName = {};
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
    settings.save();
}

void AudioInterfaceManager::initialiseAtStartup()
{
    scanHardware();
    rescanAndApply();
    startTimer (2000); // hot-plug poll (USB interfaces)
}

void AudioInterfaceManager::scanHardware()
{
    for (auto* type : deviceManager.getAvailableDeviceTypes())
        if (type != nullptr)
            type->scanForDevices();
}

void AudioInterfaceManager::timerCallback()
{
    if (suppressRescan)
        return;

    scanHardware();
    const auto inv = buildInventory();
    if (inv.fingerprint != lastFingerprint)
    {
        lastFingerprint = inv.fingerprint;
        if (settings.mode != AudioRoutingMode::manual)
            rescanAndApply();
        else
            sendChangeMessage();
    }
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
    settings.save();

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

            // If input and output are different physical devices, keep type of the output
            // when backends require it — ALSA supports mixed IDs under one type.
            if (bestIn.typeName.isNotEmpty() && bestOut.typeName.isNotEmpty()
                && bestIn.typeName == bestOut.typeName)
                typeName = bestIn.typeName;
            else if (bestOut.typeName.isNotEmpty())
                typeName = bestOut.typeName;
            else if (bestIn.typeName.isNotEmpty())
                typeName = bestIn.typeName;

            // Stereo fold for PC / virtual listen path.
            wantOut = juce::jmin (wantOut, 2);
            wantIn = juce::jmin (wantIn, 2);

            // Prefer system default for speakers — exclusive "Direct hardware" often
            // fails duplex under PipeWire and leaves JamStudio with capture only.
            if (settings.preferComputerSpeakersForMonitor)
            {
                for (const auto& d : inv.outputs)
                {
                    const auto n = d.name.toLowerCase();
                    if (n.contains ("default alsa") || n == "default"
                        || n.contains ("pulse") || n.contains ("pipewire sound"))
                    {
                        outName = d.name;
                        typeName = d.typeName.isNotEmpty() ? d.typeName : typeName;
                        break;
                    }
                }
            }
            break;
        }
    }

    // Clamp open sizes every mode (settings UI can still show higher "max" for future).
    wantIn = juce::jlimit (0, 8, wantIn);
    wantOut = juce::jlimit (2, kMaxMixChannels, juce::jmax (2, wantOut));

    if (typeName.isEmpty() && deviceManager.getAvailableDeviceTypes().size() > 0)
        typeName = deviceManager.getAvailableDeviceTypes().getUnchecked (0)->getTypeName();

    // Empty output name → let applyChoice use defaults.
    if (outName.isEmpty())
        outName = {};

    return applyChoice (typeName, inName, outName, wantIn, wantOut);
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

        // Prefer same type as input (ALSA in + ALSA out) for split routing reliability.
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

    // Hard caps — large opens often yield capture-only streams under PipeWire.
    // Allow up to 12 outs for FOH + five band monitors.
    const int cappedWantIn = juce::jlimit (0, 8, wantInCh);
    const int cappedWantOut = juce::jlimit (2, kMaxMixChannels, juce::jmax (2, wantOutCh));

    auto tryOpen = [this] (const juce::String& type,
                           const juce::String& inDev,
                           const juce::String& outDev,
                           int numIn,
                           int numOut) -> juce::String
    {
        if (type.isNotEmpty())
            deviceManager.setCurrentAudioDeviceType (type, true);

        int maxIn = numIn;
        int maxOut = numOut;

        if (auto* devType = deviceManager.getCurrentDeviceTypeObject())
        {
            if (auto* dev = devType->createDevice (outDev, inDev))
            {
                std::unique_ptr<juce::AudioIODevice> holder (dev);
                maxIn = juce::jmax (0, holder->getInputChannelNames().size());
                maxOut = juce::jmax (0, holder->getOutputChannelNames().size());
            }
        }

        // Always insist on stereo playback if the device reports any outs.
        int finalOut = juce::jlimit (0, juce::jmax (0, maxOut), numOut);
        if (finalOut < 2 && maxOut >= 2)
            finalOut = 2;
        if (finalOut < 1 && maxOut > 0)
            finalOut = juce::jmin (2, maxOut);

        int finalIn = juce::jlimit (0, juce::jmax (0, maxIn), numIn);
        if (finalIn < 1 && maxIn > 0)
            finalIn = juce::jmin (2, maxIn);

        juce::AudioDeviceManager::AudioDeviceSetup setup;
        setup.inputDeviceName = inDev;
        setup.outputDeviceName = outDev;
        setup.useDefaultInputChannels = false;
        setup.useDefaultOutputChannels = false;
        enableFirstNChannels (setup.inputChannels, finalIn);
        enableFirstNChannels (setup.outputChannels, finalOut);

        if (settings.preferredSampleRate > 0.0)
            setup.sampleRate = settings.preferredSampleRate;
        if (settings.preferredBufferSize > 0)
            setup.bufferSize = settings.preferredBufferSize;

        // Critical: never open with zero output bits if we want to hear anything.
        if (setup.outputChannels.countNumberOfSetBits() == 0)
        {
            enableFirstNChannels (setup.outputChannels, 2);
            setup.useDefaultOutputChannels = true;
        }

        return deviceManager.setAudioDeviceSetup (setup, true);
    };

    auto activeOutCount = [this]()
    {
        if (auto* dev = deviceManager.getCurrentAudioDevice())
            return dev->getActiveOutputChannels().countNumberOfSetBits();
        return deviceManager.getAudioDeviceSetup().outputChannels.countNumberOfSetBits();
    };

    juce::String err = tryOpen (typeName, inputName, outputName, cappedWantIn, cappedWantOut);
    lastError = err;

    // Fallback 1: system default stereo (most reliable on PipeWire / Pulse).
    if (err.isNotEmpty() || activeOutCount() < 1)
    {
        err = tryOpen (typeName, {}, {}, 2, 2);
        if (err.isNotEmpty() || activeOutCount() < 1)
            deviceManager.initialiseWithDefaultDevices (2, 2);

        if (activeOutCount() < 1)
        {
            // Fallback 2: explicit default / pulse names JUCE enumerates on Linux.
            for (const auto& outTry : { juce::String ("default"),
                                        juce::String ("Default ALSA Output"),
                                        juce::String ("pulse"),
                                        juce::String ("Pulseaudio output") })
            {
                err = tryOpen (typeName.isNotEmpty() ? typeName : juce::String ("ALSA"),
                               {}, outTry, 2, 2);
                if (err.isEmpty() && activeOutCount() > 0)
                    break;
            }
        }

        if (activeOutCount() < 1)
            lastError = "No playback channels opened — check system audio output.";
        else if (lastError.isNotEmpty())
            lastError = "Fell back to default stereo output (" + lastError + ")";
        else
            lastError = {};
    }

    // Final sanity: if still no outs, last resort initialise.
    if (activeOutCount() < 1)
    {
        deviceManager.initialiseWithDefaultDevices (2, 2);
        lastError = "Forced default audio device (previous setup had no outputs).";
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
    if (s.availableBuses >= 6)
        busLabel = "FOH + M1–M5 (outs 1-12)";
    else if (s.availableBuses >= 4)
        busLabel = "FOH + M1–M" + juce::String (s.availableBuses - 1)
                   + " (outs 1-" + juce::String (s.availableBuses * 2) + ")";
    else if (s.availableBuses == 3)
        busLabel = "FOH + M1–M2 (outs 1-6)";
    else if (s.availableBuses == 2)
        busLabel = "FOH + M1 (outs 1-4)";
    else
        busLabel = "FOH only (stereo / PC listen)";

    s.summary = "In: " + (s.inputName.isNotEmpty() ? s.inputName : juce::String ("(none)"))
                + " (" + juce::String (s.activeInputChannels) + " ch)  |  Out: "
                + (s.outputName.isNotEmpty() ? s.outputName : juce::String ("(none)"))
                + " (" + juce::String (s.activeOutputChannels) + " ch)  |  " + busLabel;

    if (s.sampleRate > 0.0)
        s.summary += "  @ " + juce::String (s.sampleRate / 1000.0, 1) + " kHz";

    if (s.error.isNotEmpty())
        s.summary += "  [warn: " + s.error + "]";

    return s;
}

juce::String AudioInterfaceManager::getStatusSummary() const
{
    return getSnapshot().summary;
}

int AudioInterfaceManager::scoreAsCaptureInterface (const juce::String& name,
                                                    const int maxIn,
                                                    const int maxOut)
{
    auto n = name.toLowerCase();
    int score = maxIn * 12;

    const char* brands[] = {
        "scarlett", "focusrite", "motu", "rme", "presonus", "behringer", "audient",
        "universal audio", "apollo", "komplete audio", "m-audio", "steinberg", "ur22",
        "ur44", "babyface", "fireface", "zoom uac", "ssl 2", "volta", "claw"
    };
    for (auto* b : brands)
        if (n.contains (b))
            score += 140;

    if (n.contains ("usb") || n.contains ("interface") || n.contains ("audio box"))
        score += 40;
    if (maxIn >= 4)
        score += 60;
    if (maxIn >= 8)
        score += 40;
    if (maxIn > maxOut)
        score += 25;

    // Deprioritise monitors / HDMI / loopback as capture sources.
    if (n.contains ("hdmi") || n.contains ("display") || n.contains ("loopback")
        || n.contains ("monitor of") || n.contains ("null"))
        score -= 250;

    if (n.contains ("default") || n.contains ("pulse"))
        score -= 15; // fine fallback, not preferred over real interfaces

    return score;
}

int AudioInterfaceManager::scoreAsComputerMonitor (const juce::String& name, const int maxOut)
{
    auto n = name.toLowerCase();
    int score = juce::jmin (maxOut, 2) * 8;

    // System default path is the most reliable under PipeWire.
    if (n.contains ("default alsa") || n == "default" || n.startsWith ("default "))
        score += 160;
    if (n.contains ("pulse") || n.contains ("pipewire sound"))
        score += 140;

    if (n.contains ("pch") || n.contains ("hda") || n.contains ("realtek")
        || n.contains ("built-in") || n.contains ("built in") || n.contains ("internal")
        || n.contains ("speakers") || n.contains ("macbook") || n.contains ("notebook"))
        score += 90;

    // Prefer plug devices over exclusive "direct hardware" (hw:) which breaks duplex.
    if (n.contains ("direct hardware"))
        score -= 80;
    if (n.contains ("analog") && ! n.contains ("direct"))
        score += 20;

    if (n.contains ("hdmi") || n.contains ("displayport") || n.contains ("display"))
        score -= 40;

    // Multi-IO / virtual Scarlett test sinks are not PC speaker monitors.
    if (looksLikeMultiIoInterface (name, 0, maxOut) || maxOut >= 8)
        score -= 80;

    const char* brands[] = {
        "scarlett", "focusrite", "motu", "rme", "presonus", "behringer", "audient"
    };
    for (auto* b : brands)
        if (n.contains (b))
            score -= 150;

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

} // namespace jamstudio::audio
