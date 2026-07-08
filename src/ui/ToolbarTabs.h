#pragma once

#include "IndicatorButton.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Audacity-style tabbed toolbar hosting contextual tool buttons. */
class ToolbarTabs : public juce::Component
{
public:
    enum class Tab
    {
        transport = 0,
        project,
        stems,
        notation,
        lyrics,
        record
    };

    struct Actions
    {
        std::function<void()> openSong;
        std::function<void()> saveProject;
        std::function<void()> loadProject;
        std::function<void()> showRecentProjects;
        std::function<void()> separateStems;
        std::function<void()> importScore;
        std::function<void()> showTabView;
        std::function<void()> showSheetView;
        std::function<void()> aiTab;
        std::function<void()> importLyrics;
        std::function<void()> aiLyrics;
        std::function<void()> toggleRecording;
    };

    explicit ToolbarTabs (Actions actions);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void setActiveTab (Tab tab);
    void setRecordingActive (bool recording);
    void setToolsEnabled (bool enabled);

private:
    void showTab (Tab tab);
    void layoutPanel (juce::Component& panel, const std::vector<juce::Component*>& buttons);

    Actions toolbarActions;
    Tab activeTab = Tab::transport;

    juce::TextButton transportTab { "Transport" };
    juce::TextButton projectTab { "Project" };
    juce::TextButton stemsTab { "Stems" };
    juce::TextButton notationTab { "Notation" };
    juce::TextButton lyricsTab { "Lyrics" };
    juce::TextButton recordTab { "Record" };

    juce::Component transportPanel;
    juce::Label transportHint;
    juce::Component projectPanel;
    juce::Component stemsPanel;
    juce::Component notationPanel;
    juce::Component lyricsPanel;
    juce::Component recordPanel;

    IndicatorButton openSongButton { "open", "Open Song" };
    IndicatorButton saveProjectButton { "save", "Save" };
    IndicatorButton loadProjectButton { "load", "Load" };
    IndicatorButton recentProjectsButton { "recent", "Recent" };
    IndicatorButton separateButton { "separate", "Separate" };
    IndicatorButton importScoreButton { "importScore", "Import Score" };
    IndicatorButton tabViewButton { "tabView", "Tab" };
    IndicatorButton sheetViewButton { "sheetView", "Sheet" };
    IndicatorButton aiTabButton { "aiTab", "AI Tab" };
    IndicatorButton importLyricsButton { "importLyrics", "Import LRC" };
    IndicatorButton aiLyricsButton { "aiLyrics", "AI Lyrics" };
    IndicatorButton recordButton { "record", "Record" };
};

} // namespace jamstudio::ui