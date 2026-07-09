#include "MidiControlSurface.h"

namespace jamstudio::midi
{

MidiControlSurface::MidiControlSurface (juce::AudioDeviceManager& devices,
                                        jamstudio::audio::TransportController& transportController)
    : deviceManager (devices),
      transport (transportController)
{
    loadSettings();
    refreshMidiDevices();
}

MidiControlSurface::~MidiControlSurface()
{
    cancelLearn();
    disableAllMidiInputs();
    saveSettings();
}

void MidiControlSurface::loadSettings()
{
    settings.load();
}

void MidiControlSurface::saveSettings() const
{
    settings.save();
}

void MidiControlSurface::setEnabled (const bool shouldBeEnabled)
{
    settings.enabled = shouldBeEnabled;
    if (shouldBeEnabled)
        enableAllOrSelectedInputs();
    else
        disableAllMidiInputs();
    saveSettings();
    postStatus (shouldBeEnabled ? "MIDI control enabled." : "MIDI control disabled.");
}

void MidiControlSurface::setInputDeviceIdentifier (const juce::String& identifier)
{
    settings.inputDeviceIdentifier = identifier;
    refreshMidiDevices();
    saveSettings();
}

void MidiControlSurface::applyProfile (const MidiMappingProfile& profile)
{
    settings.profileId = profile.id;
    settings.useCustomBindings = (profile.id == "custom");
    if (settings.useCustomBindings)
        settings.customProfile = profile;
    saveSettings();
    postStatus ("MIDI profile: " + profile.name);
}

void MidiControlSurface::setUseCustomProfile (const bool useCustom)
{
    settings.useCustomBindings = useCustom;
    if (useCustom && settings.customProfile.id.isEmpty())
    {
        settings.customProfile = settings.activeProfile();
        settings.customProfile.id = "custom";
        settings.customProfile.name = "Custom (MIDI Learn)";
    }
    saveSettings();
}

void MidiControlSurface::setCustomBinding (const MidiBinding& binding)
{
    settings.useCustomBindings = true;
    if (settings.customProfile.id.isEmpty())
    {
        settings.customProfile = getActiveProfile();
        settings.customProfile.id = "custom";
        settings.customProfile.name = "Custom (MIDI Learn)";
    }
    settings.customProfile.addBinding (binding);
    saveSettings();
}

void MidiControlSurface::clearCustomBindings()
{
    settings.customProfile.clear();
    settings.customProfile.id = "custom";
    settings.customProfile.name = "Custom (MIDI Learn)";
    settings.useCustomBindings = false;
    saveSettings();
}

void MidiControlSurface::beginLearn (const MidiTarget target)
{
    learnTarget = target;
    postStatus ("MIDI Learn: move a fader or press a button for \""
                + midiTargetToString (target) + "\"…");
}

void MidiControlSurface::cancelLearn()
{
    if (learnTarget != MidiTarget::none)
    {
        learnTarget = MidiTarget::none;
        postStatus ("MIDI Learn cancelled.");
    }
}

void MidiControlSurface::disableAllMidiInputs()
{
    for (const auto& device : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (device.identifier, false);
        deviceManager.removeMidiInputDeviceCallback (device.identifier, this);
    }
}

void MidiControlSurface::enableAllOrSelectedInputs()
{
    disableAllMidiInputs();

    if (! settings.enabled)
        return;

    const auto devices = juce::MidiInput::getAvailableDevices();

    for (const auto& device : devices)
    {
        const auto useThis = settings.inputDeviceIdentifier.isEmpty()
                             || settings.inputDeviceIdentifier == device.identifier
                             || settings.inputDeviceIdentifier == "all";

        if (! useThis)
            continue;

        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        deviceManager.addMidiInputDeviceCallback (device.identifier, this);
    }

    if (devices.isEmpty())
        postStatus ("No MIDI input devices found. Plug in a USB controller or create a virtual port.");
}

void MidiControlSurface::refreshMidiDevices()
{
    if (settings.enabled)
        enableAllOrSelectedInputs();
    else
        disableAllMidiInputs();
}

void MidiControlSurface::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    {
        const juce::ScopedLock lock (messageLock);
        pendingMessages.add (message);

        while (pendingMessages.size() > 64)
            pendingMessages.remove (0);
    }

    triggerAsyncUpdate();
}

void MidiControlSurface::handleIncomingMessageForTest (const juce::MidiMessage& message)
{
    handleIncomingMidiMessage (nullptr, message);
}

void MidiControlSurface::handleAsyncUpdate()
{
    juce::Array<juce::MidiMessage> batch;

    {
        const juce::ScopedLock lock (messageLock);
        batch.swapWith (pendingMessages);
    }

    for (const auto& message : batch)
        processMessage (message);
}

juce::String MidiControlSurface::getLastMessageDescription() const
{
    return lastMessageDescription;
}

void MidiControlSurface::processMessage (const juce::MidiMessage& message)
{
    if (! settings.enabled && learnTarget == MidiTarget::none)
        return;

    // Ignore active sensing / clock clutter in status line
    if (message.isActiveSense() || message.isMidiClock() || message.isMidiStart()
        || message.isMidiStop() || message.isMidiContinue())
        return;

    const auto channel = message.getChannel() > 0 ? message.getChannel() - 1 : -1;

    juce::String desc;
    MidiBinding::Type type = MidiBinding::Type::cc;
    int number = 0;
    float norm = 0.0f;
    bool isPress = false;
    bool usable = false;

    if (message.isController())
    {
        type = MidiBinding::Type::cc;
        number = message.getControllerNumber();
        norm = static_cast<float> (message.getControllerValue()) / 127.0f;
        isPress = message.getControllerValue() >= 64;
        usable = true;
        desc = "CC " + juce::String (number) + " = " + juce::String (message.getControllerValue())
               + " (ch " + juce::String (message.getChannel()) + ")";
    }
    else if (message.isNoteOn() || message.isNoteOff())
    {
        type = MidiBinding::Type::note;
        number = message.getNoteNumber();
        isPress = message.isNoteOn() && message.getVelocity() > 0;
        norm = isPress ? 1.0f : 0.0f;
        usable = true;
        desc = (isPress ? "Note On " : "Note Off ") + juce::String (number)
               + " vel " + juce::String (message.getVelocity())
               + " (ch " + juce::String (message.getChannel()) + ")";
    }
    else if (message.isPitchWheel())
    {
        type = MidiBinding::Type::pitchBend;
        number = 0;
        // 0..16383 → 0..1 (centre 8192)
        norm = static_cast<float> (message.getPitchWheelValue()) / 16383.0f;
        isPress = false;
        usable = true;
        desc = "PitchBend " + juce::String (message.getPitchWheelValue())
               + " (ch " + juce::String (message.getChannel()) + ")";
    }

    if (! usable)
        return;

    lastMessageDescription = desc;

    // MIDI Learn captures next usable message
    if (learnTarget != MidiTarget::none && (message.isController() || message.isNoteOn() || message.isPitchWheel()))
    {
        if (message.isNoteOff())
            return;

        MidiBinding learned;
        learned.type = type;
        learned.channel = channel;
        learned.number = number;
        learned.target = learnTarget;
        learned.toggle = (type == MidiBinding::Type::note)
                         || (type == MidiBinding::Type::cc
                             && (stemIndexFromMuteTarget (learnTarget) >= 0
                                 || stemIndexFromSoloTarget (learnTarget) >= 0
                                 || learnTarget == MidiTarget::recordToggle
                                 || learnTarget == MidiTarget::metronomeToggle));

        setCustomBinding (learned);
        const auto targetName = midiTargetToString (learnTarget);
        learnTarget = MidiTarget::none;
        postStatus ("Learned: " + learned.describe());
        lastMessageDescription = "LEARNED " + targetName + " ← " + desc;
        return;
    }

    if (! settings.enabled)
        return;

    const auto profile = getActiveProfile();
    const auto* binding = profile.findBinding (type, channel, number);

    if (binding == nullptr)
    {
        // Still show activity for debugging
        return;
    }

    applyBinding (*binding, norm, isPress);
}

void MidiControlSurface::applyBinding (const MidiBinding& binding,
                                       float normalizedValue,
                                       const bool isPress)
{
    auto value = juce::jlimit (0.0f, 1.0f, binding.invert ? (1.0f - normalizedValue) : normalizedValue);
    auto& mixer = transport.getStemMixer();
    auto needsUi = false;

    auto applyToggleButton = [&] (const bool currentlyOn, auto setFn)
    {
        if (binding.toggle)
        {
            if (isPress)
            {
                setFn (! currentlyOn);
                needsUi = true;
            }
        }
        else
        {
            setFn (value >= 0.5f);
            needsUi = true;
        }
    };

    const auto volStem = stemIndexFromVolumeTarget (binding.target);
    if (volStem >= 0)
    {
        if (volStem < mixer.getNumStems())
        {
            mixer.setStemVolume (volStem, value);
            needsUi = true;
        }
    }
    else if (const auto muteStem = stemIndexFromMuteTarget (binding.target); muteStem >= 0)
    {
        if (muteStem < mixer.getNumStems())
        {
            const auto* stem = mixer.getStem (muteStem);
            applyToggleButton (stem != nullptr && stem->isMuted(),
                               [&] (bool on) { mixer.setStemMuted (muteStem, on); });
        }
    }
    else if (const auto soloStem = stemIndexFromSoloTarget (binding.target); soloStem >= 0)
    {
        if (soloStem < mixer.getNumStems())
        {
            const auto* stem = mixer.getStem (soloStem);
            applyToggleButton (stem != nullptr && stem->isSolo(),
                               [&] (bool on) { mixer.setStemSolo (soloStem, on); });
        }
    }
    else
    {
        switch (binding.target)
        {
            case MidiTarget::masterVolume:
                mixer.setMasterVolume (value);
                needsUi = true;
                break;

            case MidiTarget::play:
                if (isPress || binding.type == MidiBinding::Type::cc)
                {
                    transport.play();
                    needsUi = true;
                }
                break;

            case MidiTarget::pause:
                if (isPress || binding.type == MidiBinding::Type::cc)
                {
                    transport.pause();
                    needsUi = true;
                }
                break;

            case MidiTarget::stop:
                if (isPress || binding.type == MidiBinding::Type::cc)
                {
                    transport.stop();
                    needsUi = true;
                }
                break;

            case MidiTarget::togglePlayPause:
                if (isPress || (binding.type == MidiBinding::Type::cc && value >= 0.5f))
                {
                    transport.togglePlayPause();
                    needsUi = true;
                }
                break;

            case MidiTarget::recordToggle:
                if (binding.toggle ? isPress : value >= 0.5f)
                {
                    if (recordToggleCallback != nullptr)
                        recordToggleCallback();
                    needsUi = true;
                }
                break;

            case MidiTarget::metronomeToggle:
                if (binding.toggle ? isPress : true)
                {
                    auto& metro = transport.getMetronome();
                    if (binding.toggle)
                    {
                        if (isPress)
                            metro.setEnabled (! metro.isEnabled());
                    }
                    else
                    {
                        metro.setEnabled (value >= 0.5f);
                    }
                    needsUi = true;
                }
                break;

            case MidiTarget::nextSong:
                if (isPress || (binding.type == MidiBinding::Type::cc && value >= 0.5f))
                {
                    if (nextSongCallback != nullptr)
                        nextSongCallback();
                    needsUi = true;
                }
                break;

            default:
                break;
        }
    }

    if (needsUi && uiRefreshCallback != nullptr)
        uiRefreshCallback();
}

void MidiControlSurface::postStatus (const juce::String& text)
{
    if (statusCallback != nullptr)
        statusCallback (text);
}

} // namespace jamstudio::midi
