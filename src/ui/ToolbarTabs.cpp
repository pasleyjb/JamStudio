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
    for (auto* tab : { &projectTab, &viewTab, &stemsTab, &notationTab, &lyricsTab, &recordTab, &ampTab })
        addAndMakeVisible (*tab);

    projectTab.onClick = [this] { showTab (Tab::project); };
    viewTab.onClick = [this] { showTab (Tab::view); };
    stemsTab.onClick = [this] { showTab (Tab::stems); };
    notationTab.onClick = [this] { showTab (Tab::notation); };
    lyricsTab.onClick = [this] { showTab (Tab::lyrics); };
    recordTab.onClick = [this] { showTab (Tab::record); };
    ampTab.onClick = [this] { showTab (Tab::amp); };

    for (auto* panel : { &projectPanel, &viewPanel, &stemsPanel,
                         &notationPanel, &lyricsPanel, &recordPanel, &ampPanel })
        addChildComponent (*panel);

    openSongButton.onClick = [this] { if (toolbarActions.openSong) toolbarActions.openSong(); };
    saveProjectButton.onClick = [this] { if (toolbarActions.saveProject) toolbarActions.saveProject(); };
    loadProjectButton.onClick = [this] { if (toolbarActions.loadProject) toolbarActions.loadProject(); };
    recentProjectsButton.onClick = [this] { if (toolbarActions.showRecentProjects) toolbarActions.showRecentProjects(); };
    separateButton.onClick = [this] { if (toolbarActions.separateStems) toolbarActions.separateStems(); };
    browseLibraryButton.onClick = [this] { if (toolbarActions.browseTabLibrary) toolbarActions.browseTabLibrary(); };
    importScoreButton.onClick = [this] { if (toolbarActions.importScore) toolbarActions.importScore(); };
    tabViewButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    tabViewButton.onClick = [this] { if (toolbarActions.showTabView) toolbarActions.showTabView(); };
    sheetViewButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    sheetViewButton.onClick = [this] { if (toolbarActions.showSheetView) toolbarActions.showSheetView(); };
    fullPageTabsButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    fullPageTabsButton.onClick = [this] { if (toolbarActions.openFullPageTabs) toolbarActions.openFullPageTabs(); };
    aiTabButton.onClick = [this] { if (toolbarActions.aiTab) toolbarActions.aiTab(); };
    importLyricsButton.onClick = [this] { if (toolbarActions.importLyrics) toolbarActions.importLyrics(); };
    onlineLyricsButton.onClick = [this] { if (toolbarActions.onlineLyrics) toolbarActions.onlineLyrics(); };
    aiLyricsButton.onClick = [this] { if (toolbarActions.aiLyrics) toolbarActions.aiLyrics(); };
    recordButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    recordButton.onClick = [this] { if (toolbarActions.toggleRecording) toolbarActions.toggleRecording(); };

    loadAmpModelButton.onClick = [this] { if (toolbarActions.loadAmpModel) toolbarActions.loadAmpModel(); };
    ampEnabledButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    ampEnabledButton.onClick = [this] { if (toolbarActions.toggleAmpEnabled) toolbarActions.toggleAmpEnabled(); };
    ampBypassButton.setIndicatorColour (JamStudioTheme::getColours().indicatorMute);
    ampBypassButton.onClick = [this] { if (toolbarActions.toggleAmpBypass) toolbarActions.toggleAmpBypass(); };

    ampModelLabel.setText ("No model", juce::dontSendNotification);
    ampModelLabel.setJustificationType (juce::Justification::centredLeft);
    ampModelLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    ampModelLabel.setFont (juce::FontOptions (12.0f));

    auto setupGain = [] (juce::Slider& s, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 18);
        s.setRange (-24.0, 24.0, 0.1);
        s.setValue (0.0, juce::dontSendNotification);
        s.setTextValueSuffix (" dB");
        s.setName (name);
        s.setTooltip (name);
    };

    setupGain (ampInputGain, "Input");
    setupGain (ampOutputGain, "Output");
    ampInputGain.onValueChange = [this]
    {
        if (onAmpInputGainChanged)
            onAmpInputGainChanged (static_cast<float> (ampInputGain.getValue()));
    };
    ampOutputGain.onValueChange = [this]
    {
        if (onAmpOutputGainChanged)
            onAmpOutputGainChanged (static_cast<float> (ampOutputGain.getValue()));
    };

    for (auto* toggle : { &showLyricsPanelButton, &showNotationPanelButton,
                          &showStemsPanelButton, &showMixerPanelButton })
    {
        toggle->setClickingTogglesState (true);
        toggle->setColour (juce::TextButton::buttonOnColourId,
                           JamStudioTheme::getColours().accent.withAlpha (0.35f));
    }

    showLyricsPanelButton.onClick = [this] { if (toolbarActions.toggleLyricsPanel) toolbarActions.toggleLyricsPanel(); };
    showNotationPanelButton.onClick = [this] { if (toolbarActions.toggleNotationPanel) toolbarActions.toggleNotationPanel(); };
    showStemsPanelButton.onClick = [this] { if (toolbarActions.toggleStemsPanel) toolbarActions.toggleStemsPanel(); };
    showMixerPanelButton.onClick = [this] { if (toolbarActions.toggleMixerWindow) toolbarActions.toggleMixerWindow(); };

    projectPanel.addAndMakeVisible (openSongButton);
    projectPanel.addAndMakeVisible (saveProjectButton);
    projectPanel.addAndMakeVisible (loadProjectButton);
    projectPanel.addAndMakeVisible (recentProjectsButton);

    viewPanel.addAndMakeVisible (showLyricsPanelButton);
    viewPanel.addAndMakeVisible (showNotationPanelButton);
    viewPanel.addAndMakeVisible (showStemsPanelButton);
    viewPanel.addAndMakeVisible (showMixerPanelButton);

    stemsPanel.addAndMakeVisible (separateButton);

    notationPanel.addAndMakeVisible (browseLibraryButton);
    notationPanel.addAndMakeVisible (importScoreButton);
    notationPanel.addAndMakeVisible (tabViewButton);
    notationPanel.addAndMakeVisible (sheetViewButton);
    notationPanel.addAndMakeVisible (fullPageTabsButton);
    notationPanel.addAndMakeVisible (aiTabButton);

    lyricsPanel.addAndMakeVisible (onlineLyricsButton);
    lyricsPanel.addAndMakeVisible (importLyricsButton);
    lyricsPanel.addAndMakeVisible (aiLyricsButton);

    recordPanel.addAndMakeVisible (recordButton);

    ampPanel.addAndMakeVisible (loadAmpModelButton);
    ampPanel.addAndMakeVisible (ampEnabledButton);
    ampPanel.addAndMakeVisible (ampBypassButton);
    ampPanel.addAndMakeVisible (ampModelLabel);
    ampPanel.addAndMakeVisible (ampInputGain);
    ampPanel.addAndMakeVisible (ampOutputGain);

    showTab (Tab::project);
}

void ToolbarTabs::setAmpGainCallbacks (std::function<void (float)> inputDb,
                                       std::function<void (float)> outputDb)
{
    onAmpInputGainChanged = std::move (inputDb);
    onAmpOutputGainChanged = std::move (outputDb);
}

void ToolbarTabs::styleToggle (juce::TextButton& button, const bool on)
{
    button.setToggleState (on, juce::dontSendNotification);
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
    const auto tabWidth = tabRow.getWidth() / 7;

    projectTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    viewTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    stemsTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    notationTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    lyricsTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    recordTab.setBounds (tabRow.removeFromLeft (tabWidth).reduced (1, 0));
    ampTab.setBounds (tabRow.reduced (1, 0));

    styleTabButton (projectTab, activeTab == Tab::project);
    styleTabButton (viewTab, activeTab == Tab::view);
    styleTabButton (stemsTab, activeTab == Tab::stems);
    styleTabButton (notationTab, activeTab == Tab::notation);
    styleTabButton (lyricsTab, activeTab == Tab::lyrics);
    styleTabButton (recordTab, activeTab == Tab::record);
    styleTabButton (ampTab, activeTab == Tab::amp);

    auto panelArea = bounds.reduced (4, 2);
    projectPanel.setBounds (panelArea);
    viewPanel.setBounds (panelArea);
    stemsPanel.setBounds (panelArea);
    notationPanel.setBounds (panelArea);
    lyricsPanel.setBounds (panelArea);
    recordPanel.setBounds (panelArea);
    ampPanel.setBounds (panelArea);

    layoutPanel (projectPanel, { &openSongButton, &saveProjectButton, &loadProjectButton, &recentProjectsButton });
    layoutPanel (viewPanel, { &showLyricsPanelButton, &showNotationPanelButton, &showStemsPanelButton, &showMixerPanelButton });
    layoutPanel (stemsPanel, { &separateButton });
    layoutPanel (notationPanel, { &browseLibraryButton, &importScoreButton, &tabViewButton, &sheetViewButton,
                                  &fullPageTabsButton, &aiTabButton });
    layoutPanel (lyricsPanel, { &onlineLyricsButton, &importLyricsButton, &aiLyricsButton });
    layoutPanel (recordPanel, { &recordButton });

    // Amp panel: buttons + label + two sliders
    {
        auto area = ampPanel.getLocalBounds().reduced (2);
        const int btnW = 96;
        loadAmpModelButton.setBounds (area.removeFromLeft (btnW).reduced (2));
        ampEnabledButton.setBounds (area.removeFromLeft (btnW).reduced (2));
        ampBypassButton.setBounds (area.removeFromLeft (btnW).reduced (2));
        ampModelLabel.setBounds (area.removeFromLeft (juce::jmin (180, area.getWidth() / 3)).reduced (4, 0));
        ampInputGain.setBounds (area.removeFromLeft (area.getWidth() / 2).reduced (4, 2));
        ampOutputGain.setBounds (area.reduced (4, 2));
    }
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

    projectPanel.setVisible (tab == Tab::project);
    viewPanel.setVisible (tab == Tab::view);
    stemsPanel.setVisible (tab == Tab::stems);
    notationPanel.setVisible (tab == Tab::notation);
    lyricsPanel.setVisible (tab == Tab::lyrics);
    recordPanel.setVisible (tab == Tab::record);
    ampPanel.setVisible (tab == Tab::amp);

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
    onlineLyricsButton.setEnabled (enabled);
}

void ToolbarTabs::setNotationViewState (const jamstudio::notation::NotationMode mode)
{
    const auto isTab = mode == jamstudio::notation::NotationMode::tab;
    const auto isSheet = mode == jamstudio::notation::NotationMode::standard;

    tabViewButton.setIndicatorActive (isTab);
    sheetViewButton.setIndicatorActive (isSheet);
}

void ToolbarTabs::setPanelVisibilityState (const bool lyricsVisible, const bool notationVisible,
                                           const bool stemsVisible, const bool mixerVisible)
{
    styleToggle (showLyricsPanelButton, lyricsVisible);
    styleToggle (showNotationPanelButton, notationVisible);
    styleToggle (showStemsPanelButton, stemsVisible);
    styleToggle (showMixerPanelButton, mixerVisible);
}

void ToolbarTabs::setAmpState (const bool enabled, const bool bypassed, const juce::String& modelName)
{
    ampEnabledButton.setIndicatorActive (enabled);
    ampEnabledButton.setButtonText (enabled ? "Amp On" : "Amp Off");
    ampBypassButton.setIndicatorActive (bypassed);
    ampModelLabel.setText (modelName.isNotEmpty() ? modelName : juce::String ("No model"),
                           juce::dontSendNotification);
}

} // namespace jamstudio::ui
