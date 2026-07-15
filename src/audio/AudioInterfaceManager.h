#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/** How JamStudio chooses capture vs monitor devices. */
enum class AudioRoutingMode
{
    /** Detect multi-input interfaces for capture; prefer computer speakers for monitor. */
    plugAndPlay = 0,
    /** Same device for input and output (classic interface / multi-out stage). */
    sameDevice,
    /** Explicit input + output names chosen by the user. */
    manual
};

[[nodiscard]] inline juce::String audioRoutingModeToString (AudioRoutingMode mode)
{
    switch (mode)
    {
        case AudioRoutingMode::plugAndPlay: return "plugAndPlay";
        case AudioRoutingMode::sameDevice: return "sameDevice";
        case AudioRoutingMode::manual: return "manual";
    }
    return "plugAndPlay";
}

[[nodiscard]] inline AudioRoutingMode audioRoutingModeFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("sameDevice")) return AudioRoutingMode::sameDevice;
    if (s.equalsIgnoreCase ("manual")) return AudioRoutingMode::manual;
    return AudioRoutingMode::plugAndPlay;
}

struct AudioInterfaceSettings
{
    AudioRoutingMode mode = AudioRoutingMode::plugAndPlay;
    /** When true (default), multi-input USB interfaces feed capture while speakers monitor. */
    bool preferComputerSpeakersForMonitor = true;
    juce::String deviceTypeName;       // empty = pick best available type
    juce::String preferredInputName;   // empty = auto / none
    juce::String preferredOutputName;  // empty = auto / none
    int maxInputChannels = 2;          // channels to open (matched down to hardware)
    int maxOutputChannels = 6;         // FOH+Mon A+Mon B; matched down to hardware
    double preferredSampleRate = 0.0;  // 0 = device default
    int preferredBufferSize = 0;       // 0 = device default

    [[nodiscard]] static juce::File settingsFile();
    void load();
    void save() const;
};

struct AudioDeviceChoice
{
    juce::String typeName;
    juce::String name;
    int maxInputChannels = 0;
    int maxOutputChannels = 0;
    int score = 0;
};

struct AudioRoutingSnapshot
{
    juce::String deviceTypeName;
    juce::String inputName;
    juce::String outputName;
    int activeInputChannels = 0;
    int activeOutputChannels = 0;
    int availableBuses = 1; // stereo pairs used for FOH/Mon A/Mon B
    double sampleRate = 0.0;
    int bufferSize = 0;
    juce::String error;
    juce::String summary; // human-readable status line
};

/**
 * Plug-and-play audio routing for practice / stage:
 * - Scans available devices (ALSA/JACK/WASAPI/…)
 * - Matches channel counts to hardware
 * - Hot-plugs USB interfaces (e.g. Scarlett) without a full restart
 *
 * Note: class-compliant USB interfaces do NOT report whether TRS cables are
 * physically plugged into line outs. "No cables on the Scarlett" is handled by
 * routing monitor to the computer speakers (preferComputerSpeakersForMonitor).
 */
class AudioInterfaceManager : public juce::ChangeBroadcaster,
                              private juce::Timer
{
public:
    explicit AudioInterfaceManager (juce::AudioDeviceManager& deviceManager);
    ~AudioInterfaceManager() override;

    void initialiseAtStartup();
    void rescanAndApply();
    [[nodiscard]] juce::String applySettings (const AudioInterfaceSettings& settings);

    [[nodiscard]] const AudioInterfaceSettings& getSettings() const noexcept { return settings; }
    void setSettings (const AudioInterfaceSettings& newSettings, bool applyNow = true);

    [[nodiscard]] AudioRoutingSnapshot getSnapshot() const;
    [[nodiscard]] juce::String getStatusSummary() const;

    [[nodiscard]] juce::Array<AudioDeviceChoice> listInputDevices() const;
    [[nodiscard]] juce::Array<AudioDeviceChoice> listOutputDevices() const;
    [[nodiscard]] juce::StringArray listDeviceTypeNames() const;

    /** Refresh ALSA/WASAPI device lists (USB plug events). */
    void scanHardware();

private:
    void timerCallback() override;

    struct DeviceInventory
    {
        juce::String fingerprint;
        juce::Array<AudioDeviceChoice> inputs;
        juce::Array<AudioDeviceChoice> outputs;
    };

    [[nodiscard]] DeviceInventory buildInventory() const;
    [[nodiscard]] AudioDeviceChoice pickBestInput (const DeviceInventory& inv) const;
    [[nodiscard]] AudioDeviceChoice pickBestMonitorOutput (const DeviceInventory& inv,
                                                           const AudioDeviceChoice& chosenInput) const;
    [[nodiscard]] AudioDeviceChoice pickSameDevicePair (const DeviceInventory& inv) const;
    [[nodiscard]] juce::String applyChoice (const juce::String& typeName,
                                           const juce::String& inputName,
                                           const juce::String& outputName,
                                           int wantInCh,
                                           int wantOutCh);

    static int scoreAsCaptureInterface (const juce::String& name, int maxIn, int maxOut);
    static int scoreAsComputerMonitor (const juce::String& name, int maxOut);
    static bool looksLikeMultiIoInterface (const juce::String& name, int maxIn, int maxOut);

    juce::AudioDeviceManager& deviceManager;
    AudioInterfaceSettings settings;
    juce::String lastFingerprint;
    juce::String lastError;
    bool suppressRescan = false;
};

} // namespace jamstudio::audio
