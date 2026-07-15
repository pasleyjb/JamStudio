#pragma once

#include "../audio/LiveToneEngine.h"
#include "../performance/SetListData.h"
#include "../performance/ToneProfile.h"

#include <JuceHeader.h>
#include <functional>

namespace jamstudio::ui
{

/** Performance has two faces: edit the show, then run it. */
enum class PerformanceStageMode
{
    setup, // build tones, assign profiles, dry-run
    live   // stage manager — automate the set
};

/**
 * Focused performance workspace:
 *  - Setup: full 3-path NAM-style rack + profile save/assign
 *  - Live: compact rack meters + stage manager cues (no lyrics/tabs)
 */
class PerformanceStagePanel : public juce::Component,
                              private juce::Timer
{
public:
    using ModeChangedCallback = std::function<void (PerformanceStageMode)>;
    using TriggerCallback = std::function<void()>;
    using SaveSongTonesCallback = std::function<void()>;
    using GoLiveCallback = std::function<void()>;
    using BackToSetupCallback = std::function<void()>;

    PerformanceStagePanel (jamstudio::audio::LiveToneEngine& engine,
                           jamstudio::performance::ToneLibrary& library);

    void setStageMode (PerformanceStageMode mode);
    [[nodiscard]] PerformanceStageMode getStageMode() const noexcept { return stageMode; }

    void setModeChangedCallback (ModeChangedCallback cb) { onModeChanged = std::move (cb); }
    void setTriggerCallback (TriggerCallback cb) { onTrigger = std::move (cb); }
    void setSaveSongTonesCallback (SaveSongTonesCallback cb) { onSaveSongTones = std::move (cb); }
    void setGoLiveCallback (GoLiveCallback cb) { onGoLive = std::move (cb); }
    void setBackToSetupCallback (BackToSetupCallback cb) { onBackToSetup = std::move (cb); }

    void setSetListInfo (const juce::String& setName,
                         int songIndex,
                         int songCount,
                         const juce::String& songTitle);
    void setPhaseMessage (const juce::String& message);
    void setWaitingForTrigger (bool waiting);
    void setUpNext (const juce::String& nextTitle);

    /** Push library profiles into path combo boxes and refresh UI from engine. */
    void refreshFromLibrary();
    void loadPathFromProfile (jamstudio::performance::LiveInstrumentRole role,
                              const jamstudio::performance::ToneProfile& profile);
    void selectProfileInCombo (jamstudio::performance::LiveInstrumentRole role,
                               const juce::String& profileId);

    /** Read knobs → ToneProfile for a path (includes name/id from current selection). */
    [[nodiscard]] jamstudio::performance::ToneProfile capturePathProfile (
        jamstudio::performance::LiveInstrumentRole role) const;

    [[nodiscard]] juce::String getSelectedProfileId (jamstudio::performance::LiveInstrumentRole role) const;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class NamPathPanel : public juce::Component
    {
    public:
        NamPathPanel (jamstudio::performance::LiveInstrumentRole role,
                      jamstudio::audio::LiveToneEngine& engine,
                      jamstudio::performance::ToneLibrary& library);

        void setCompact (bool shouldBeCompact);
        void refreshProfileList();
        void selectProfileId (const juce::String& id);
        void loadProfile (const jamstudio::performance::ToneProfile& profile);
        [[nodiscard]] jamstudio::performance::ToneProfile captureProfile() const;
        [[nodiscard]] juce::String getSelectedProfileId() const;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void timerTick(); // meters

        std::function<void()> onProfileSaved;
        std::function<void()> onProfileSelected;

    private:
        void pushParamsToEngine();
        void styleKnob (juce::Slider& s, const juce::String& name);
        void bindKnob (juce::Slider& s);
        void importNamModel();
        void chooseExistingNam();
        void openModelsFolder();
        void updateModelLabel();

        jamstudio::performance::LiveInstrumentRole role;
        jamstudio::audio::LiveToneEngine& engine;
        jamstudio::performance::ToneLibrary& library;
        bool compact = false;

        juce::Label titleLabel;
        juce::ComboBox profileBox;
        juce::TextButton saveButton { "Save" };
        juce::TextButton saveAsButton { "Save As" };
        juce::TextButton loadNamButton { "Load .nam" };
        juce::TextButton modelsFolderButton { "Models folder" };
        juce::ToggleButton enableToggle { "On" };
        juce::ToggleButton bypassToggle { "Bypass" };
        juce::Label modelLabel;
        juce::Slider inputGain, drive, bass, mid, treble, presence, outputLevel;
        juce::Label inMeterLabel, outMeterLabel;
        float inLevel = 0.0f, outLevel = 0.0f;
        juce::String currentProfileId;
        juce::String currentNamPath;
        std::unique_ptr<juce::FileChooser> fileChooser;
    };

    void timerCallback() override;
    void updateModeChrome();

    jamstudio::audio::LiveToneEngine& engine;
    jamstudio::performance::ToneLibrary& library;
    PerformanceStageMode stageMode = PerformanceStageMode::setup;

    juce::Label modeBadge;
    juce::TextButton setupModeButton { "SETUP" };
    juce::TextButton liveModeButton { "LIVE" };
    juce::TextButton goLiveButton { "GO LIVE →" };
    juce::TextButton backSetupButton { "← SETUP" };
    juce::TextButton triggerButton { "START / NEXT" };
    juce::TextButton saveSongTonesButton { "Save tones → song" };

    juce::Label setLabel;
    juce::Label songLabel;
    juce::Label phaseLabel;
    juce::Label upNextLabel;
    juce::Label hintLabel;

    std::unique_ptr<NamPathPanel> pathPanels[jamstudio::performance::kNumLiveTonePaths];

    ModeChangedCallback onModeChanged;
    TriggerCallback onTrigger;
    SaveSongTonesCallback onSaveSongTones;
    GoLiveCallback onGoLive;
    BackToSetupCallback onBackToSetup;
    bool waiting = false;
};

} // namespace jamstudio::ui
