#pragma once

#include "../ai/BasicPitchTranscriber.h"
#include "../ai/DemucsSeparator.h"
#include "../ai/WhisperTranscriber.h"
#include "../audio/AudioRecorder.h"
#include "../audio/RecordingExporter.h"
#include "../audio/TransportController.h"
#include "../notation/LyricsView.h"
#include "../notation/LyricsTrack.h"
#include "../notation/NotationView.h"
#include "../notation/Score.h"
#include "../ui/JamStudioTheme.h"
#include "../ui/SeparationProgressBar.h"
#include "../ui/StemStrip.h"
#include "../ui/ToolbarTabs.h"
#include "../ui/TransportBar.h"
#include "../project/RecentProjects.h"
#include "../ui/WaveformDisplay.h"

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
        importScoreCmd,
        aiTabCmd,
        importLyricsCmd,
        aiLyricsCmd,
        recordCmd,
        aboutCmd
    };

    void openSong();
    void saveProject();
    void loadProject();
    void loadProjectFile (const juce::File& file);
    void showRecentProjectsMenu();
    void importScore();
    void importLyrics();
    void separateStems();
    void transcribeLyrics();
    void transcribeTab();
    void toggleRecording();
    [[nodiscard]] juce::File findStemFileForType (jamstudio::audio::StemType preferredType);
    [[nodiscard]] juce::File findMelodicStemFile();
    void loadRecordingAsStem (const juce::File& recordingFile);
    void loadStemsIntoMixer (const juce::Array<juce::File>& stemFiles);
    void rebuildStemStrips();
    void setStatus (const juce::String& message);
    void refreshTheme();
    [[nodiscard]] juce::File getDefaultRecordingFile() const;

    juce::AudioDeviceManager& audioDeviceManager;
    jamstudio::audio::TransportController transportController;
    jamstudio::audio::AudioRecorder audioRecorder;
    jamstudio::audio::RecordingExporter recordingExporter;
    jamstudio::ai::DemucsSeparator demucsSeparator;
    jamstudio::ai::WhisperTranscriber whisperTranscriber;
    jamstudio::ai::BasicPitchTranscriber basicPitchTranscriber;
    jamstudio::project::RecentProjects recentProjects;
    jamstudio::notation::Score currentScore;
    jamstudio::notation::LyricsTrack currentLyrics;

    juce::AudioThumbnailCache thumbnailCache { 4 };

    jamstudio::ui::ToolbarTabs toolbarTabs;
    juce::Label statusLabel;
    jamstudio::ui::SeparationProgressBar separationProgress;
    jamstudio::ui::WaveformDisplay waveformDisplay;
    juce::Viewport notationViewport;
    jamstudio::notation::LyricsView lyricsView;
    jamstudio::notation::NotationView notationView;
    jamstudio::ui::TransportBar transportBar;
    juce::Viewport stemViewport;
    juce::Component stemContainer;

    juce::File currentSongFile;
    juce::File currentScoreFile;
    juce::File currentLyricsFile;
    juce::File currentProjectFile;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace jamstudio::app