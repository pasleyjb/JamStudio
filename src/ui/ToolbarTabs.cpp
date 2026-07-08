#include "ToolbarTabs.h"

namespace jamstudio::ui
{

namespace
{
void styleTabButton (juce::TextButton& button, const bool selected)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonOnColourId,
                      JamStudioTheme::getColours().accent.withAlpha (0.25f));
    button.setToggleState (selected, juce::dontSendNotification);
}
} // namespace

ToolbarTabs::ToolbarTabs (Actions actions)
    : toolbarActions (std::move (actions))
{
    for (auto* tab : { &transportTab, &projectTab, &stemsTab, &notationTab, &lyricsTab, &recordTab })
        addAndMakeVisible (*tab);

    transportTab.onClick = [this] { showTab (Tab::transport); };
    projectTab.onClick = [this] { showTab (Tab::project); };
    stemsTab.onClick = [this] { showTab (Tab::stems); };
    notationTab.onClick = [this] { showTab (Tab::notation); };
    lyricsTab.onClick = [this] { showTab (Tab::lyrics); };
    recordTab.onClick = [this] { showTab (Tab::record); };

    for (auto* panel : { &transportPanel, &projectPanel, &stemsPanel,
                         &notationPanel, &lyricsPanel, &recordPanel })
        addChildComponent (*panel);

    openSongButton.onClick = [this] { if (toolbarActions.openSong) toolbarActions.openSong(); };
    saveProjectButton.onClick = [this] { if (toolbarActions.saveProject) toolbarActions.saveProject(); };
    loadProjectButton.onClick = [this] { if (toolbarActions.loadProject) toolbarActions.loadProject(); };
    recentProjectsButton.onClick = [this] { if (toolbarActions.showRecentProjects) toolbarActions.showRecentProjects(); };
    separateButton.onClick = [this] { if (toolbarActions.separateStems) toolbarActions.separateStems(); };
    importScoreButton.onClick = [this] { if (toolbarActions.importScore) toolbarActions.importScore(); };
    aiTabButton.onClick = [this] { if (toolbarActions.aiTab) toolbarActions.aiTab(); };
    importLyricsButton.onClick = [this] { if (toolbarActions.importLyrics) toolbarActions.importLyrics(); };
    aiLyricsButton.onClick = [this] { if (toolbarActions.aiLyrics) toolbarActions.aiLyrics(); };
    recordButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    recordButton.onClick = [this] { if (toolbarActions.toggleRecording) toolbarActions.toggleRecording(); };

    projectPanel.addAndMakeVisible (openSongButton);
    projectPanel.addAndMakeVisible (saveProjectButton);
    projectPanel.addAndMakeVisible (loadProjectButton);
    projectPanel.addAndMakeVisible (recentProjectsButton);

    stemsPanel.addAndMakeVisible (separateButton);

    notationPanel.addAndMakeVisible (importScoreButton);
    notationPanel.addAndMakeVisible (aiTabButton);

    lyricsPanel.addAndMakeVisible (importLyricsButton);
    lyricsPanel.addAndMakeVisible (aiLyricsButton);

    recordPanel.addAndMakeVisible (recordButton);

    transportPanel.addAndMakeVisible (transportHint);
    transportHint.setText ("Play, pause, stop, and metronome are in the transport bar below the waveform.",
                           juce::dontSendNotification);
    transportHint.setJustificationType (juce::Justification::centredLeft);

    showTab (Tab::transport);
}

void ToolbarTabs::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.toolbarBackground);
    g.setColour (colours.border);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void ToolbarTabs::resized()
{
    auto bounds = getLocalBounds();

    auto tabRow = bounds.removeFromTop (28);
    const auto tabWidth = tabRow.getWidth() / 6;

    transportTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    projectTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    stemsTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    notationTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    lyricsTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    recordTab.setBounds (tabRow.reduced (1, 0));

    styleTabButton (transportTab, activeTab == Tab::transport);
    styleTabButton (projectTab, activeTab == Tab::project);
    styleTabButton (stemsTab, activeTab == Tab::stems);
    styleTabButton (notationTab, activeTab == Tab::notation);
    styleTabButton (lyricsTab, activeTab == Tab::lyrics);
    styleTabButton (recordTab, activeTab == Tab::record);

    auto panelArea = bounds.reduced (4, 2);
    transportPanel.setBounds (panelArea);
    projectPanel.setBounds (panelArea);
    stemsPanel.setBounds (panelArea);
    notationPanel.setBounds (panelArea);
    lyricsPanel.setBounds (panelArea);
    recordPanel.setBounds (panelArea);

    transportHint.setBounds (transportPanel.getLocalBounds().reduced (6, 2));
    layoutPanel (projectPanel, { &openSongButton, &saveProjectButton, &loadProjectButton, &recentProjectsButton });
    layoutPanel (stemsPanel, { &separateButton });
    layoutPanel (notationPanel, { &importScoreButton, &aiTabButton });
    layoutPanel (lyricsPanel, { &importLyricsButton, &aiLyricsButton });
    layoutPanel (recordPanel, { &recordButton });
}

void ToolbarTabs::layoutPanel (juce::Component& panel, const std::vector<juce::Component*>& buttons)
{
    auto area = panel.getLocalBounds().reduced (2);
    const auto buttonWidth = juce::jmax (90, area.getWidth() / juce::jmax (1, static_cast<int> (buttons.size())) - 4);

    for (auto* button : buttons)
        button->setBounds (area.removeFromLeft (buttonWidth).reduced (2));
}

void ToolbarTabs::showTab (const Tab tab)
{
    activeTab = tab;

    transportPanel.setVisible (tab == Tab::transport);
    projectPanel.setVisible (tab == Tab::project);
    stemsPanel.setVisible (tab == Tab::stems);
    notationPanel.setVisible (tab == Tab::notation);
    lyricsPanel.setVisible (tab == Tab::lyrics);
    recordPanel.setVisible (tab == Tab::record);

    resized();
}

void ToolbarTabs::setActiveTab (const Tab tab)
{
    showTab (tab);
}

void ToolbarTabs::setRecordingActive (const bool recording)
{
    recordButton.setButtonText (recording ? "Stop" : "Record");
    recordButton.setIndicatorActive (recording, recording);
}

void ToolbarTabs::setToolsEnabled (const bool enabled)
{
    separateButton.setEnabled (enabled);
    aiTabButton.setEnabled (enabled);
    aiLyricsButton.setEnabled (enabled);
}

} // namespace jamstudio::ui