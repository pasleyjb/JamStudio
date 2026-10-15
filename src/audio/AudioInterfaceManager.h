#pragma once

#include "MixOutputTap.h"

#include <JuceHeader.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

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

    /** PipeWire sink names for FOH / Mon A / Mon B. Empty sends that mix nowhere. */
    juce::String fohSink;
    juce::String monitorASink;
    juce::String monitorBSink;
    /** False until the user (or first-run defaults) has stored the three sinks. */
    bool busOutputsSpecified = false;

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

/** One hardware playback sink the user can send a mix to. */
struct PlaybackSink
{
    juce::String name;
    juce::String description;
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
 * - Scans available devices (Core Audio, ALSA/JACK, WASAPI, …)
 * - Matches channel counts to hardware
 * - Hot-plugs USB interfaces (e.g. Scarlett) without a full restart
 *
 * Note: class-compliant USB interfaces do NOT report whether TRS cables are
 * physically plugged into line outs. "No cables on the Scarlett" is handled by
 * routing monitor to the computer speakers (preferComputerSpeakersForMonitor).
 */
struct DeviceFanout;

class AudioInterfaceManager : public juce::ChangeBroadcaster,
                              public MixOutputTap,
                              private juce::Timer
{
public:
    explicit AudioInterfaceManager (juce::AudioDeviceManager& deviceManager);
    ~AudioInterfaceManager() override;

    /** Write the 6-channel ALSA bus and make sure its PipeWire sink exists.
        Call this before the first ALSA device scan. */
    static void installBusPlaybackDevice();

    void initialiseAtStartup();
    void rescanAndApply();
    [[nodiscard]] juce::String applySettings (const AudioInterfaceSettings& settings);

    [[nodiscard]] const AudioInterfaceSettings& getSettings() const noexcept { return settings; }
    void setSettings (const AudioInterfaceSettings& newSettings, bool applyNow = true);

    [[nodiscard]] AudioRoutingSnapshot getSnapshot() const;
    [[nodiscard]] juce::String getStatusSummary() const;

    [[nodiscard]] juce::Array<AudioDeviceChoice> listInputDevices() const;
    [[nodiscard]] juce::Array<AudioDeviceChoice> listOutputDevices() const;
    [[nodiscard]] juce::Array<PlaybackSink> listPlaybackSinks() const;
    [[nodiscard]] juce::StringArray listDeviceTypeNames() const;

    /** Keep FOH / Mon A / Mon B connected while transport is playing or counting in.
        Silence releases the links so an idle headphone jack does not stay open. */
    void setMixOutputsHeld (bool held);

    /** Point FOH / Mon A / Mon B at playback sinks. Empty means Off.
        Reopens the device only if the mix bus is not already the output. */
    void setBusDestinations (const juce::String& foh,
                             const juce::String& monitorA,
                             const juce::String& monitorB);

    /** Refresh ALSA/WASAPI device lists (USB plug events). */
    void scanHardware();

    void sendMix (const float* const* mixChannels,
                  int numMixChannels,
                  int numSamples,
                  float* const* deviceOut,
                  int numDeviceOut) noexcept override;

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

    [[nodiscard]] bool anyBusDestination() const noexcept;
    void assignDefaultBusOutputsIfNeeded();
    void ensureBusGraph();
    void syncBusLoopbacks();
    void syncDeviceFanout();
    void rememberBusStatus();
    void stopBusCopy (int index);
    void startBusCopy (int index, const juce::String& source, const juce::String& sink);

    struct BusCopy
    {
        juce::String source;
        juce::String sink;
        int pid = 0;
        std::unique_ptr<juce::ChildProcess> process;
    };

    std::array<BusCopy, 3> busCopies;

    juce::AudioDeviceManager& deviceManager;
    AudioInterfaceSettings settings;
    juce::String lastFingerprint;
    juce::String lastError;
    juce::String busStatusText;
    bool suppressRescan = false;
    std::atomic<bool> outputsHeld { false };
    // Bit 0..2: that mix bus is summed into the open output device.
    std::atomic<std::uint8_t> mainBusMask { 1 };
    std::atomic<bool> mainGetsAllBuses { false };
    bool loopbacksOpen = false;
    int silenceTicks = 0;
    std::unique_ptr<DeviceFanout> deviceFanout;
};

} // namespace jamstudio::audio
