#pragma once

#include "../ai/AiToolsCatalog.h"
#include "../ai/BasicPitchTranscriber.h"
#include "../ai/DemucsSeparator.h"
#include "../ai/WhisperTranscriber.h"
#include "../audio/AudioInterfaceManager.h"
#include "../audio/AudioRecorder.h"
#include "../audio/LiveToneEngine.h"
#include "../audio/RecordingExporter.h"
#include "../audio/RecordingTakeManager.h"
#include "../audio/TransportController.h"
#include "../midi/MidiControlSurface.h"
#include "../notation/LyricsView.h"
#include "../notation/LyricsTrack.h"
#include "../notation/NotationView.h"
#include "../notation/Score.h"
#include "../ui/NotationHeaderBar.h"
#include "../ui/FullPageTabsWindow.h"
#include "../ui/FullPageLyricsWindow.h"
#include "../ui/JamStudioTheme.h"
#include "../ui/FloatingWindowDock.h"
#include "../ui/HelpBrowserDialog.h"
#include "../ui/MixerWindow.h"
#include "../ui/StageFxControllerWindow.h"
#include "../ui/SeparationProgressBar.h"
#include "../ui/StemLane.h"
#include "../ui/StartupWizard.h"
#include "../ui/ToolbarTabs.h"
#include "../ui/TranscriptionCorrectionDialog.h"
#include "../ui/TransportBar.h"
#include "../ui/PerformanceBar.h"
#include "../ui/PerformanceStagePanel.h"
#include "../ui/RecordingTakesPanel.h"
#include "../ui/SetListEditorDialog.h"
#include "../ui/VideoOutputWindow.h"
#include "../project/RecentProjects.h"
#include "../performance/SetListData.h"
#include "../performance/SetListManager.h"
#include "../performance/ToneProfile.h"
#include "../ui/WaveformDisplay.h"
#include "PracticeSetupPipeline.h"

namespace jamstudio::app
{

class MainComponent : public juce::Component,
                      public juce::ChangeListener,
                      public juce::DarkModeSettingListener
{
public:
    explicit MainComponent (juce::AudioDeviceManager& deviceManager);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void darkModeSettingChanged() override;

    [[nodiscard]] juce::StringArray buildMenuBarNames();
    [[nodiscard]] juce::PopupMenu buildMenuForIndex (int topLevelMenuIndex, const juce::String& menuName);
    void handleMenuCommand (int menuItemID, int topLevelMenuIndex);

private:
    enum MenuCommand
    {
        openSongCmd = 1,
        saveProjectCmd,
        loadProjectCmd,
        recentProjectsCmd,
        welcomeWizardCmd,
        newPracticeSessionCmd,
        quitCmd,
        separateStemsCmd,
        browseTabLibraryCmd,
        importScoreCmd,
        showTabViewCmd,
        showSheetViewCmd,
        openFullPageTabsCmd,
        openFullPageLyricsCmd,
        aiTabCmd,
        importLyricsCmd,
        onlineLyricsCmd,
        aiLyricsCmd,
        recordCmd,
        openExternalRecorderCmd,
        openArdourStudioCmd,
        importTakeCmd,
        detectTempoCmd,
        toggleCountInCmd,
        aboutCmd,
        helpInstructionsCmd,
        aiToolsCmd,
        midiControlCmd,
        audioSettingsCmd,
        toggleLyricsPanelCmd,
        toggleNotationPanelCmd,
        toggleStemsPanelCmd,
        toggleMixerWindowCmd,
        toggleStageFxControllerCmd,
        dockAttachCmd,
        dockDetachCmd,
        dockSideRightCmd,
        dockSideLeftCmd,
        dockSideTopCmd,
        dockSideBottomCmd,
        dockAutoStickCmd,
        dockDetachOnMaxCmd,
        dockGapTightCmd,
        dockGapNormalCmd,
        dockGapWideCmd,
        editSetListCmd,
        openStageShowBuilderCmd,
        performanceNextSongCmd,
        performanceGoLiveCmd,
        performanceBackSetupCmd,
        stopPerformanceCmd,
        openKaraokeOutputCmd,
        openStageFxOutputCmd,
        cycleKaraokeDisplayCmd,
        cycleStageFxDisplayCmd
    };

    void openSong();
    void saveProject();
    void loadProject();
    void loadProjectFile (const juce::File& file);
    /** Re-run Demucs when a project's separated stems are missing; packs into .media and re-saves. */
    void recoverMissingProjectStems (const juce::File& projectFile);
    void showRecentProjectsMenu();
    void browseTabLibrary();
    void importScore();
    void importScoreFile (const juce::File& file, const juce::String& displayName);
    void importLyrics();
    void findOnlineLyrics();
    void applyScore (const jamstudio::notation::Score& score, bool replaceLyricsFromScore);
    void setNotationDisplayMode (jamstudio::notation::NotationMode mode);
    void toggleTabView();
    void toggleSheetView();
    void openFullPageTabs();
    void openFullPageLyrics();
    void setActiveScorePart (int partIndex);
    void updateNotationPanelVisibility();
    void syncNotationUiState();
    void separateStems();
    void transcribeLyrics();
    void transcribeTab();
    void toggleRecording();
    void openExternalRecorder();
    void openArdourStudio();
    void importTakeFromFile();
    [[nodiscard]] juce::File findStemFileForType (jamstudio::audio::StemType preferredType);
    [[nodiscard]] juce::File findMelodicStemFile();
    void loadRecordingAsStem (const juce::File& recordingFile, const juce::String& displayName);
    void loadStemsIntoMixer (const juce::Array<juce::File>& stemFiles);
    void rebuildStemLanes();
    void rebuildMixerWindow();
    void setStatus (const juce::String& message);
    void refreshTheme();
    void showAiToolsSetup();
    void showMidiControlSetup();
    void showAudioSettings();
    void refreshMixerUiFromMidi();
    void refreshAudioRoutingStatus();
    void detectTempoFromSong (const juce::File& audioFile, bool announceResult);
    void beginBackgroundTask (const juce::String& message, std::function<void()> onCancel);
    void endBackgroundTask();
    [[nodiscard]] juce::Array<jamstudio::ai::AiToolInfo> getAiToolStatuses() const;
    [[nodiscard]] juce::File getDefaultRecordingFile() const;

    void updatePanelToggleStates();
    void applyPanelVisibility();
    void revealWorkspacePanels();
    void toggleLyricsPanel();
    void toggleNotationPanel();
    void toggleStemsPanel();
    void toggleMixerWindow();
    void toggleStageFxController();
    void layoutStemLanes();
    void layoutStatusBar (juce::Rectangle<int> statusBar);

    void setupStartupWizard();
    void hideStartupWizard();
    /** Re-show the welcome wizard (mode picker) from menus while the app is running. */
    void showStartupWizard();
    /** Ensure project/song opens leave the wizard and enter a usable workspace. */
    void ensureWorkspaceForProjectOpen();
    void enterWorkspaceMode (jamstudio::ui::StartupWizard::Mode mode);
    void handlePracticeChoice (jamstudio::ui::StartupWizard::PracticeChoice choice);
    void handleRecordingChoice (jamstudio::ui::StartupWizard::RecordingChoice choice);
    void enterRecordingWorkspace();
    void refreshRecordingTakesPanel();
    void startPracticeFromSongFile (const juce::File& songFile);
    void applyPracticeSetupResult (PracticeSetupResult result);
    void autoSavePracticeProject (const juce::String& projectTitle);
    [[nodiscard]] static juce::File getProjectsDirectory();

    // Performance mode (Setup + On Stage Live)
    void openSetListEditor (bool stageShowBuilder = false);
    void startPerformanceMode (jamstudio::performance::SetList list);
    void stopPerformanceMode();
    void enterPerformanceSetup();
    void enterPerformanceLive();
    void applyPerformanceWorkspaceLayout();
    void performanceTriggerNext();
    void loadPerformanceSong (int index, bool autoPlay);
    void onPerformanceSongEnded();
    void applyPerformanceStemPrefsForCurrentSong();
    void applyPerformanceTonesForCurrentSong();
    void saveCurrentSongTonesToSetlist();
    void saveCurrentMixerToSetlistTrack();
    void updateMixerPerformanceContext();
    void loadSongStageMedia (const jamstudio::performance::SetListSong& song);
    void updatePerformanceBar();
    void preferLeadTabPart (const juce::String& partHint);
    void openKaraokeOutput();
    void openStageFxOutput();
    void cycleKaraokeDisplay();
    void cycleStageFxDisplay();
    void syncVideoOutputs();

    juce::AudioDeviceManager& audioDeviceManager;
    jamstudio::audio::AudioInterfaceManager audioInterfaceManager;
    jamstudio::audio::TransportController transportController;
    jamstudio::midi::MidiControlSurface midiControlSurface;
    jamstudio::audio::AudioRecorder audioRecorder;
    jamstudio::audio::RecordingExporter recordingExporter;
    jamstudio::audio::RecordingTakeManager recordingTakeManager;
    jamstudio::ai::DemucsSeparator demucsSeparator;
    jamstudio::ai::WhisperTranscriber whisperTranscriber;
    jamstudio::ai::BasicPitchTranscriber basicPitchTranscriber;
    jamstudio::project::RecentProjects recentProjects;
    jamstudio::notation::Score currentScore;
    jamstudio::notation::LyricsTrack currentLyrics;

    juce::AudioThumbnailCache thumbnailCache { 16 };

    /** Kept for internal callbacks only - never shown (workspace chrome is menus + View). */
    jamstudio::ui::ToolbarTabs toolbarTabs;
    jamstudio::ui::StartupWizard startupWizard;
    std::unique_ptr<PracticeSetupPipeline> practiceSetupPipeline;
    juce::Label statusLabel;
    juce::TextButton statusCancelButton { "Cancel" };
    /** Kept for code compatibility - never shown (status bar carries messages). */
    jamstudio::ui::SeparationProgressBar separationProgress;
    juce::Label lyricsSectionLabel { {}, "LYRICS" };
    juce::Label notationSectionLabel { {}, "TABS / NOTATION" };
    juce::Label stemsSectionLabel { {}, "STEMS" };
    jamstudio::ui::WaveformDisplay waveformDisplay;
    juce::Viewport notationViewport;
    jamstudio::ui::NotationHeaderBar notationHeaderBar;
    jamstudio::notation::LyricsView lyricsView;
    jamstudio::notation::NotationView notationView;
    jamstudio::ui::TransportBar transportBar;
    juce::Viewport stemViewport;
    juce::Component stemContainer;
    jamstudio::ui::MixerWindow mixerWindow;
    jamstudio::ui::StageFxControllerWindow stageFxController;
    jamstudio::ui::FloatingWindowDock floatingDock;
    jamstudio::ui::FullPageTabsWindow fullPageTabsWindow;
    jamstudio::ui::FullPageLyricsWindow fullPageLyricsWindow;
    jamstudio::audio::LiveToneEngine liveToneEngine;
    jamstudio::performance::ToneLibrary toneLibrary;
    jamstudio::ui::PerformanceBar performanceBar;
    jamstudio::ui::PerformanceStagePanel performanceStagePanel;
    jamstudio::ui::RecordingTakesPanel recordingTakesPanel;
    jamstudio::ui::KaraokeOutputWindow karaokeOutput;
    jamstudio::ui::StageFxOutputWindow stageFxOutput;

    jamstudio::performance::SetList performanceSetList;
    int performanceSongIndex = -1;
    bool performanceActive = false;
    bool performanceWaitingForTrigger = false;
    /** True only after a song finishes/skips - NEXT advances to the following track.
        False when the current song is pre-loaded and waiting for first play. */
    bool performanceAwaitingNextSong = false;
    bool performanceWasPlaying = false;
    jamstudio::ui::PerformanceStageMode performanceStageMode =
        jamstudio::ui::PerformanceStageMode::setup;
    int karaokeDisplayIndex = 0;
    int stageFxDisplayIndex = 1;

    juce::File currentSongFile;
    juce::File currentScoreFile;
    juce::File currentLyricsFile;
    juce::File currentProjectFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool lyricsPanelVisible = true;
    bool notationPanelVisible = true;
    /** Stem lanes with mini-waveforms under the transport (mixer still available). */
    bool stemsPanelVisible = true;
    bool workspaceReady = false;
    jamstudio::ui::StartupWizard::Mode currentMode = jamstudio::ui::StartupWizard::Mode::practice;

    std::atomic<uint32_t> backgroundTaskGeneration { 0 };
    bool backgroundTaskActive = false;
};

} // namespace jamstudio::app
