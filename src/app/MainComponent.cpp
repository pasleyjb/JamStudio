#include "MainComponent.h"

#include "../audio/ArdourCompanion.h"
#include "../audio/ExternalRecorder.h"
#include "../audio/StemType.h"
#include "../audio/TempoDetector.h"
#include "../ui/AiToolsSetupDialog.h"
#include "../ui/AudioSettingsDialog.h"
#include "../ui/MidiControlDialog.h"
#include "../ui/OnlineLyricsDialog.h"
#include "../ui/TabLibraryBrowserDialog.h"
#include "../ui/JamStudioLookAndFeel.h"
#include "../notation/LrcParser.h"
#include "../notation/MusicXmlParser.h"
#include "../notation/ScoreLyricsExtractor.h"
#include "../notation/SongMetadata.h"
#include "../project/ProjectManager.h"

namespace jamstudio::app
{

MainComponent::MainComponent (juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager (deviceManager),
      audioInterfaceManager (deviceManager),
      transportController (deviceManager),
      midiControlSurface (deviceManager, transportController),
      recordingExporter (transportController.getFormatManager()),
      recentProjects (juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                          .getChildFile ("JamStudio")
                          .getChildFile ("recent-projects.json")),
      toolbarTabs ({
          [this] { openSong(); },
          [this] { saveProject(); },
          [this] { loadProject(); },
          [this] { showRecentProjectsMenu(); },
          [this] { separateStems(); },
          [this] { browseTabLibrary(); },
          [this] { importScore(); },
          [this] { toggleTabView(); },
          [this] { toggleSheetView(); },
          [this] { openFullPageTabs(); },
          [this] { transcribeTab(); },
          [this] { importLyrics(); },
          [this] { findOnlineLyrics(); },
          [this] { transcribeLyrics(); },
          [this] { toggleRecording(); },
          [this] { toggleLyricsPanel(); },
          [this] { toggleNotationPanel(); },
          [this] { toggleStemsPanel(); },
          [this] { toggleMixerWindow(); }
      }),
      waveformDisplay (transportController.getFormatManager(), thumbnailCache, transportController),
      lyricsView (transportController),
      notationView (transportController),
      transportBar (transportController),
      mixerWindow (transportController),
      stageFxController (transportController),
      fullPageTabsWindow (transportController),
      fullPageLyricsWindow (transportController),
      karaokeOutput (transportController),
      stageFxOutput (transportController)
{
    setSize (1280, 900);
    refreshTheme();

    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    statusLabel.setColour (juce::Label::textColourId,
                           jamstudio::ui::JamStudioTheme::getColours().text);
    statusLabel.setFont (juce::FontOptions (13.0f));
    setStatus ("Choose Practice, Performance, or Recording to begin.");

    statusCancelButton.setVisible (false);
    statusCancelButton.setColour (juce::TextButton::buttonColourId,
                                  jamstudio::ui::JamStudioTheme::getColours().buttonFace);

    // Top workspace toolbar permanently removed - use menus and View toggles.
    toolbarTabs.setVisible (false);

    // Progress strip never shown - status lives in the bottom border bar.
    separationProgress.setVisible (false);

    addAndMakeVisible (statusLabel);
    addAndMakeVisible (statusCancelButton);
    addAndMakeVisible (transportBar);

    for (auto* label : { &lyricsSectionLabel, &notationSectionLabel, &stemsSectionLabel })
    {
        label->setFont (juce::FontOptions (11.0f, juce::Font::bold));
        label->setJustificationType (juce::Justification::centredLeft);
        label->setColour (juce::Label::textColourId,
                          jamstudio::ui::JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (*label);
    }

    addAndMakeVisible (lyricsView);

    notationHeaderBar.setModeChangedCallback ([this] (const jamstudio::notation::NotationMode mode)
    {
        setNotationDisplayMode (mode);
    });
    notationHeaderBar.setPartChangedCallback ([this] (const int partIndex)
    {
        setActiveScorePart (partIndex);
    });
    notationHeaderBar.setFullPageCallback ([this] { openFullPageTabs(); });
    addAndMakeVisible (notationHeaderBar);

    notationViewport.setViewedComponent (&notationView, false);
    notationViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (notationViewport);

    addAndMakeVisible (waveformDisplay);

    stemViewport.setViewedComponent (&stemContainer, false);
    stemViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (stemViewport);

    audioDeviceManager.addAudioCallback (&audioRecorder);

    // Plug-and-play: detect multi-IO interfaces, match channel counts, hot-plug USB.
    audioInterfaceManager.initialiseAtStartup();
    audioInterfaceManager.addChangeListener (this);
    refreshAudioRoutingStatus();

    transportController.getStemMixer().addChangeListener (this);
    transportController.addChangeListener (this);

    transportBar.setDetectTempoCallback ([this]
    {
        if (currentSongFile.existsAsFile())
            detectTempoFromSong (currentSongFile, true);
        else if (transportController.getStemMixer().getNumStems() > 0)
            detectTempoFromSong (transportController.getStemMixer().getStem (0)->getFile(), true);
        else
            setStatus ("Open a song before detecting tempo.");
    });
    // REC / Open Studio → Ardour companion on Linux; otherwise external recorder.
    transportBar.setRecordCallback ([this]
    {
       #if JUCE_LINUX
        openArdourStudio();
       #else
        openExternalRecorder();
       #endif
    });
    transportBar.setInputLevelProvider ([this] { return audioRecorder.getInputLevel(); });

    recordingTakesPanel.setTakeManager (&recordingTakeManager);
    recordingTakesPanel.setLoadTakeCallback ([this] (const jamstudio::audio::RecordingTakeManager::Take& take)
    {
        loadRecordingAsStem (take.file, take.displayName);
        setStatus ("Loaded " + take.displayName + " into mixer.");
    });
    recordingTakesPanel.setOpenExternalCallback ([this]
    {
       #if JUCE_LINUX
        openArdourStudio();
       #else
        openExternalRecorder();
       #endif
    });
    recordingTakesPanel.setImportTakeCallback ([this] { importTakeFromFile(); });
    {
       #if JUCE_LINUX
        recordingTakesPanel.setPreferredRecorderName (
            jamstudio::audio::ArdourCompanion::isAvailable() ? "Studio (Ardour)" : "Studio (install Ardour)");
       #else
        const auto preferred = jamstudio::audio::ExternalRecorder::getPreferred();
        recordingTakesPanel.setPreferredRecorderName (preferred.name);
       #endif
    }
    recordingTakesPanel.setVisible (false);
    addChildComponent (recordingTakesPanel);

    // Panels available immediately with empty states.
    rebuildMixerWindow();
    mixerWindow.setVisibilityChangedCallback ([this] (bool)
    {
        updatePanelToggleStates();
    });
    updatePanelToggleStates();
    applyPanelVisibility();

    midiControlSurface.setStatusCallback ([this] (const juce::String& msg) { setStatus (msg); });
    midiControlSurface.setUiRefreshCallback ([this] { refreshMixerUiFromMidi(); });
    midiControlSurface.setRecordToggleCallback ([this] { toggleRecording(); });
    midiControlSurface.setNextSongCallback ([this] { performanceTriggerNext(); });
    midiControlSurface.setEnabled (midiControlSurface.getSettings().enabled);

    performanceBar.setVisible (false);
    performanceBar.setTriggerCallback ([this] { performanceTriggerNext(); });
    addChildComponent (performanceBar);

    practiceSetupPipeline = std::make_unique<PracticeSetupPipeline> (
        demucsSeparator, whisperTranscriber, basicPitchTranscriber,
        transportController.getFormatManager());

    // Secondary windows must not appear until the user opens them.
    fullPageTabsWindow.showWindow (false);
    fullPageLyricsWindow.showWindow (false);
    karaokeOutput.hideOutput();
    stageFxOutput.hideOutput();
    mixerWindow.showMixer (false);
    stageFxController.showController (false);

    floatingDock.setWindows (&mixerWindow, &stageFxController);
    mixerWindow.setDockCallbacks ([this] { floatingDock.attach(); setStatus ("Windows stuck together."); },
                                  [this] { floatingDock.detach(); setStatus ("Windows unstuck."); },
                                  [this] { floatingDock.onMixerMaximised(); });
    stageFxController.setDockCallbacks ([this] { floatingDock.attach(); setStatus ("Windows stuck together."); },
                                        [this] { floatingDock.detach(); setStatus ("Windows unstuck."); });
    mixerWindow.setSaveSetlistMixCallback ([this] { saveCurrentMixerToSetlistTrack(); });
    updateMixerPerformanceContext();

    stageFxController.setVideoRouting ({
        [this] { return karaokeDisplayIndex; },
        [this] { return stageFxDisplayIndex; },
        [this] (const int i)
        {
            karaokeDisplayIndex = juce::jmax (0, i);
        },
        [this] (const int i)
        {
            stageFxDisplayIndex = juce::jmax (0, i);
        },
        [this] { return karaokeOutput.isOutputVisible(); },
        [this] { return stageFxOutput.isOutputVisible(); },
        [this] { openKaraokeOutput(); },
        [this]
        {
            karaokeOutput.hideOutput();
            stageFxController.syncVideoRoutingUi();
            setStatus ("Karaoke output closed.");
        },
        [this] { openStageFxOutput(); },
        [this]
        {
            stageFxOutput.hideOutput();
            stageFxController.syncVideoRoutingUi();
            setStatus ("Stage FX output closed.");
        }
    });

    setupStartupWizard();
}

MainComponent::~MainComponent()
{
    floatingDock.shutdown();
    fileChooser.reset();
    if (practiceSetupPipeline != nullptr)
        practiceSetupPipeline->cancel();
    mixerWindow.showMixer (false);
    stageFxController.showController (false);
    fullPageTabsWindow.showWindow (false);
    fullPageLyricsWindow.showWindow (false);
    karaokeOutput.hideOutput();
    stageFxOutput.hideOutput();
    audioRecorder.stopRecording();
    audioDeviceManager.removeAudioCallback (&audioRecorder);
    demucsSeparator.cancel();
    whisperTranscriber.cancel();
    basicPitchTranscriber.cancel();
    audioInterfaceManager.removeChangeListener (this);
    transportController.getStemMixer().removeChangeListener (this);
    transportController.removeChangeListener (this);
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
}

void MainComponent::updatePanelToggleStates()
{
    toolbarTabs.setPanelVisibilityState (lyricsPanelVisible,
                                         notationPanelVisible,
                                         stemsPanelVisible,
                                         mixerWindow.isMixerVisible());
}

void MainComponent::applyPanelVisibility()
{
    lyricsSectionLabel.setVisible (lyricsPanelVisible);
    lyricsView.setVisible (lyricsPanelVisible);

    notationSectionLabel.setVisible (notationPanelVisible);
    notationHeaderBar.setVisible (notationPanelVisible && ! currentScore.isEmpty());
    notationViewport.setVisible (notationPanelVisible);

    stemsSectionLabel.setVisible (stemsPanelVisible);
    stemViewport.setVisible (stemsPanelVisible);

    updatePanelToggleStates();
    resized();
}

void MainComponent::revealWorkspacePanels()
{
    // Lyrics + large tabs + stem waveform lanes (mixer is extra detail).
    lyricsPanelVisible = true;
    notationPanelVisible = true;
    stemsPanelVisible = true;
    applyPanelVisibility();
    mixerWindow.showMixer (true);
    updatePanelToggleStates();
}

void MainComponent::toggleLyricsPanel()
{
    lyricsPanelVisible = ! lyricsPanelVisible;
    applyPanelVisibility();
}

void MainComponent::toggleNotationPanel()
{
    notationPanelVisible = ! notationPanelVisible;
    applyPanelVisibility();
}

void MainComponent::toggleStemsPanel()
{
    stemsPanelVisible = ! stemsPanelVisible;
    applyPanelVisibility();
}

void MainComponent::toggleMixerWindow()
{
    mixerWindow.showMixer (! mixerWindow.isMixerVisible());
    updatePanelToggleStates();
    floatingDock.onWindowVisibilityChanged();
}

void MainComponent::toggleStageFxController()
{
    const bool show = ! stageFxController.isControllerVisible();
    if (show)
        syncVideoOutputs();
    stageFxController.showController (show);
    floatingDock.onWindowVisibilityChanged();
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto colours = jamstudio::ui::JamStudioTheme::getColours();
    g.fillAll (colours.windowBackground);

    // Bottom status border bar (messages live here - not a top progress strip).
    auto statusArea = getLocalBounds().removeFromBottom (30);
    g.setColour (colours.statusBackground);
    g.fillRect (statusArea);
    g.setColour (colours.border);
    g.drawHorizontalLine (statusArea.getY(), 0.0f, static_cast<float> (getWidth()));
    g.drawRect (statusArea, 1);
}

void MainComponent::resized()
{
    if (startupWizard.isVisible())
    {
        // Free-floating wizard covers the main area; status bar still peeks at the bottom.
        auto all = getLocalBounds();
        auto statusBar = all.removeFromBottom (30);
        layoutStatusBar (statusBar);
        startupWizard.setBounds (all);
        return;
    }

    auto bounds = getLocalBounds();

    // Status border bar pinned to the very bottom of the workspace.
    auto statusBar = bounds.removeFromBottom (30);
    layoutStatusBar (statusBar);

    bounds = bounds.reduced (8);

    // Performance stage bar (next-song / foot pedal) above the transport stack.
    if (performanceBar.isVisible())
    {
        performanceBar.setBounds (bounds.removeFromBottom (64));
        bounds.removeFromBottom (6);
    }

    // Recording takes panel (right column in recording mode).
    const bool showTakes = recordingTakesPanel.isVisible();
    juce::Rectangle<int> takesArea;
    if (showTakes)
    {
        takesArea = bounds.removeFromRight (220);
        bounds.removeFromRight (8);
        recordingTakesPanel.setBounds (takesArea);
    }

    // Bottom dock: overview waveform -> transport -> stem lanes with mini-waves
    const int stemLaneCount = stemContainer.getNumChildComponents();
    const int stemLaneHeight = 52;
    const int stemsBlockHeight = stemsPanelVisible
        ? (18 + juce::jlimit (64, 260, juce::jmax (1, stemLaneCount) * stemLaneHeight + 8))
        : 0;
    const int waveHeight = 72;
    const int transportHeight = 48;
    const int bottomStackHeight = waveHeight + 4 + transportHeight
                                  + (stemsPanelVisible ? 8 + stemsBlockHeight : 0);

    auto bottom = bounds.removeFromBottom (bottomStackHeight);

    waveformDisplay.setBounds (bottom.removeFromTop (waveHeight));
    bottom.removeFromTop (4);
    transportBar.setBounds (bottom.removeFromTop (transportHeight));

    if (stemsPanelVisible)
    {
        bottom.removeFromTop (8);
        auto stemsArea = bottom;
        stemsSectionLabel.setBounds (stemsArea.removeFromTop (16));
        stemViewport.setBounds (stemsArea);
        layoutStemLanes();
    }

    // Count visible upper panels so we can favour tabs when both are open.
    const int upperPanels = (lyricsPanelVisible ? 1 : 0) + (notationPanelVisible ? 1 : 0);

    // Compact karaoke strip (2-3 lines) - not a huge scrolling list.
    if (lyricsPanelVisible)
    {
        const int lyricsHeight = upperPanels == 1
            ? juce::jlimit (120, 220, bounds.getHeight() / 3)
            : juce::jlimit (100, 140, 120);
        lyricsSectionLabel.setBounds (bounds.removeFromTop (16));
        lyricsView.setBounds (bounds.removeFromTop (lyricsHeight));
        bounds.removeFromTop (6);
    }

    // Tabs take all remaining space (primary practice surface).
    if (notationPanelVisible)
    {
        notationSectionLabel.setBounds (bounds.removeFromTop (16));

        if (! currentScore.isEmpty())
        {
            notationHeaderBar.setVisible (true);
            notationHeaderBar.setBounds (bounds.removeFromTop (28));
            bounds.removeFromTop (2);
        }
        else
        {
            notationHeaderBar.setVisible (false);
        }

        notationViewport.setVisible (true);
        notationViewport.setBounds (bounds);
        // Scale frets/measures to the actual panel size (not a fixed 160px strip).
        notationView.setStripViewportHeight (notationViewport.getHeight());
        notationView.setSize (juce::jmax (notationViewport.getWidth(), notationView.getContentWidth()),
                              juce::jmax (notationViewport.getHeight(), notationView.getContentHeight()));
    }
}

void MainComponent::layoutStatusBar (juce::Rectangle<int> statusBar)
{
    auto area = statusBar.reduced (8, 3);

    if (statusCancelButton.isVisible())
    {
        statusCancelButton.setBounds (area.removeFromRight (88).reduced (0, 1));
        area.removeFromRight (8);
    }

    statusLabel.setBounds (area);
}

void MainComponent::layoutStemLanes()
{
    constexpr int laneHeight = 52;
    const auto containerWidth = juce::jmax (stemViewport.getMaximumVisibleWidth(), stemViewport.getWidth());
    stemContainer.setSize (containerWidth, juce::jmax (1, stemContainer.getNumChildComponents()) * laneHeight + 4);

    auto stripBounds = stemContainer.getLocalBounds().reduced (2);
    int y = stripBounds.getY();

    for (int i = 0; i < stemContainer.getNumChildComponents(); ++i)
    {
        if (auto* lane = stemContainer.getChildComponent (i))
        {
            lane->setBounds (stripBounds.getX(), y, stripBounds.getWidth(), laneHeight - 2);
            y += laneHeight;
        }
    }
}

juce::StringArray MainComponent::buildMenuBarNames()
{
    return { "File", "Project", "View", "Stems", "Notation", "Lyrics", "Transport", "Performance", "Help" };
}

juce::PopupMenu MainComponent::buildMenuForIndex (const int topLevelMenuIndex, const juce::String& menuName)
{
    juce::PopupMenu menu;

    if (menuName == "File")
    {
        menu.addItem (openSongCmd, "Open Song...", true, false);
        menu.addSeparator();
        menu.addItem (quitCmd, "Quit", true, false);
    }
    else if (menuName == "Project")
    {
        menu.addItem (saveProjectCmd, "Save Project", true, false);
        menu.addItem (loadProjectCmd, "Load Project", true, false);
        menu.addItem (recentProjectsCmd, "Recent Projects", true, false);
    }
    else if (menuName == "View")
    {
        menu.addItem (toggleLyricsPanelCmd, "Show Lyrics", true, lyricsPanelVisible);
        menu.addItem (toggleNotationPanelCmd, "Show Tabs", true, notationPanelVisible);
        menu.addItem (toggleStemsPanelCmd, "Show Stem Lanes", true, stemsPanelVisible);
        menu.addItem (toggleMixerWindowCmd, "Show Mixer", true, mixerWindow.isMixerVisible());
        menu.addItem (toggleStageFxControllerCmd, "Show Stage FX Controller", true,
                      stageFxController.isControllerVisible());
        menu.addSeparator();

        const auto& dock = floatingDock.getSettings();
        juce::PopupMenu dockMenu;
        dockMenu.addItem (dockAttachCmd, "Stick Windows Together  <>", true, dock.sticky);
        dockMenu.addItem (dockDetachCmd, "Unstick Windows  ><", true, ! dock.sticky);
        dockMenu.addSeparator();
        dockMenu.addItem (dockSideRightCmd, "Dock Stage FX on Right", true,
                          dock.dockSide == jamstudio::ui::FloatingDockSettings::Side::right);
        dockMenu.addItem (dockSideLeftCmd, "Dock Stage FX on Left", true,
                          dock.dockSide == jamstudio::ui::FloatingDockSettings::Side::left);
        dockMenu.addItem (dockSideTopCmd, "Dock Stage FX on Top", true,
                          dock.dockSide == jamstudio::ui::FloatingDockSettings::Side::top);
        dockMenu.addItem (dockSideBottomCmd, "Dock Stage FX on Bottom", true,
                          dock.dockSide == jamstudio::ui::FloatingDockSettings::Side::bottom);
        dockMenu.addSeparator();
        dockMenu.addItem (dockGapTightCmd, "Gap: Tight (0 px)", true, dock.gapPx == 0);
        dockMenu.addItem (dockGapNormalCmd, "Gap: Normal (4 px)", true, dock.gapPx == 4);
        dockMenu.addItem (dockGapWideCmd, "Gap: Wide (12 px)", true, dock.gapPx == 12);
        dockMenu.addSeparator();
        dockMenu.addItem (dockAutoStickCmd, "Auto-Stick When Edges Touch", true, dock.autoStickOnTouch);
        dockMenu.addItem (dockDetachOnMaxCmd, "Unstick When Mixer Maximised", true, dock.detachOnMaximise);
        menu.addSubMenu ("Floating Window Dock", dockMenu);
    }
    else if (menuName == "Stems")
    {
        menu.addItem (separateStemsCmd, "Separate Stems", demucsSeparator.isAvailable(), false);
        menu.addItem (toggleMixerWindowCmd, "Show Mixer", true, mixerWindow.isMixerVisible());
    }
    else if (menuName == "Notation")
    {
        menu.addItem (browseTabLibraryCmd, "Browse Tab Library...", true, false);
        menu.addItem (importScoreCmd, "Import Local MusicXML...", true, false);
        menu.addItem (aiTabCmd, "AI Tab Transcription", basicPitchTranscriber.isAvailable(), false);
        menu.addSeparator();
        menu.addItem (showTabViewCmd, "Tab View", true,
                      currentScore.isNotationVisible()
                          && currentScore.getNotationMode() == jamstudio::notation::NotationMode::tab);
        menu.addItem (showSheetViewCmd, "Sheet View", true,
                      currentScore.isNotationVisible()
                          && currentScore.getNotationMode() == jamstudio::notation::NotationMode::standard);
        menu.addItem (openFullPageTabsCmd, "Open Full Page Tabs (Printable)...", true, false);
        menu.addItem (toggleNotationPanelCmd, "Show Tabs Panel", true, notationPanelVisible);
    }
    else if (menuName == "Lyrics")
    {
        menu.addItem (onlineLyricsCmd, "Find Synced Lyrics Online...", true, false);
        menu.addItem (importLyricsCmd, "Import LRC File...", true, false);
        menu.addItem (aiLyricsCmd, "AI Vocal Transcription (Whisper)", whisperTranscriber.isAvailable(), false);
        menu.addSeparator();
        menu.addItem (openFullPageLyricsCmd, "Open Full Page Lyrics (Printable)...", true, false);
        menu.addItem (toggleLyricsPanelCmd, "Show Lyrics Panel", true, lyricsPanelVisible);
    }
    else if (menuName == "Transport")
    {
        menu.addItem (detectTempoCmd, "Detect Tempo", true, false);
        menu.addSeparator();
        menu.addItem (openArdourStudioCmd, "Open Studio (Ardour)…", true, false);
        menu.addItem (openExternalRecorderCmd, "Open External Recorder (Audacity…)", true, false);
        menu.addItem (importTakeCmd, "Import Take from File…", true, false);
        menu.addItem (recordCmd, "Internal Record / Stop", true, false);
        menu.addSeparator();
        menu.addItem (toggleCountInCmd, "4-Count Intro", true,
                      transportController.isCountInEnabled());
    }
    else if (menuName == "Performance")
    {
        menu.addItem (editSetListCmd, "Edit / Start Set List...", true, false);
        menu.addItem (openStageShowBuilderCmd, "Stage Show Builder...", true, false);
        menu.addItem (performanceNextSongCmd, "Next Song / Start (Foot Pedal)", performanceActive, false);
        menu.addItem (stopPerformanceCmd, "Stop Performance Mode", performanceActive, false);
        menu.addSeparator();
        menu.addItem (toggleStageFxControllerCmd, "Show Stage FX Controller", true,
                      stageFxController.isControllerVisible());
        menu.addItem (openKaraokeOutputCmd, "Open Karaoke Video Output", true, karaokeOutput.isOutputVisible());
        menu.addItem (openStageFxOutputCmd, "Open Stage FX Video Output", true, stageFxOutput.isOutputVisible());
        menu.addItem (cycleKaraokeDisplayCmd, "Move Karaoke to Next Display", karaokeOutput.isOutputVisible(), false);
        menu.addItem (cycleStageFxDisplayCmd, "Move Stage FX to Next Display", stageFxOutput.isOutputVisible(), false);
    }
    else if (menuName == "Help")
    {
        menu.addItem (helpInstructionsCmd, "Instructions…", true, false);
        menu.addSeparator();
        menu.addItem (audioSettingsCmd, "Audio Interface…", true, false);
        menu.addItem (aiToolsCmd, "AI Tools Setup...", true, false);
        menu.addItem (midiControlCmd, "MIDI Control Surface...", true, false);
        menu.addSeparator();
        menu.addItem (aboutCmd, "About JamStudio", true, false);
    }

    juce::ignoreUnused (topLevelMenuIndex);
    return menu;
}

void MainComponent::handleMenuCommand (const int menuItemID, const int /*topLevelMenuIndex*/)
{
    switch (menuItemID)
    {
        case openSongCmd: openSong(); break;
        case saveProjectCmd: saveProject(); break;
        case loadProjectCmd: loadProject(); break;
        case recentProjectsCmd: showRecentProjectsMenu(); break;
        case quitCmd: juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;
        case separateStemsCmd: separateStems(); break;
        case browseTabLibraryCmd: browseTabLibrary(); break;
        case importScoreCmd: importScore(); break;
        case showTabViewCmd: toggleTabView(); break;
        case showSheetViewCmd: toggleSheetView(); break;
        case openFullPageTabsCmd: openFullPageTabs(); break;
        case openFullPageLyricsCmd: openFullPageLyrics(); break;
        case aiTabCmd: transcribeTab(); break;
        case importLyricsCmd: importLyrics(); break;
        case onlineLyricsCmd: findOnlineLyrics(); break;
        case aiLyricsCmd: transcribeLyrics(); break;
        case detectTempoCmd:
            if (currentSongFile.existsAsFile())
                detectTempoFromSong (currentSongFile, true);
            else
                setStatus ("Open a song before detecting tempo.");
            break;
        case toggleCountInCmd:
            transportController.setCountInEnabled (! transportController.isCountInEnabled());
            setStatus (transportController.isCountInEnabled()
                           ? "4-count intro ON - play clicks 1-2-3-4 then starts."
                           : "4-count intro OFF.");
            break;
        case recordCmd: toggleRecording(); break;
        case openArdourStudioCmd: openArdourStudio(); break;
        case openExternalRecorderCmd: openExternalRecorder(); break;
        case importTakeCmd: importTakeFromFile(); break;
        case aiToolsCmd: showAiToolsSetup(); break;
        case midiControlCmd: showMidiControlSetup(); break;
        case audioSettingsCmd: showAudioSettings(); break;
        case toggleLyricsPanelCmd: toggleLyricsPanel(); break;
        case toggleNotationPanelCmd: toggleNotationPanel(); break;
        case toggleStemsPanelCmd: toggleStemsPanel(); break;
        case toggleMixerWindowCmd: toggleMixerWindow(); break;
        case toggleStageFxControllerCmd: toggleStageFxController(); break;
        case dockAttachCmd:
            floatingDock.attach();
            setStatus ("Mixer + Stage FX stuck together.");
            break;
        case dockDetachCmd:
            floatingDock.detach();
            setStatus ("Floating windows unstuck.");
            break;
        case dockSideRightCmd:
        case dockSideLeftCmd:
        case dockSideTopCmd:
        case dockSideBottomCmd:
        {
            auto s = floatingDock.getSettings();
            if (menuItemID == dockSideRightCmd)
                s.dockSide = jamstudio::ui::FloatingDockSettings::Side::right;
            else if (menuItemID == dockSideLeftCmd)
                s.dockSide = jamstudio::ui::FloatingDockSettings::Side::left;
            else if (menuItemID == dockSideTopCmd)
                s.dockSide = jamstudio::ui::FloatingDockSettings::Side::top;
            else
                s.dockSide = jamstudio::ui::FloatingDockSettings::Side::bottom;
            floatingDock.setSettings (s);
            setStatus ("Dock side: " + jamstudio::ui::FloatingDockSettings::sideToString (s.dockSide));
            break;
        }
        case dockGapTightCmd:
        case dockGapNormalCmd:
        case dockGapWideCmd:
        {
            auto s = floatingDock.getSettings();
            s.gapPx = (menuItemID == dockGapTightCmd ? 0 : (menuItemID == dockGapWideCmd ? 12 : 4));
            floatingDock.setSettings (s);
            setStatus ("Dock gap: " + juce::String (s.gapPx) + " px");
            break;
        }
        case dockAutoStickCmd:
        {
            auto s = floatingDock.getSettings();
            s.autoStickOnTouch = ! s.autoStickOnTouch;
            floatingDock.setSettings (s);
            setStatus (s.autoStickOnTouch ? "Auto-stick ON when edges touch."
                                          : "Auto-stick OFF (use <> button).");
            break;
        }
        case dockDetachOnMaxCmd:
        {
            auto s = floatingDock.getSettings();
            s.detachOnMaximise = ! s.detachOnMaximise;
            floatingDock.setSettings (s);
            setStatus (s.detachOnMaximise ? "Will unstick when mixer maximised."
                                          : "Stay stuck when mixer maximised.");
            break;
        }
        case editSetListCmd: openSetListEditor (false); break;
        case openStageShowBuilderCmd: openSetListEditor (true); break;
        case performanceNextSongCmd: performanceTriggerNext(); break;
        case stopPerformanceCmd: stopPerformanceMode(); break;
        case openKaraokeOutputCmd: openKaraokeOutput(); break;
        case openStageFxOutputCmd: openStageFxOutput(); break;
        case cycleKaraokeDisplayCmd: cycleKaraokeDisplay(); break;
        case cycleStageFxDisplayCmd: cycleStageFxDisplay(); break;
        case helpInstructionsCmd:
            jamstudio::ui::HelpBrowserDialog::show (this);
            break;
        case aboutCmd:
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "JamStudio",
                                                    "JamStudio v0.9.6\n"
                                                    "Guitar practice workstation with stems, tabs, lyrics,\n"
                                                    "multi-bus mixer, and stage video.\n\n"
                                                    "Help → Instructions… for a searchable guide.\n\n"
                                                    "Designed by man, engineered and coded by Grok.");
            break;
        default: break;
    }
}

void MainComponent::darkModeSettingChanged()
{
    juce::MessageManager::callAsync ([safeThis = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safeThis != nullptr)
            safeThis->refreshTheme();
    });
}

void MainComponent::refreshTheme()
{
    jamstudio::ui::JamStudioTheme::applyToComponent (*this);
    jamstudio::ui::JamStudioTheme::refreshAll (*this);

    if (auto* laf = dynamic_cast<jamstudio::ui::JamStudioLookAndFeel*> (&getLookAndFeel()))
        laf->refreshTheme();
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &audioInterfaceManager)
    {
        refreshAudioRoutingStatus();
        return;
    }

    if (source == &transportController.getStemMixer())
    {
        rebuildStemLanes();
        rebuildMixerWindow();
    }

    if (source == &transportController && performanceActive)
    {
        const auto playing = transportController.isPlaying();

        // Song finished naturally -> pause between songs and wait for foot pedal.
        if (performanceWasPlaying && ! playing)
        {
            const auto length = transportController.getLengthInSeconds();
            const auto pos = transportController.getPosition();

            if (length > 0.5 && pos >= length - 0.15)
                onPerformanceSongEnded();
        }

        performanceWasPlaying = playing;
    }

    transportBar.updatePositionSlider();
}

void MainComponent::saveProject()
{
    if (transportController.getStemMixer().getNumStems() == 0)
    {
        setStatus ("Load a song before saving a project.");
        return;
    }

    auto defaultFile = currentProjectFile;

    if (! defaultFile.existsAsFile())
    {
        defaultFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("JamStudio")
            .getChildFile ("Projects")
            .getChildFile (currentSongFile.existsAsFile()
                ? currentSongFile.getFileNameWithoutExtension() + ".jamstudio"
                : "untitled.jamstudio");
    }

    fileChooser = std::make_unique<juce::FileChooser> ("Save JamStudio project", defaultFile, "*.jamstudio");

    const auto chooserFlags = juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();

        if (file == juce::File())
            return;

        if (! file.hasFileExtension ("jamstudio"))
            file = file.withFileExtension (".jamstudio");

        auto data = jamstudio::project::ProjectManager::captureState (
            currentSongFile, currentScoreFile, currentLyricsFile,
            currentScore, currentLyrics, transportController, transportBar);

        if (jamstudio::project::ProjectManager::saveProject (file, data))
        {
            currentProjectFile = file;
            recentProjects.add (file);

            // Switch mixer to permanent stem copies under {Name}.media/stems/
            juce::Array<juce::File> permanentStems;

            for (const auto& stem : data.stems)
            {
                const juce::File f (stem.filePath);

                if (f.existsAsFile())
                    permanentStems.add (f);
            }

            if (! permanentStems.isEmpty())
            {
                loadStemsIntoMixer (permanentStems);

                auto& mixer = transportController.getStemMixer();

                for (int i = 0; i < mixer.getNumStems() && i < data.stems.size(); ++i)
                {
                    const auto& s = data.stems.getReference (i);
                    mixer.setStemMuted (i, s.muted);
                    mixer.setStemSolo (i, s.solo);
                    mixer.setStemVolume (i, s.volume);

                    if (s.name.isNotEmpty())
                        mixer.setStemName (i, s.name);
                }
            }

            setStatus ("Project saved: " + file.getFileName()
                       + " (stems stored in " + file.getFileNameWithoutExtension() + ".media/)");
        }
        else
        {
            setStatus ("Failed to save project - stem files missing or could not be copied.");
        }
    });
}

void MainComponent::loadProject()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Load JamStudio project",
                                                      juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                          .getChildFile ("JamStudio")
                                                          .getChildFile ("Projects"),
                                                      "*.jamstudio");

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (file.existsAsFile())
            loadProjectFile (file);
    });
}

void MainComponent::loadProjectFile (const juce::File& file)
{
    jamstudio::project::ProjectData data;
    juce::String error;

    if (! jamstudio::project::ProjectManager::loadProject (file, data, error))
    {
        setStatus ("Failed to load project: " + error);
        return;
    }

    const auto resolvedStemCount = jamstudio::project::ProjectManager::countResolvedStems (data, file);
    const auto needsRecovery = jamstudio::project::ProjectManager::needsStemRecovery (data, file);

    if (! jamstudio::project::ProjectManager::applyState (data, transportController, transportBar,
                                                        currentScore, currentLyrics,
                                                        currentSongFile, currentScoreFile, currentLyricsFile,
                                                        error, file))
    {
        // Still try to surface embedded score/lyrics even when all audio is gone.
        if (data.hasEmbeddedScore)
            juce::ignoreUnused (jamstudio::notation::Score::fromVar (data.embeddedScore, currentScore));

        if (data.hasEmbeddedLyrics)
            juce::ignoreUnused (jamstudio::notation::LyricsTrack::fromVar (data.embeddedLyrics, currentLyrics));

        if (! currentScore.isEmpty())
            applyScore (currentScore, false);

        if (! currentLyrics.isEmpty())
            lyricsView.setLyrics (currentLyrics);

        currentProjectFile = file;
        setStatus ("Failed to restore audio: " + error);
        revealWorkspacePanels();

        if (needsRecovery && juce::File (data.songFilePath).existsAsFile() && demucsSeparator.isAvailable())
        {
            currentSongFile = juce::File (data.songFilePath);
            recoverMissingProjectStems (file);
        }

        return;
    }

    currentProjectFile = file;
    recentProjects.add (file);
    waveformDisplay.setSourceFile (currentSongFile.existsAsFile() ? currentSongFile
                                                                  : juce::File (data.stems.getFirst().filePath));

    if (! currentScore.isEmpty())
        applyScore (currentScore, currentLyrics.isEmpty() && ! currentLyricsFile.existsAsFile());
    else
    {
        notationView.clear();
        notationHeaderBar.setHasScore (false);
        notationHeaderBar.setTitle ({});
    }

    if (! currentLyrics.isEmpty())
        lyricsView.setLyrics (currentLyrics);
    else if (currentScore.isEmpty())
        lyricsView.clear();

    recordingTakeManager.clear();

    for (const auto& stemState : data.stems)
    {
        if (stemState.name.startsWith ("Take "))
            recordingTakeManager.addRestoredTake (juce::File (stemState.filePath), stemState.name);
    }

    rebuildStemLanes();
    rebuildMixerWindow();
    revealWorkspacePanels();

    juce::StringArray loadedParts;

    if (! currentScore.isEmpty())
        loadedParts.add ("notation");

    if (! currentLyrics.isEmpty())
        loadedParts.add ("lyrics");

    if (resolvedStemCount >= 2)
        loadedParts.add (juce::String (resolvedStemCount) + " stems");
    else if (transportController.getStemMixer().getNumStems() > 0)
        loadedParts.add ("mix only");

    setStatus ("Project loaded: " + file.getFileName()
               + (loadedParts.isEmpty() ? "" : " (" + loadedParts.joinIntoString (" + ") + ")"));

    // Old projects pointed at /tmp demucs output - re-separate and pack into .media permanently.
    if (needsRecovery && currentSongFile.existsAsFile())
    {
        if (demucsSeparator.isAvailable())
            recoverMissingProjectStems (file);
        else
            setStatus ("Project loaded with lyrics/tabs, but stems are missing (were under /tmp). "
                       "Install Demucs (Help -> AI Tools), then open this project again to recover.");
    }
}

void MainComponent::recoverMissingProjectStems (const juce::File& projectFile)
{
    if (! currentSongFile.existsAsFile())
    {
        setStatus ("Cannot recover stems - original song file not found.");
        return;
    }

    if (! demucsSeparator.isAvailable())
    {
        setStatus ("Cannot recover stems - Demucs is not available.");
        return;
    }

    if (backgroundTaskActive)
    {
        setStatus ("A background job is already running; stems will not recover yet.");
        return;
    }

    beginBackgroundTask ("Recovering missing stems from song (one-time)...",
                         [this] { demucsSeparator.cancel(); });

    const auto jobGeneration = backgroundTaskGeneration.load();
    const auto songForRecovery = currentSongFile;
    const auto projectToUpdate = projectFile;

    demucsSeparator.separateAsync (songForRecovery,
        [this, jobGeneration, projectToUpdate] (const jamstudio::ai::SeparationResult& result)
        {
            if (jobGeneration != backgroundTaskGeneration.load())
                return;

            endBackgroundTask();

            if (! result.success || result.stemFiles.isEmpty())
            {
                setStatus (result.errorMessage.isNotEmpty()
                               ? result.errorMessage
                               : "Stem recovery failed. Try Stems -> Separate Stems, then Save Project.");
                return;
            }

            loadStemsIntoMixer (result.stemFiles);
            mixerWindow.showMixer (true);

            // Pack into permanent project media and rewrite the .jamstudio paths.
            auto data = jamstudio::project::ProjectManager::captureState (
                currentSongFile, currentScoreFile, currentLyricsFile,
                currentScore, currentLyrics, transportController, transportBar);

            if (jamstudio::project::ProjectManager::saveProject (projectToUpdate, data))
            {
                currentProjectFile = projectToUpdate;
                recentProjects.add (projectToUpdate);

                juce::Array<juce::File> permanent;

                for (const auto& stem : data.stems)
                {
                    const juce::File f (stem.filePath);

                    if (f.existsAsFile())
                        permanent.add (f);
                }

                if (! permanent.isEmpty())
                {
                    loadStemsIntoMixer (permanent);

                    auto& mixer = transportController.getStemMixer();

                    for (int i = 0; i < mixer.getNumStems() && i < data.stems.size(); ++i)
                    {
                        const auto& s = data.stems.getReference (i);
                        mixer.setStemMuted (i, s.muted);
                        mixer.setStemSolo (i, s.solo);
                        mixer.setStemVolume (i, s.volume);

                        if (s.name.isNotEmpty())
                            mixer.setStemName (i, s.name);
                    }
                }

                setStatus ("Stems recovered and saved to "
                           + projectToUpdate.getFileNameWithoutExtension()
                           + ".media/stems/ (" + juce::String (result.stemFiles.size()) + " channels).");
            }
            else
            {
                setStatus ("Stems recovered in session, but could not update project file. "
                           "Use Project -> Save Project to keep them.");
            }
        },
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            const auto pct = juce::roundToInt (progress * 100.0f);
            setStatus ("Recovering stems: " + (pct > 0 ? (juce::String (pct) + "% - ") : juce::String()) + message);
        });
}

void MainComponent::importLyrics()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import LRC lyrics file",
                                                      juce::File {},
                                                      "*.lrc;*.txt");

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        jamstudio::notation::LyricsTrack importedLyrics;
        juce::String error;

        if (! jamstudio::notation::LrcParser::parseFile (file, importedLyrics, error))
        {
            setStatus ("Lyrics import failed: " + error);
            return;
        }

        currentLyricsFile = file;
        currentLyrics = importedLyrics;
        lyricsView.setLyrics (currentLyrics);
        if (fullPageLyricsWindow.isLyricsWindowVisible())
            fullPageLyricsWindow.setLyrics (currentLyrics);
        lyricsPanelVisible = true;
        applyPanelVisibility();
        toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::lyrics);
        setStatus ("Lyrics loaded: " + file.getFileName() + " (" + juce::String (currentLyrics.getNumLines()) + " lines)");
    });
}

void MainComponent::findOnlineLyrics()
{
    juce::File source = currentSongFile;

    if (! source.existsAsFile())
    {
        // Fall back to first loaded stem / song audio
        if (const auto* stem = transportController.getStemMixer().getStem (0))
            source = stem->getFile();
    }

    if (! source.existsAsFile())
    {
        setStatus ("Open a song first so JamStudio can read title/artist metadata.");
        return;
    }

    auto metadata = jamstudio::notation::extractSongMetadata (source, transportController.getFormatManager());
    setStatus ("Looking up online lyrics for: " + metadata.displayLabel());

    jamstudio::ui::OnlineLyricsDialog::show (this, std::move (metadata),
        [this] (jamstudio::notation::LyricsTrack lyrics)
        {
            currentLyricsFile = juce::File();
            currentLyrics = std::move (lyrics);
            lyricsView.setLyrics (currentLyrics);
            if (fullPageLyricsWindow.isLyricsWindowVisible())
                fullPageLyricsWindow.setLyrics (currentLyrics);
            lyricsPanelVisible = true;
            applyPanelVisibility();
            toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::lyrics);

            const auto wordInfo = currentLyrics.hasWordTimings() ? " (word-level if present)" : " (line-synced LRC)";
            setStatus ("Online lyrics applied: " + juce::String (currentLyrics.getNumLines())
                       + " lines" + wordInfo + " - " + currentLyrics.getTitle());
        });
}

void MainComponent::showRecentProjectsMenu()
{
    juce::PopupMenu menu;
    recentProjects.buildMenu (menu);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
    {
        if (result <= 0)
            return;

        const auto paths = recentProjects.getProjectPaths();
        const auto index = result - 1;

        if (juce::isPositiveAndBelow (index, paths.size()))
            loadProjectFile (juce::File (paths[index]));
    });
}

void MainComponent::openSong()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select a song file",
                                                      juce::File {},
                                                      "*.wav;*.mp3;*.flac;*.ogg;*.aiff");

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        currentSongFile = file;
        recordingTakeManager.clear();
        transportController.stop();
        waveformDisplay.setSourceFile (file);

        if (transportController.getStemMixer().loadStems ({ file }))
        {
            detectTempoFromSong (file, false);
            rebuildStemLanes();
            rebuildMixerWindow();
            revealWorkspacePanels();
            setStatus ("Loaded: " + file.getFileName()
                       + " @ " + juce::String (static_cast<int> (transportBar.getBpm()))
                       + " BPM - Lyrics / Tabs / Stems / Mixer ready.");
        }
        else
        {
            setStatus ("Failed to load: " + file.getFileName());
        }
    });
}

void MainComponent::browseTabLibrary()
{
    jamstudio::ui::TabLibraryBrowserDialog::show (this,
        [this] (const juce::File& file, const jamstudio::notation::TabLibraryEntry& entry)
        {
            importScoreFile (file, entry.title);
        });
}

void MainComponent::importScore()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Import MusicXML score",
                                                      juce::File {},
                                                      "*.musicxml;*.xml;*.mxl");

    const auto chooserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        importScoreFile (file, file.getFileNameWithoutExtension());
    });
}

void MainComponent::importScoreFile (const juce::File& file, const juce::String& displayName)
{
    if (! file.existsAsFile())
    {
        setStatus ("Score file not found.");
        return;
    }

    jamstudio::notation::Score importedScore;
    juce::String error;

    if (! jamstudio::notation::MusicXmlParser::parseFile (file, importedScore, error))
    {
        setStatus ("Score import failed: " + error);
        return;
    }

    currentScoreFile = file;
    applyScore (importedScore, true);
    toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::notation);

    auto message = "Score loaded: " + displayName + " (" + juce::String (currentScore.getNumMeasures()) + " measures)";

    if (currentScore.hasLyrics())
        message += " with synced lyrics";

    setStatus (message);
    resized();
}

void MainComponent::applyScore (const jamstudio::notation::Score& score, const bool replaceLyricsFromScore)
{
    currentScore = score;

    if (currentScore.getNotationMode() == jamstudio::notation::NotationMode::hidden
        && ! currentScore.isEmpty())
    {
        const auto* activePart = currentScore.getPart (currentScore.getActivePartIndex());
        currentScore.setNotationMode (activePart != nullptr && activePart->notationMode == jamstudio::notation::NotationMode::tab
                                          ? jamstudio::notation::NotationMode::tab
                                          : jamstudio::notation::NotationMode::standard);
    }

    notationView.setScore (currentScore);
    transportController.getMetronome().setBpm (currentScore.getTempo());
    notationHeaderBar.setHasScore (! currentScore.isEmpty());
    notationHeaderBar.setTitle (currentScore.getTitle());
    notationHeaderBar.setParts (currentScore.getPartNames(), currentScore.getActivePartIndex());
    syncNotationUiState();

    if (fullPageTabsWindow.isTabsWindowVisible())
        fullPageTabsWindow.setScore (currentScore);

    if (replaceLyricsFromScore && currentScore.hasLyrics())
    {
        currentLyricsFile = juce::File();
        currentLyrics = jamstudio::notation::ScoreLyricsExtractor::fromScore (currentScore);
        lyricsView.setLyrics (currentLyrics);
        lyricsPanelVisible = true;
    }

    notationPanelVisible = true;
    updateNotationPanelVisibility();
    applyPanelVisibility();
}

void MainComponent::setNotationDisplayMode (const jamstudio::notation::NotationMode mode)
{
    if (currentScore.isEmpty())
    {
        notationPanelVisible = true;
        applyPanelVisibility();
        setStatus ("Tabs panel open - import MusicXML or run AI Tab.");
        return;
    }

    currentScore.setNotationMode (mode);
    notationView.setScore (currentScore);
    syncNotationUiState();
    notationPanelVisible = true;
    updateNotationPanelVisibility();
    applyPanelVisibility();

    if (fullPageTabsWindow.isTabsWindowVisible())
        fullPageTabsWindow.setScore (currentScore);

    if (mode == jamstudio::notation::NotationMode::hidden)
        setStatus ("Notation content hidden. Tabs panel stays available.");
    else if (mode == jamstudio::notation::NotationMode::tab)
        setStatus ("Showing tab for " + currentScore.getActivePart().name + ".");
    else
        setStatus ("Showing sheet for " + currentScore.getActivePart().name + ".");
}

void MainComponent::toggleTabView()
{
    if (currentScore.isEmpty())
    {
        notationPanelVisible = true;
        applyPanelVisibility();
        setStatus ("Tabs panel open - import MusicXML or run AI Tab.");
        return;
    }

    if (currentScore.isNotationVisible()
        && currentScore.getNotationMode() == jamstudio::notation::NotationMode::tab)
        setNotationDisplayMode (jamstudio::notation::NotationMode::hidden);
    else
        setNotationDisplayMode (jamstudio::notation::NotationMode::tab);
}

void MainComponent::toggleSheetView()
{
    if (currentScore.isEmpty())
    {
        notationPanelVisible = true;
        applyPanelVisibility();
        setStatus ("Tabs panel open - import MusicXML or run AI Tab.");
        return;
    }

    if (currentScore.isNotationVisible()
        && currentScore.getNotationMode() == jamstudio::notation::NotationMode::standard)
        setNotationDisplayMode (jamstudio::notation::NotationMode::hidden);
    else
        setNotationDisplayMode (jamstudio::notation::NotationMode::standard);
}

void MainComponent::openFullPageTabs()
{
    if (currentScore.isEmpty())
    {
        setStatus ("Load or generate notation before opening full-page tabs.");
        return;
    }

    // Prefer tab mode for the printable window when nothing is selected yet.
    if (currentScore.getNotationMode() == jamstudio::notation::NotationMode::hidden)
        currentScore.setNotationMode (jamstudio::notation::NotationMode::tab);

    fullPageTabsWindow.setScore (currentScore);
    fullPageTabsWindow.showWindow (true);
    setStatus ("Full page tabs open - use Print or Export PNG for a printable copy.");
}

void MainComponent::openFullPageLyrics()
{
    if (currentLyrics.isEmpty())
    {
        setStatus ("Load or generate lyrics before opening full-page lyrics.");
        return;
    }

    fullPageLyricsWindow.setLyrics (currentLyrics);
    fullPageLyricsWindow.showWindow (true);
    setStatus ("Full page lyrics open - use Print or Export PNG for a printable sheet.");
}

void MainComponent::setActiveScorePart (const int partIndex)
{
    if (currentScore.isEmpty())
        return;

    // Keep the user's Tab/Sheet choice - don't force guitar-only tab mode per part.
    const auto modeBefore = currentScore.getNotationMode();
    currentScore.setActivePartIndex (partIndex);

    if (modeBefore != jamstudio::notation::NotationMode::hidden)
        currentScore.setNotationMode (modeBefore);

    notationView.setScore (currentScore);
    notationHeaderBar.setParts (currentScore.getPartNames(), currentScore.getActivePartIndex());
    syncNotationUiState();
    updateNotationPanelVisibility();

    if (fullPageTabsWindow.isTabsWindowVisible())
        fullPageTabsWindow.setScore (currentScore);

    const auto partName = currentScore.getActivePart().name.isNotEmpty()
                              ? currentScore.getActivePart().name
                              : ("Part " + juce::String (partIndex + 1));
    const auto modeLabel = currentScore.getNotationMode() == jamstudio::notation::NotationMode::tab
                               ? "tab"
                               : "sheet";
    setStatus ("Viewing " + partName + " (" + modeLabel + "). Use Part menu or Tab/Sheet to change.");
}

void MainComponent::updateNotationPanelVisibility()
{
    // Panel shell stays available; score content fills when present.
    if (notationPanelVisible)
    {
        notationHeaderBar.setVisible (! currentScore.isEmpty());
        notationViewport.setVisible (true);
    }

    resized();
}

void MainComponent::syncNotationUiState()
{
    notationHeaderBar.setNotationMode (currentScore.getNotationMode());
    toolbarTabs.setNotationViewState (currentScore.getNotationMode());
}

juce::File MainComponent::findStemFileForType (const jamstudio::audio::StemType preferredType)
{
    const auto& mixer = transportController.getStemMixer();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        if (const auto* stem = mixer.getStem (i))
        {
            if (stem->getType() == preferredType && stem->getFile().existsAsFile())
                return stem->getFile();
        }
    }

    return {};
}

juce::File MainComponent::findMelodicStemFile()
{
    // Prefer guitar for AI tab generation - this app is for guitar practice.
    for (const auto stemType : { jamstudio::audio::StemType::guitar,
                                 jamstudio::audio::StemType::other,
                                 jamstudio::audio::StemType::bass,
                                 jamstudio::audio::StemType::piano,
                                 jamstudio::audio::StemType::vocals })
    {
        if (const auto file = findStemFileForType (stemType); file.existsAsFile())
            return file;
    }

    const auto& mixer = transportController.getStemMixer();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        if (const auto* stem = mixer.getStem (i))
        {
            if (stem->getType() != jamstudio::audio::StemType::drums && stem->getFile().existsAsFile())
                return stem->getFile();
        }
    }

    return {};
}

juce::Array<jamstudio::ai::AiToolInfo> MainComponent::getAiToolStatuses() const
{
    return jamstudio::ai::AiToolsCatalog::getToolStatuses (demucsSeparator, whisperTranscriber, basicPitchTranscriber);
}

void MainComponent::showAiToolsSetup()
{
    jamstudio::ui::AiToolsSetupDialog::show (this, getAiToolStatuses());
}

void MainComponent::showMidiControlSetup()
{
    jamstudio::ui::MidiControlDialog::show (this, midiControlSurface);
}

void MainComponent::showAudioSettings()
{
    jamstudio::ui::AudioSettingsDialog::show (this, audioInterfaceManager);
}

void MainComponent::refreshAudioRoutingStatus()
{
    setStatus (audioInterfaceManager.getStatusSummary());
}

void MainComponent::refreshMixerUiFromMidi()
{
    auto& mixer = transportController.getStemMixer();
    mixerWindow.syncFromMixer (mixer);
    transportBar.setMasterVolume (mixer.getMasterVolume());
    transportBar.setMetronomeEnabled (transportController.getMetronome().isEnabled());
    transportBar.updatePositionSlider();
}

void MainComponent::detectTempoFromSong (const juce::File& audioFile, const bool announceResult)
{
    if (! audioFile.existsAsFile())
    {
        if (announceResult)
            setStatus ("No audio file available for tempo detection.");

        return;
    }

    double detectedBpm = 0.0;

    if (jamstudio::audio::TempoDetector::detectFromFile (audioFile,
                                                        transportController.getFormatManager(),
                                                        detectedBpm))
    {
        transportBar.setBpm (detectedBpm);

        if (announceResult)
            setStatus ("Detected tempo: " + juce::String (static_cast<int> (detectedBpm)) + " BPM");
    }
    else if (announceResult)
    {
        setStatus ("Could not detect tempo. Adjust BPM manually.");
    }
}

void MainComponent::beginBackgroundTask (const juce::String& message, std::function<void()> onCancel)
{
    ++backgroundTaskGeneration;
    const auto generation = backgroundTaskGeneration.load();
    backgroundTaskActive = true;

    // No top progress strip - message + Cancel live in the bottom status bar.
    separationProgress.setVisible (false);
    statusCancelButton.setVisible (true);
    statusCancelButton.onClick = [this, generation, onCancel = std::move (onCancel)]
    {
        if (generation != backgroundTaskGeneration.load())
            return;

        if (onCancel)
            onCancel();

        endBackgroundTask();
        setStatus ("Cancelled.");
    };

    setStatus (message);
    resized();
}

void MainComponent::endBackgroundTask()
{
    if (! backgroundTaskActive)
        return;

    backgroundTaskActive = false;
    statusCancelButton.setVisible (false);
    statusCancelButton.onClick = nullptr;
    separationProgress.reset();
    separationProgress.setVisible (false);
    resized();
}

void MainComponent::transcribeLyrics()
{
    const auto tools = getAiToolStatuses();

    if (! whisperTranscriber.isAvailable())
    {
        setStatus (jamstudio::ai::AiToolsCatalog::buildUnavailableHint ("whisper", tools));
        return;
    }

    auto vocalsFile = findStemFileForType (jamstudio::audio::StemType::vocals);

    if (! vocalsFile.existsAsFile())
        vocalsFile = currentSongFile;

    if (! vocalsFile.existsAsFile())
    {
        setStatus ("Open a song or separate stems before transcribing lyrics.");
        return;
    }

    beginBackgroundTask ("Transcribing vocals with Whisper...",
                         [this] { whisperTranscriber.cancel(); });

    const auto jobGeneration = backgroundTaskGeneration.load();

    whisperTranscriber.transcribeAsync (vocalsFile,
        [this, jobGeneration] (const jamstudio::ai::TranscriptionResult& result)
        {
            // Ignore late results after a newer job started or cancel already ended the UI.
            if (jobGeneration != backgroundTaskGeneration.load())
                return;

            endBackgroundTask();

            if (! result.success)
            {
                // Cancel is a normal outcome - keep the app open and show status only.
                setStatus (result.errorMessage.isNotEmpty() ? result.errorMessage : "Transcription cancelled.");
                return;
            }

            jamstudio::ui::TranscriptionCorrectionDialog::showLyrics (this, result.lyrics,
                [this] (const jamstudio::notation::LyricsTrack& corrected)
                {
                    currentLyricsFile = juce::File();
                    currentLyrics = corrected;
                    lyricsView.setLyrics (currentLyrics);
                    toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::lyrics);

                    const auto wordInfo = currentLyrics.hasWordTimings() ? " with word-level timing" : "";
                    setStatus ("AI lyrics applied: " + juce::String (currentLyrics.getNumLines()) + " lines" + wordInfo + ".");
                });
        },
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            const auto pct = juce::roundToInt (progress * 100.0f);
            setStatus ((pct > 0 ? (juce::String (pct) + "% - ") : juce::String()) + message);
        });
}

void MainComponent::transcribeTab()
{
    const auto tools = getAiToolStatuses();

    if (! basicPitchTranscriber.isAvailable())
    {
        setStatus (jamstudio::ai::AiToolsCatalog::buildUnavailableHint ("basic-pitch", tools));
        return;
    }

    const auto melodicFile = findMelodicStemFile();

    if (! melodicFile.existsAsFile())
    {
        setStatus ("Open a song or separate stems before transcribing tab.");
        return;
    }

    beginBackgroundTask ("Transcribing notes with basic-pitch...",
                         [this] { basicPitchTranscriber.cancel(); });

    const auto jobGeneration = backgroundTaskGeneration.load();

    basicPitchTranscriber.transcribeAsync (melodicFile,
        [this, jobGeneration] (const jamstudio::ai::PitchTranscriptionResult& result)
        {
            if (jobGeneration != backgroundTaskGeneration.load())
                return;

            endBackgroundTask();

            if (! result.success)
            {
                setStatus (result.errorMessage.isNotEmpty() ? result.errorMessage : "Transcription cancelled.");
                return;
            }

            jamstudio::ui::TranscriptionCorrectionDialog::showScore (this, result.score,
                [this] (const jamstudio::notation::Score& corrected)
                {
                    currentScoreFile = juce::File();
                    applyScore (corrected, false);
                    toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::notation);
                    setStatus ("AI tab applied: " + juce::String (currentScore.getNumMeasures()) + " measures.");
                    resized();
                });
        },
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            const auto pct = juce::roundToInt (progress * 100.0f);
            setStatus ((pct > 0 ? (juce::String (pct) + "% - ") : juce::String()) + message);
        });
}

void MainComponent::separateStems()
{
    if (! currentSongFile.existsAsFile())
    {
        setStatus ("Open a song file before separating stems.");
        return;
    }

    const auto tools = getAiToolStatuses();

    if (! demucsSeparator.isAvailable())
    {
        setStatus (jamstudio::ai::AiToolsCatalog::buildUnavailableHint ("demucs", tools));
        return;
    }

    beginBackgroundTask ("Separating stems (guitar model)...",
                         [this] { demucsSeparator.cancel(); });

    const auto jobGeneration = backgroundTaskGeneration.load();

    demucsSeparator.separateAsync (currentSongFile,
        [this, jobGeneration] (const jamstudio::ai::SeparationResult& result)
        {
            if (jobGeneration != backgroundTaskGeneration.load())
                return;

            endBackgroundTask();

            if (! result.success)
            {
                setStatus (result.errorMessage.isNotEmpty() ? result.errorMessage : "Separation cancelled.");
                return;
            }

            loadStemsIntoMixer (result.stemFiles);

            const auto hasGuitar = findStemFileForType (jamstudio::audio::StemType::guitar).existsAsFile();
            setStatus (hasGuitar
                           ? "Separation complete - Guitar stem ready. Solo Guitar to learn the part, "
                             "or mute Guitar to play along with the band."
                           : "Separation complete. " + juce::String (result.stemFiles.size())
                                 + " stems loaded.");
        },
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            const auto pct = juce::roundToInt (progress * 100.0f);
            setStatus ((pct > 0 ? (juce::String (pct) + "% - ") : juce::String()) + message);
        });
}

void MainComponent::toggleRecording()
{
    if (audioRecorder.isRecording())
    {
        const auto savedFile = audioRecorder.stopRecording();
        toolbarTabs.setRecordingActive (false);
        transportBar.setRecordingActive (false);

        if (! savedFile.existsAsFile())
        {
            setStatus ("Recording stopped, but no audio was captured.");
            refreshRecordingTakesPanel();
            return;
        }

        const auto exportResult = recordingExporter.exportRecording (savedFile);
        const auto takeResult = recordingTakeManager.addTake (savedFile);

        for (const auto& pruned : takeResult.prunedFiles)
            transportController.getStemMixer().removeStemByFile (pruned);

        loadRecordingAsStem (savedFile, takeResult.take.displayName);
        refreshRecordingTakesPanel();

        juce::StringArray exportedPaths;
        exportedPaths.add (savedFile.getFullPathName());

        if (exportResult.oggFile.existsAsFile())
            exportedPaths.add (exportResult.oggFile.getFullPathName());

        if (exportResult.mp3File.existsAsFile())
            exportedPaths.add (exportResult.mp3File.getFullPathName());

        const auto takeLabel = takeResult.take.displayName;
        setStatus (takeLabel + " added to mixer. Exported: " + exportedPaths.joinIntoString (", "));
        return;
    }

    const auto destination = getDefaultRecordingFile();

    if (audioRecorder.startRecording (destination))
    {
        toolbarTabs.setRecordingActive (true);
        transportBar.setRecordingActive (true);
        setStatus ("Recording... play along with the backing, then press REC/STOP when finished.");
    }
    else
    {
        setStatus ("Could not start recording. Check that an audio input device is available (and sample rate is running).");
    }
}

void MainComponent::refreshRecordingTakesPanel()
{
    recordingTakesPanel.setVisible (currentMode == jamstudio::ui::StartupWizard::Mode::recording
                                    && workspaceReady);
    recordingTakesPanel.refresh();
    resized();
}

void MainComponent::loadRecordingAsStem (const juce::File& recordingFile,
                                           const juce::String& displayName)
{
    if (! recordingFile.existsAsFile())
        return;

    auto& mixer = transportController.getStemMixer();

    if (! mixer.loadStem (recordingFile))
        return;

    if (auto* stem = mixer.getStem (mixer.getNumStems() - 1))
    {
        stem->setType (jamstudio::audio::StemType::recording);
        stem->setName (displayName);
    }

    rebuildStemLanes();
    rebuildMixerWindow();
}

void MainComponent::loadStemsIntoMixer (const juce::Array<juce::File>& stemFiles)
{
    recordingTakeManager.clear();
    transportController.stop();
    transportController.getStemMixer().loadStems (stemFiles);

    if (stemFiles.size() > 0)
    {
        // Prefer original song for the main overview waveform when available.
        if (currentSongFile.existsAsFile())
            waveformDisplay.setSourceFile (currentSongFile);
        else
            waveformDisplay.setSourceFile (stemFiles.getReference (0));

        if (currentSongFile.existsAsFile())
            detectTempoFromSong (currentSongFile, false);
        else
            detectTempoFromSong (stemFiles.getReference (0), false);
    }

    rebuildStemLanes();
    rebuildMixerWindow();
    mixerWindow.showMixer (true);
    updatePanelToggleStates();
    resized();
}

void MainComponent::rebuildStemLanes()
{
    stemContainer.removeAllChildren();

    auto& mixer = transportController.getStemMixer();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        if (const auto* stem = mixer.getStem (i))
        {
            auto lane = std::make_unique<jamstudio::ui::StemLane> (
                i, *stem,
                transportController.getFormatManager(),
                thumbnailCache,
                transportController);

            stemContainer.addAndMakeVisible (lane.release());
        }
    }

    layoutStemLanes();
    resized();
}

void MainComponent::rebuildMixerWindow()
{
    mixerWindow.rebuild (transportController.getStemMixer(),
                         [this] (const int /*index*/)
                         {
                             // Strip applies mute/solo/sends directly; refresh lanes if needed.
                             juce::ignoreUnused (this);
                         });
}

void MainComponent::setStatus (const juce::String& message)
{
    statusLabel.setText (message, juce::dontSendNotification);
}

juce::File MainComponent::getDefaultRecordingFile() const
{
    const auto timestamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("JamStudio")
        .getChildFile ("Recordings")
        .getChildFile ("recording-" + timestamp + ".wav");
}

juce::File MainComponent::getProjectsDirectory()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("JamStudio")
                   .getChildFile ("Projects");
    dir.createDirectory();
    return dir;
}

void MainComponent::setupStartupWizard()
{
    addAndMakeVisible (startupWizard);
    startupWizard.toFront (false);

    startupWizard.setModeChosenCallback ([this] (const jamstudio::ui::StartupWizard::Mode mode)
    {
        if (mode == jamstudio::ui::StartupWizard::Mode::practice)
        {
            currentMode = mode;
            setStatus ("Practice: open a project or choose a song to set up automatically.");
            return;
        }

        if (mode == jamstudio::ui::StartupWizard::Mode::performance)
        {
            currentMode = mode;
            openSetListEditor (false);
            return;
        }

        if (mode == jamstudio::ui::StartupWizard::Mode::stageShowBuilder)
        {
            currentMode = mode;
            openSetListEditor (true);
            return;
        }

        if (mode == jamstudio::ui::StartupWizard::Mode::recording)
        {
            currentMode = mode;
            setStatus ("Recording: choose a backing project, song, or empty session.");
            return;
        }

        enterWorkspaceMode (mode);
    });

    startupWizard.setPracticeChoiceCallback ([this] (const jamstudio::ui::StartupWizard::PracticeChoice choice)
    {
        handlePracticeChoice (choice);
    });

    startupWizard.setRecordingChoiceCallback ([this] (const jamstudio::ui::StartupWizard::RecordingChoice choice)
    {
        handleRecordingChoice (choice);
    });

    workspaceReady = false;
    setStatus ("Welcome - choose Practice, Performance, or Recording.");
    resized();
}

void MainComponent::hideStartupWizard()
{
    startupWizard.setVisible (false);
    workspaceReady = true;
    resized();
}

void MainComponent::enterWorkspaceMode (const jamstudio::ui::StartupWizard::Mode mode)
{
    currentMode = mode;
    hideStartupWizard();
    revealWorkspacePanels();

    switch (mode)
    {
        case jamstudio::ui::StartupWizard::Mode::practice:
            setStatus ("Practice workspace ready. Open a song or project from the File/Project menus.");
            break;
        case jamstudio::ui::StartupWizard::Mode::performance:
            setStatus ("Performance mode - build a set list under Performance menu.");
            break;
        case jamstudio::ui::StartupWizard::Mode::stageShowBuilder:
            setStatus ("Stage Show Builder - pin videos/slideshows to songs in your set list.");
            break;
        case jamstudio::ui::StartupWizard::Mode::recording:
            enterRecordingWorkspace();
            break;
    }
}

void MainComponent::enterRecordingWorkspace()
{
    // Recording: focus stems + mixer; tabs/lyrics optional.
    lyricsPanelVisible = false;
    notationPanelVisible = false;
    stemsPanelVisible = true;
    applyPanelVisibility();
    mixerWindow.showMixer (true);
    updatePanelToggleStates();
    refreshRecordingTakesPanel();

   #if JUCE_LINUX
    if (jamstudio::audio::ArdourCompanion::isAvailable())
    {
        recordingTakesPanel.setPreferredRecorderName ("Ardour Studio");
        setStatus ("Recording mode — Open Studio (Ardour) sets up stems + hands off your interface. "
                   "Import Take when you finish in Ardour.");
        return;
    }
   #endif

    const auto preferred = jamstudio::audio::ExternalRecorder::getPreferred();
    recordingTakesPanel.setPreferredRecorderName (preferred.name);

    if (preferred.name.isNotEmpty())
        setStatus ("Recording mode — REC / Open " + preferred.name
                   + " bounces your mix and opens it for plugins & amp sims. Import Take when done.");
    else
        setStatus ("Recording mode — install Ardour (sudo apt install ardour) for Open Studio, "
                   "or Audacity for a lighter external recorder.");
}

void MainComponent::openArdourStudio()
{
   #if ! JUCE_LINUX
    setStatus ("Open Studio (Ardour) is available on Linux.");
    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Open Studio",
        "The Ardour companion currently targets Linux.\n"
        "Use Transport → Open External Recorder on other platforms.");
    return;
   #else
    transportController.pause();
    transportController.stop();

    jamstudio::audio::ArdourHandoffContext ctx;
    ctx.songTitle = currentSongFile.existsAsFile()
                        ? currentSongFile.getFileNameWithoutExtension()
                        : juce::String ("JamStudio-Session");
    ctx.songFile = currentSongFile;
    ctx.tempoBpm = transportController.getMetronome().getBpm();

    {
        const auto setup = audioDeviceManager.getAudioDeviceSetup();
        ctx.sampleRate = setup.sampleRate > 0.0 ? setup.sampleRate : 48000.0;
        ctx.bufferSize = setup.bufferSize > 0 ? setup.bufferSize : 256;
        ctx.inputDeviceName = setup.inputDeviceName;
        ctx.outputDeviceName = setup.outputDeviceName;
        ctx.inputChannels = setup.inputChannels.countNumberOfSetBits();
        ctx.outputChannels = setup.outputChannels.countNumberOfSetBits();
        ctx.deviceTypeName = audioDeviceManager.getCurrentAudioDeviceType();
        if (auto* dev = audioDeviceManager.getCurrentAudioDevice())
        {
            ctx.sampleRate = dev->getCurrentSampleRate();
            ctx.bufferSize = dev->getCurrentBufferSizeSamples();
            ctx.inputChannels = dev->getActiveInputChannels().countNumberOfSetBits();
            ctx.outputChannels = dev->getActiveOutputChannels().countNumberOfSetBits();
        }
    }

    setStatus ("Preparing Ardour Studio pack (export stems, release interface)…");

    const auto result = jamstudio::audio::ArdourCompanion::openStudio (
        transportController.getStemMixer(),
        transportController.getFormatManager(),
        ctx,
        [this]
        {
            // Release hardware so Ardour can claim ALSA / JACK / PipeWire.
            audioDeviceManager.closeAudioDevice();
        });

    if (! result.ok)
    {
        setStatus (result.message.replaceCharacter ('\n', ' '));
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::WarningIcon,
            "Open Studio (Ardour)",
            result.message);
        // Try to restore audio
        audioInterfaceManager.rescanAndApply();
        return;
    }

    setStatus ("Ardour launched — interface handed off. Pack: " + result.sessionPackDir.getFileName()
               + "  |  Import Take when finished.");

    juce::AlertWindow::showMessageBoxAsync (
        juce::MessageBoxIconType::InfoIcon,
        "Ardour Studio ready",
        result.message + "\n\nTip: drag everything from the opened interop/ folder into Ardour after New Session.");
   #endif
}

void MainComponent::openExternalRecorder()
{
    // Explicit menu: "Open External Recorder (Audacity…)" — never auto-route to Ardour
    // so users can still pick Audacity when they want it. Open Studio uses openArdourStudio().

    auto app = jamstudio::audio::ExternalRecorder::getPreferred();
    // Prefer Audacity only for this explicit path
    {
        const auto apps = jamstudio::audio::ExternalRecorder::detectInstalled();
        for (const auto& a : apps)
            if (a.name.containsIgnoreCase ("Audacity"))
            {
                app = a;
                break;
            }
    }

    if (app.name.isEmpty())
    {
        setStatus ("No external recorder found. Install Audacity or use Open Studio (Ardour).");
        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "External recorder",
            "No Audacity (or other recorder) found.\n\n"
            "On Linux prefer: Transport → Open Studio (Ardour)…\n"
            "  sudo apt install ardour\n\n"
            "Or install Audacity for a lighter editor.");
        return;
    }

    juce::File bounceFile;
    juce::String error;

    auto& mixer = transportController.getStemMixer();
    if (mixer.getNumStems() > 0)
    {
        transportController.pause();
        const auto timestamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
        bounceFile = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                         .getChildFile ("JamStudio")
                         .getChildFile ("Recordings")
                         .getChildFile ("backing-bounce-" + timestamp + ".wav");

        setStatus ("Bouncing mix for " + app.name + "…");
        if (! jamstudio::audio::ExternalRecorder::bounceMixToWav (mixer, bounceFile, error))
        {
            setStatus ("Bounce failed: " + error + " — launching " + app.name + " empty.");
            bounceFile = juce::File();
        }
    }

    if (! jamstudio::audio::ExternalRecorder::launch (app, bounceFile, error))
    {
        setStatus (error);
        return;
    }

    if (bounceFile.existsAsFile())
        setStatus ("Opened " + app.name + " with bounced mix. Record with plugins, Export Audio as WAV, then Import Take.");
    else
        setStatus ("Opened " + app.name + ". Record your take, Export Audio as WAV, then Import Take into JamStudio.");
}

void MainComponent::importTakeFromFile()
{
    fileChooser = std::make_unique<juce::FileChooser> (
        "Import take from external recorder",
        juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
            .getChildFile ("JamStudio")
            .getChildFile ("Recordings"),
        "*.wav;*.aiff;*.flac;*.ogg;*.mp3;*.m4a");

    const auto flags = juce::FileBrowserComponent::openMode
                       | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        // Copy into JamStudio Recordings so the take is owned by the project area.
        const auto timestamp = juce::Time::getCurrentTime().formatted ("%Y%m%d-%H%M%S");
        auto dest = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                        .getChildFile ("JamStudio")
                        .getChildFile ("Recordings")
                        .getChildFile ("imported-" + timestamp + "-" + file.getFileName());
        dest.getParentDirectory().createDirectory();
        if (! file.copyFileTo (dest))
            dest = file;

        const auto takeResult = recordingTakeManager.addTake (dest);
        for (const auto& pruned : takeResult.prunedFiles)
            transportController.getStemMixer().removeStemByFile (pruned);

        loadRecordingAsStem (dest, takeResult.take.displayName);
        refreshRecordingTakesPanel();
        setStatus ("Imported " + takeResult.take.displayName + " from " + file.getFileName());
    });
}

void MainComponent::handleRecordingChoice (const jamstudio::ui::StartupWizard::RecordingChoice choice)
{
    using Choice = jamstudio::ui::StartupWizard::RecordingChoice;

    if (choice == Choice::emptySession)
    {
        hideStartupWizard();
        enterRecordingWorkspace();
        setStatus ("Empty recording session — arm input, press REC. Open a backing track anytime from File.");
        return;
    }

    if (choice == Choice::openProject)
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Open project as backing track",
            getProjectsDirectory(),
            "*.jamstudio");

        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();
            if (! file.existsAsFile())
                return;

            hideStartupWizard();
            enterRecordingWorkspace();
            loadProjectFile (file);
            setStatus ("Backing project loaded — press Play, then REC to capture your take.");
        });
        return;
    }

    // openBackingTrack
    fileChooser = std::make_unique<juce::FileChooser> (
        "Open backing track (audio)",
        juce::File::getSpecialLocation (juce::File::userMusicDirectory),
        "*.wav;*.mp3;*.flac;*.ogg;*.aiff;*.m4a");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        hideStartupWizard();
        enterRecordingWorkspace();
        currentSongFile = file;
        juce::Array<juce::File> stems;
        stems.add (file);
        loadStemsIntoMixer (stems);
        waveformDisplay.setSourceFile (file);
        detectTempoFromSong (file, false);
        setStatus ("Backing loaded: " + file.getFileName() + " — Play + REC to record.");
    });
}

void MainComponent::handlePracticeChoice (const jamstudio::ui::StartupWizard::PracticeChoice choice)
{
    if (choice == jamstudio::ui::StartupWizard::PracticeChoice::openProject)
    {
        fileChooser = std::make_unique<juce::FileChooser> (
            "Open practice project",
            getProjectsDirectory(),
            "*.jamstudio");

        const auto chooserFlags = juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles;

        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
        {
            const auto file = chooser.getResult();

            if (! file.existsAsFile())
                return;

            hideStartupWizard();
            loadProjectFile (file);
            currentMode = jamstudio::ui::StartupWizard::Mode::practice;
        });
        return;
    }

    // New practice from song file
    fileChooser = std::make_unique<juce::FileChooser> (
        "Choose a song to practice",
        juce::File {},
        "*.wav;*.mp3;*.flac;*.ogg;*.aiff");

    const auto chooserFlags = juce::FileBrowserComponent::openMode
                              | juce::FileBrowserComponent::canSelectFiles;

    fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();

        if (! file.existsAsFile())
            return;

        startPracticeFromSongFile (file);
    });
}

void MainComponent::startPracticeFromSongFile (const juce::File& songFile)
{
    if (practiceSetupPipeline == nullptr)
        return;

    if (practiceSetupPipeline->isRunning() || backgroundTaskActive)
    {
        setStatus ("A background job is already running.");
        return;
    }

    hideStartupWizard();
    currentMode = jamstudio::ui::StartupWizard::Mode::practice;
    currentSongFile = songFile;
    currentScoreFile = juce::File();
    currentLyricsFile = juce::File();
    currentProjectFile = juce::File();
    currentScore.clear();
    currentLyrics.clear();
    lyricsView.clear();
    notationView.clear();
    notationHeaderBar.setHasScore (false);
    recordingTakeManager.clear();

    transportController.stop();
    transportController.getStemMixer().clear();
    waveformDisplay.setSourceFile (songFile);
    rebuildStemLanes();
    rebuildMixerWindow();

    beginBackgroundTask ("Setting up practice project...",
                         [this]
                         {
                             if (practiceSetupPipeline != nullptr)
                                 practiceSetupPipeline->cancel();
                         });

    const auto jobGeneration = backgroundTaskGeneration.load();

    practiceSetupPipeline->start (songFile,
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            const auto pct = juce::roundToInt (progress * 100.0f);
            setStatus ((pct > 0 ? (juce::String (pct) + "% - ") : juce::String()) + message);
        },
        [this, jobGeneration] (PracticeSetupResult result)
        {
            if (jobGeneration != backgroundTaskGeneration.load())
                return;

            endBackgroundTask();
            applyPracticeSetupResult (std::move (result));
        });
}

void MainComponent::applyPracticeSetupResult (PracticeSetupResult result)
{
    if (result.cancelled)
    {
        setStatus ("Practice setup cancelled.");
        return;
    }

    if (! result.success)
    {
        setStatus (result.errorMessage.isNotEmpty() ? result.errorMessage
                                                    : "Practice setup failed.");
        return;
    }

    currentSongFile = result.songFile;

    if (! result.stemFiles.isEmpty())
        loadStemsIntoMixer (result.stemFiles);
    else if (result.songFile.existsAsFile())
        loadStemsIntoMixer ({ result.songFile });

    if (! result.score.isEmpty())
    {
        currentScoreFile = juce::File();
        applyScore (result.score, false);
    }

    if (! result.lyrics.isEmpty())
    {
        currentLyricsFile = juce::File();
        currentLyrics = std::move (result.lyrics);
        lyricsView.setLyrics (currentLyrics);
    }

    revealWorkspacePanels();
    autoSavePracticeProject (result.projectTitle);

    juce::StringArray summary;
    summary.add ("Project: " + result.projectTitle);
    summary.add (juce::String (result.stemFiles.size()) + " stems");

    if (result.scoreSource == "web")
        summary.add ("tabs (web)");
    else if (result.scoreSource == "ai")
        summary.add ("tabs (AI)");
    else
        summary.add ("no tabs");

    if (result.lyricsSource == "web")
        summary.add ("lyrics (web)");
    else if (result.lyricsSource == "ai")
        summary.add ("lyrics (AI)");
    else
        summary.add ("no lyrics");

    setStatus ("Practice ready - " + summary.joinIntoString (" - "));
}

void MainComponent::openSetListEditor (const bool stageShowBuilder)
{
    const auto mode = stageShowBuilder
                          ? jamstudio::ui::SetListEditorDialog::EditorMode::stageShowBuilder
                          : jamstudio::ui::SetListEditorDialog::EditorMode::performance;

    jamstudio::ui::SetListEditorDialog::show (
        this,
        [this] (jamstudio::performance::SetList list)
        {
            startPerformanceMode (std::move (list));
        },
        mode);
}

void MainComponent::startPerformanceMode (jamstudio::performance::SetList list)
{
    if (list.songs.isEmpty())
    {
        setStatus ("Set list is empty.");
        return;
    }

    performanceSetList = std::move (list);
    performanceActive = true;
    performanceSongIndex = -1;
    performanceWaitingForTrigger = true;
    performanceWasPlaying = false;
    currentMode = jamstudio::ui::StartupWizard::Mode::performance;

    hideStartupWizard();
    revealWorkspacePanels();
    performanceBar.setVisible (true);
    updatePerformanceBar();
    performanceBar.setWaitingForTrigger (true);
    performanceBar.setPhaseMessage ("Press NEXT / START or foot pedal (MIDI: Next Song) for song 1");

    // Do not auto-open video screens — user assigns displays from Stage FX Controller.
    syncVideoOutputs();
    stageFxController.syncVideoRoutingUi();

    setStatus ("Performance ready - " + performanceSetList.name + " ("
               + juce::String (performanceSetList.songs.size()) + " songs). "
               + "Open Karaoke / Stage FX from Stage FX Controller or Performance menu.");
    updateMixerPerformanceContext();
    resized();
}

void MainComponent::stopPerformanceMode()
{
    performanceActive = false;
    performanceWaitingForTrigger = false;
    performanceSongIndex = -1;
    performanceWasPlaying = false;
    performanceBar.setVisible (false);
    // Leave video outputs as the user left them (do not force-close on stop).
    transportController.stop();
    stageFxController.syncVideoRoutingUi();
    updateMixerPerformanceContext();
    setStatus ("Performance mode stopped.");
    resized();
}

void MainComponent::openKaraokeOutput()
{
    karaokeOutput.showOnDisplay (karaokeDisplayIndex);
    syncVideoOutputs();
    stageFxController.syncVideoRoutingUi();
    setStatus ("Karaoke output on " + jamstudio::ui::VideoOutputHelpers::getDisplayLabel (karaokeDisplayIndex));
}

void MainComponent::openStageFxOutput()
{
    stageFxOutput.showOnDisplay (stageFxDisplayIndex);
    syncVideoOutputs();
    stageFxController.syncVideoRoutingUi();
    setStatus ("Stage FX output on " + jamstudio::ui::VideoOutputHelpers::getDisplayLabel (stageFxDisplayIndex));
}

void MainComponent::cycleKaraokeDisplay()
{
    const auto n = jamstudio::ui::VideoOutputHelpers::getNumDisplays();
    karaokeDisplayIndex = (karaokeDisplayIndex + 1) % n;
    if (karaokeOutput.isOutputVisible())
        openKaraokeOutput();
    else
        stageFxController.syncVideoRoutingUi();
}

void MainComponent::cycleStageFxDisplay()
{
    const auto n = jamstudio::ui::VideoOutputHelpers::getNumDisplays();
    stageFxDisplayIndex = (stageFxDisplayIndex + 1) % n;
    if (stageFxOutput.isOutputVisible())
        openStageFxOutput();
    else
        stageFxController.syncVideoRoutingUi();
}

void MainComponent::syncVideoOutputs()
{
    juce::String title = currentSongFile.existsAsFile()
                             ? currentSongFile.getFileNameWithoutExtension()
                             : currentLyrics.getTitle();

    if (juce::isPositiveAndBelow (performanceSongIndex, performanceSetList.songs.size()))
        title = performanceSetList.songs.getReference (performanceSongIndex).displayName;

    // Stage FX controller previews always stay in sync (even when full-screen outs closed).
    stageFxController.setSongTitle (title);
    stageFxController.setLyrics (currentLyrics);

    if (karaokeOutput.isOutputVisible())
    {
        karaokeOutput.setSongTitle (title);
        karaokeOutput.setLyrics (currentLyrics);
    }

    if (stageFxOutput.isOutputVisible())
    {
        stageFxOutput.setSongTitle (title);
        stageFxOutput.setSetInfo (performanceSetList.name,
                                  performanceSongIndex,
                                  performanceSetList.songs.size());
        stageFxOutput.setWaitingBetweenSongs (performanceWaitingForTrigger);
        stageFxOutput.setExternalEnergy (transportController.getStageMedia().getMeterLevel());
    }
}

void MainComponent::updatePerformanceBar()
{
    juce::String title;

    if (juce::isPositiveAndBelow (performanceSongIndex, performanceSetList.songs.size()))
        title = performanceSetList.songs.getReference (performanceSongIndex).displayName;

    performanceBar.setSetListInfo (performanceSetList.name,
                                   performanceSongIndex,
                                   performanceSetList.songs.size(),
                                   title);
    syncVideoOutputs();
}

void MainComponent::performanceTriggerNext()
{
    if (! performanceActive)
    {
        openSetListEditor();
        return;
    }

    // Between songs or before first: advance and play.
    if (performanceWaitingForTrigger || performanceSongIndex < 0)
    {
        const auto next = performanceSongIndex + 1;

        if (next >= performanceSetList.songs.size())
        {
            performanceBar.setPhaseMessage ("Set complete - nice show!");
            performanceBar.setWaitingForTrigger (false);
            setStatus ("Set list finished.");
            transportController.stop();
            return;
        }

        loadPerformanceSong (next, true);
        return;
    }

    // During a song: skip to end / prepare next (manual advance)
    transportController.pause();
    onPerformanceSongEnded();
}

void MainComponent::onPerformanceSongEnded()
{
    if (! performanceActive)
        return;

    transportController.pause();
    performanceWasPlaying = false;
    performanceWaitingForTrigger = true;
    updatePerformanceBar();

    const auto next = performanceSongIndex + 1;

    if (next >= performanceSetList.songs.size())
    {
        performanceBar.setWaitingForTrigger (false);
        performanceBar.setPhaseMessage ("Set complete!");
        setStatus ("Last song finished.");
        return;
    }

    const auto& nextSong = performanceSetList.songs.getReference (next);
    performanceBar.setWaitingForTrigger (true);
    performanceBar.setPhaseMessage ("Song ended - press foot pedal / NEXT for: " + nextSong.displayName);
    syncVideoOutputs();
    setStatus ("Paused between songs. Pedal to start: " + nextSong.displayName);
}

void MainComponent::loadPerformanceSong (const int index, const bool autoPlay)
{
    if (! juce::isPositiveAndBelow (index, performanceSetList.songs.size()))
        return;

    const auto song = performanceSetList.songs.getReference (index);
    const auto projectFile = song.projectFile();

    if (! projectFile.existsAsFile())
    {
        setStatus ("Missing project: " + song.projectPath);
        performanceBar.setPhaseMessage ("Missing file - skip with NEXT");
        performanceWaitingForTrigger = true;
        performanceSongIndex = index; // stay on broken slot so next advances
        updatePerformanceBar();
        return;
    }

    loadProjectFile (projectFile);
    performanceSongIndex = index;
    performanceWaitingForTrigger = false;
    updateMixerPerformanceContext();
    applyPerformanceStemPrefsForCurrentSong();
    loadSongStageMedia (song);

    if (song.showTabs)
    {
        notationPanelVisible = true;
        preferLeadTabPart (song.preferredPartHint);
        setNotationDisplayMode (jamstudio::notation::NotationMode::tab);
    }

    if (song.showLyrics)
        lyricsPanelVisible = true;

    applyPanelVisibility();
    updatePerformanceBar();
    performanceBar.setWaitingForTrigger (false);
    performanceBar.setPhaseMessage (autoPlay ? "Playing..." : "Loaded - press play or pedal");

    if (autoPlay)
    {
        transportController.setPosition (0.0);
        transportController.play();
        performanceWasPlaying = true;
    }

    syncVideoOutputs();
    setStatus ("Now: " + song.displayName
               + "  (" + juce::String (index + 1) + "/"
               + juce::String (performanceSetList.songs.size()) + ")"
               + (song.hasStageMedia() ? "  [stage media]" : juce::String()));
}

void MainComponent::loadSongStageMedia (const jamstudio::performance::SetListSong& song)
{
    auto& stage = transportController.getStageMedia();
    stage.stop();
    stage.clear();

    if (! song.hasStageMedia())
        return;

    const auto setFile = performanceSetList.sourceFile();
    juce::String error;

    if (song.stageMediaKind == jamstudio::performance::StageMediaKind::video)
    {
        const auto mediaFile = song.resolveStageMediaFile (setFile);

        if (! mediaFile.existsAsFile())
        {
            setStatus ("Stage video missing: " + song.stageMediaPath);
            return;
        }

        if (! stage.loadFile (mediaFile, error))
        {
            setStatus ("Stage video: " + error);
            return;
        }
    }
    else if (song.stageMediaKind == jamstudio::performance::StageMediaKind::slideshow)
    {
        const auto slides = song.resolveSlideFiles (setFile);

        if (slides.isEmpty())
        {
            setStatus ("Stage slideshow has no images.");
            return;
        }

        if (! stage.loadSlideshow (slides, song.stageSlideSeconds, error))
        {
            setStatus ("Stage slideshow: " + error);
            return;
        }
    }

    if (song.stageMediaAutoPlay)
        stage.play();

    mixerWindow.syncVideoSoundSlider();
}

void MainComponent::applyPerformanceStemPrefsForCurrentSong()
{
    if (! juce::isPositiveAndBelow (performanceSongIndex, performanceSetList.songs.size()))
        return;

    const auto& song = performanceSetList.songs.getReference (performanceSongIndex);
    const auto& prefs = song.stemPrefs.isEmpty() ? performanceSetList.defaultStemPrefs
                                                 : song.stemPrefs;

    jamstudio::performance::applyStemPrefs (transportController.getStemMixer(), prefs);
    rebuildMixerWindow();
    rebuildStemLanes();
    refreshMixerUiFromMidi();
    updateMixerPerformanceContext();
}

void MainComponent::updateMixerPerformanceContext()
{
    juce::String name;
    if (juce::isPositiveAndBelow (performanceSongIndex, performanceSetList.songs.size()))
        name = performanceSetList.songs.getReference (performanceSongIndex).displayName;

    mixerWindow.setPerformanceMixContext (performanceActive,
                                          performanceSongIndex,
                                          performanceSetList.songs.size(),
                                          name);
}

void MainComponent::saveCurrentMixerToSetlistTrack()
{
    if (! performanceActive)
    {
        setStatus ("Load a Performance set list first, then start a song.");
        return;
    }

    if (! juce::isPositiveAndBelow (performanceSongIndex, performanceSetList.songs.size()))
    {
        setStatus ("No current set track — press Next/Start to load a song, then save mix.");
        return;
    }

    auto& song = performanceSetList.songs.getReference (performanceSongIndex);
    song.stemPrefs = jamstudio::performance::captureStemPrefs (transportController.getStemMixer());

    auto file = performanceSetList.sourceFile();
    if (! file.existsAsFile())
    {
        const auto safeName = juce::File::createLegalFileName (
            performanceSetList.name.isNotEmpty() ? performanceSetList.name : "My Set");
        file = jamstudio::performance::SetListManager::getSetListsDirectory()
                   .getChildFile (safeName + ".setlist");
    }

    if (! jamstudio::performance::SetListManager::saveSetList (file, performanceSetList))
    {
        setStatus ("Could not write set list file.");
        return;
    }

    auto defaultCopy = performanceSetList;
    juce::ignoreUnused (jamstudio::performance::SetListManager::saveSetList (
        jamstudio::performance::SetListManager::defaultSetListFile(), defaultCopy));

    setStatus ("Saved mix for set track " + juce::String (performanceSongIndex + 1) + ": "
               + song.displayName + " (" + juce::String (song.stemPrefs.size())
               + " stems) → " + file.getFileName());
    updateMixerPerformanceContext();
}

void MainComponent::preferLeadTabPart (const juce::String& partHint)
{
    if (currentScore.isEmpty() || partHint.isEmpty())
        return;

    const auto names = currentScore.getPartNames();

    for (int i = 0; i < names.size(); ++i)
    {
        if (names[i].containsIgnoreCase (partHint))
        {
            setActiveScorePart (i);
            return;
        }
    }

    // Prefer any guitar-ish part if exact hint missing
    for (int i = 0; i < names.size(); ++i)
    {
        if (names[i].containsIgnoreCase ("guitar") || names[i].containsIgnoreCase ("lead"))
        {
            setActiveScorePart (i);
            return;
        }
    }
}

void MainComponent::autoSavePracticeProject (const juce::String& projectTitle)
{
    auto title = projectTitle.trim();

    if (title.isEmpty())
        title = currentSongFile.existsAsFile() ? currentSongFile.getFileNameWithoutExtension()
                                               : "Untitled";

    const auto projectFile = getProjectsDirectory().getChildFile (title + ".jamstudio");

    auto data = jamstudio::project::ProjectManager::captureState (
        currentSongFile, currentScoreFile, currentLyricsFile,
        currentScore, currentLyrics, transportController, transportBar);

    if (jamstudio::project::ProjectManager::saveProject (projectFile, data))
    {
        currentProjectFile = projectFile;
        recentProjects.add (projectFile);

        // Point the mixer at permanent copies so playback survives /tmp cleanup.
        juce::Array<juce::File> permanentStems;

        for (const auto& stem : data.stems)
        {
            const juce::File f (stem.filePath);

            if (f.existsAsFile())
                permanentStems.add (f);
        }

        if (! permanentStems.isEmpty())
        {
            loadStemsIntoMixer (permanentStems);

            auto& mixer = transportController.getStemMixer();

            for (int i = 0; i < mixer.getNumStems() && i < data.stems.size(); ++i)
            {
                const auto& s = data.stems.getReference (i);
                mixer.setStemMuted (i, s.muted);
                mixer.setStemSolo (i, s.solo);
                mixer.setStemVolume (i, s.volume);

                if (s.name.isNotEmpty())
                    mixer.setStemName (i, s.name);
            }
        }
    }
    else
    {
        setStatus ("Practice ready, but failed to save project (stem files may still be under a temp path).");
    }
}

} // namespace jamstudio::app