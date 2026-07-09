#pragma once

#include "../audio/TransportController.h"
#include "MidiMappingProfile.h"

namespace jamstudio::midi
{

/**
 * Listens to USB/class-compliant MIDI controllers and drives the mixer + transport.
 * Works with any MIDI device that sends CCs/notes; pick a built-in profile or MIDI Learn.
 */
class MidiControlSurface : private juce::MidiInputCallback,
                           private juce::AsyncUpdater
{
public:
    using StatusCallback = std::function<void (const juce::String& message)>;
    using UiRefreshCallback = std::function<void()>;
    using RecordToggleCallback = std::function<void()>;

    MidiControlSurface (juce::AudioDeviceManager& deviceManager,
                        jamstudio::audio::TransportController& transport);
    ~MidiControlSurface() override;

    void setEnabled (bool shouldBeEnabled);
    [[nodiscard]] bool isEnabled() const noexcept { return settings.enabled; }

    void setInputDeviceIdentifier (const juce::String& identifier);
    [[nodiscard]] juce::String getInputDeviceIdentifier() const { return settings.inputDeviceIdentifier; }

    void applyProfile (const MidiMappingProfile& profile);
    void setUseCustomProfile (bool useCustom);
    void setCustomBinding (const MidiBinding& binding);
    void clearCustomBindings();

    [[nodiscard]] const MidiControlSettings& getSettings() const noexcept { return settings; }
    [[nodiscard]] MidiMappingProfile getActiveProfile() const { return settings.activeProfile(); }
    [[nodiscard]] MidiControlSettings& getSettingsForEdit() noexcept { return settings; }

    void loadSettings();
    void saveSettings() const;
    void refreshMidiDevices();

    // MIDI Learn: next matching message binds to target
    void beginLearn (MidiTarget target);
    void cancelLearn();
    [[nodiscard]] bool isLearning() const noexcept { return learnTarget != MidiTarget::none; }
    [[nodiscard]] MidiTarget getLearnTarget() const noexcept { return learnTarget; }

    /** Inject a message without hardware (for testing). */
    void handleIncomingMessageForTest (const juce::MidiMessage& message);

    void setStatusCallback (StatusCallback cb) { statusCallback = std::move (cb); }
    void setUiRefreshCallback (UiRefreshCallback cb) { uiRefreshCallback = std::move (cb); }
    void setRecordToggleCallback (RecordToggleCallback cb) { recordToggleCallback = std::move (cb); }

    [[nodiscard]] juce::String getLastMessageDescription() const;

private:
    void handleIncomingMidiMessage (juce::MidiInput* source, const juce::MidiMessage& message) override;
    void handleAsyncUpdate() override;
    void processMessage (const juce::MidiMessage& message);
    void applyBinding (const MidiBinding& binding, float normalizedValue, bool isPress);
    void enableAllOrSelectedInputs();
    void disableAllMidiInputs();
    void postStatus (const juce::String& text);

    juce::AudioDeviceManager& deviceManager;
    jamstudio::audio::TransportController& transport;
    MidiControlSettings settings;

    MidiTarget learnTarget = MidiTarget::none;
    juce::String lastMessageDescription;
    juce::CriticalSection messageLock;
    juce::Array<juce::MidiMessage> pendingMessages;

    StatusCallback statusCallback;
    UiRefreshCallback uiRefreshCallback;
    RecordToggleCallback recordToggleCallback;
};

} // namespace jamstudio::midi
