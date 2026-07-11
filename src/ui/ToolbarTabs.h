#pragma once

#include "../notation/Score.h"
#include "IndicatorButton.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Tabbed tool ribbon. Panel show/hide lives under the View tab (not on the workspace). */
class ToolbarTabs : public juce::Component
{
public:
    enum class Tab
    {
        project = 0,
        view,
        stems,
        notation,
        lyrics,
        record,
        amp
    };

    struct Actions
    {
        std::function<void()> openSong;
        std::function<void()> saveProject;
        std::function<void()> loadProject;
        std::function<void()> showRecentProjects;
        std::function<void()> separateStems;
        std::function<void()> browseTabLibrary;
        std::function<void()> importScore;
        std::function<void()> showTabView;
        std::function<void()> showSheetView;
        std::function<void()> openFullPageTabs;
        std::function<void()> aiTab;
        std::function<void()> importLyrics;
        std::function<void()> onlineLyrics;
        std::function<void()> aiLyrics;
        std::function<void()> toggleRecording;
        std::function<void()> toggleLyricsPanel;
        std::function<void()> toggleNotationPanel;
        std::function<void()> toggleStemsPanel;
        std::function<void()> toggleMixerWindow;
        std::function<void()> loadAmpModel;
        std::function<void()> toggleAmpEnabled;
        std::function<void()> toggleAmpBypass;
    };

    explicit ToolbarTabs (Actions actions);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void setActiveTab (Tab tab);
    void setRecordingActive (bool recording);
    void setToolsEnabled (bool enabled);
    void setNotationViewState (jamstudio::notation::NotationMode mode);
    void setPanelVisibilityState (bool lyricsVisible, bool notationVisible,
                                  bool stemsVisible, bool mixerVisible);
    void setAmpState (bool enabled, bool bypassed, const juce::String& modelName);

private:
    void showTab (Tab tab);
    void layoutPanel (juce::Component& panel, const std::vector<juce::Component*>& buttons);
    void styleToggle (juce::TextButton& button, bool on);

    Actions toolbarActions;
    Tab activeTab = Tab::project;

    juce::TextButton projectTab { "Project" };
    juce::TextButton viewTab { "View" };
    juce::TextButton stemsTab { "Stems" };
    juce::TextButton notationTab { "Notation" };
    juce::TextButton lyricsTab { "Lyrics" };
    juce::TextButton recordTab { "Record" };
    juce::TextButton ampTab { "Amp" };

    juce::Component projectPanel;
    juce::Component viewPanel;
    juce::Component stemsPanel;
    juce::Component notationPanel;
    juce::Component lyricsPanel;
    juce::Component recordPanel;
    juce::Component ampPanel;

    IndicatorButton openSongButton { "open", "Open Song" };
    IndicatorButton saveProjectButton { "save", "Save" };
    IndicatorButton loadProjectButton { "load", "Load" };
    IndicatorButton recentProjectsButton { "recent", "Recent" };
    IndicatorButton separateButton { "separate", "Separate" };
    IndicatorButton browseLibraryButton { "browseLibrary", "Browse Library" };
    IndicatorButton importScoreButton { "importScore", "Import File" };
    IndicatorButton tabViewButton { "tabView", "Tab" };
    IndicatorButton sheetViewButton { "sheetView", "Sheet" };
    IndicatorButton fullPageTabsButton { "fullPageTabs", "Full Page" };
    IndicatorButton aiTabButton { "aiTab", "AI Tab" };
    IndicatorButton importLyricsButton { "importLyrics", "Import LRC" };
    IndicatorButton onlineLyricsButton { "onlineLyrics", "Online Lyrics" };
    IndicatorButton aiLyricsButton { "aiLyrics", "AI Lyrics" };
    IndicatorButton recordButton { "record", "Record" };
    IndicatorButton loadAmpModelButton { "loadAmp", "Load .nam" };
    IndicatorButton ampEnabledButton { "ampOn", "Amp On" };
    IndicatorButton ampBypassButton { "ampBypass", "Bypass" };

    juce::Label ampModelLabel;
    juce::Slider ampInputGain;
    juce::Slider ampOutputGain;
    std::function<void (float)> onAmpInputGainChanged;
    std::function<void (float)> onAmpOutputGainChanged;

public:
    void setAmpGainCallbacks (std::function<void (float)> inputDb,
                              std::function<void (float)> outputDb);

private:
    juce::TextButton showLyricsPanelButton { "Lyrics Panel" };
    juce::TextButton showNotationPanelButton { "Tabs Panel" };
    juce::TextButton showStemsPanelButton { "Stems Panel" };
    juce::TextButton showMixerPanelButton { "Mixer Window" };
};

} // namespace jamstudio::ui
