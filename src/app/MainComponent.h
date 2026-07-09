#pragma once

#include "../ai/AiToolsCatalog.h"
#include "../ai/BasicPitchTranscriber.h"
#include "../ai/DemucsSeparator.h"
#include "../ai/WhisperTranscriber.h"
#include "../audio/AudioRecorder.h"
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
#include "../ui/JamStudioTheme.h"
#include "../ui/MixerWindow.h"
#include "../ui/SeparationProgressBar.h"
#include "../ui/StemLane.h"
#include "../ui/StartupWizard.h"
#include "../ui/ToolbarTabs.h"
#include "../ui/TranscriptionCorrectionDialog.h"
#include "../ui/TransportBar.h"
#include "../project/RecentProjects.h"
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
        quitCmd,
        separateStemsCmd,
        browseTabLibraryCmd,
        importScoreCmd,
        showTabViewCmd,
        showSheetViewCmd,
        openFullPageTabsCmd,
        aiTabCmd,
        importLyricsCmd,
        onlineLyricsCmd,
        aiLyricsCmd,
        recordCmd,
        detectTempoCmd,
        aboutCmd,
        aiToolsCmd,
        midiControlCmd,
        toggleLyricsPanelCmd,
        toggleNotationPanelCmd,
        toggleStemsPanelCmd,
        toggleMixerWindowCmd
    };

    void openSong();
    void saveProject();
    void loadProject();
    void loadProjectFile (const juce::File& file);
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
    void setActiveScorePart (int partIndex);
    void updateNotationPanelVisibility();
    void syncNotationUiState();
    void separateStems();
    void transcribeLyrics();
    void transcribeTab();
    void toggleRecording();
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
    void refreshMixerUiFromMidi();
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
    void layoutStemLanes();

    void setupStartupWizard();
    void hideStartupWizard();
    void enterWorkspaceMode (jamstudio::ui::StartupWizard::Mode mode);
    void handlePracticeChoice (jamstudio::ui::StartupWizard::PracticeChoice choice);
    void startPracticeFromSongFile (const juce::File& songFile);
    void applyPracticeSetupResult (PracticeSetupResult result);
    void autoSavePracticeProject (const juce::String& projectTitle);
    [[nodiscard]] static juce::File getProjectsDirectory();

    juce::AudioDeviceManager& audioDeviceManager;
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

    /** Kept for internal callbacks only — never shown (workspace chrome is menus + View). */
    jamstudio::ui::ToolbarTabs toolbarTabs;
    jamstudio::ui::StartupWizard startupWizard;
    std::unique_ptr<PracticeSetupPipeline> practiceSetupPipeline;
    juce::Label statusLabel;
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
    jamstudio::ui::FullPageTabsWindow fullPageTabsWindow;

    juce::File currentSongFile;
    juce::File currentScoreFile;
    juce::File currentLyricsFile;
    juce::File currentProjectFile;
    std::unique_ptr<juce::FileChooser> fileChooser;

    bool lyricsPanelVisible = true;
    bool notationPanelVisible = true;
    /** Stem lanes off by default — use Mixer window; keeps tabs large. */
    bool stemsPanelVisible = false;
    bool workspaceReady = false;
    jamstudio::ui::StartupWizard::Mode currentMode = jamstudio::ui::StartupWizard::Mode::practice;

    std::atomic<uint32_t> backgroundTaskGeneration { 0 };
    bool backgroundTaskActive = false;
};

} // namespace jamstudio::app
