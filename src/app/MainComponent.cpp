#include "MainComponent.h"

#include "../audio/StemType.h"
#include "../audio/TempoDetector.h"
#include "../ui/AiToolsSetupDialog.h"
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
      transportController (deviceManager),
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
      fullPageTabsWindow (transportController)
{
    setSize (1280, 900);
    refreshTheme();

    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    statusLabel.setColour (juce::Label::backgroundColourId,
                           jamstudio::ui::JamStudioTheme::getColours().statusBackground);
    setStatus ("Ready. Open a song. Use toolbar View tab to show/hide Lyrics, Tabs, Stems, Mixer.");

    addAndMakeVisible (toolbarTabs);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (separationProgress);
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

    // Panels available immediately with empty states.
    rebuildMixerWindow();
    mixerWindow.setVisibilityChangedCallback ([this] (bool)
    {
        updatePanelToggleStates();
    });
    updatePanelToggleStates();
    applyPanelVisibility();
}

MainComponent::~MainComponent()
{
    fileChooser.reset();
    mixerWindow.setVisible (false);
    fullPageTabsWindow.setVisible (false);
    audioRecorder.stopRecording();
    audioDeviceManager.removeAudioCallback (&audioRecorder);
    demucsSeparator.cancel();
    whisperTranscriber.cancel();
    basicPitchTranscriber.cancel();
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
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (jamstudio::ui::JamStudioTheme::getColours().windowBackground);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    toolbarTabs.setBounds (bounds.removeFromTop (54));
    bounds.removeFromTop (4);

    if (separationProgress.isVisible())
    {
        separationProgress.setBounds (bounds.removeFromTop (42));
        bounds.removeFromTop (4);
    }

    statusLabel.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (6);

    // Bottom dock: main waveform → transport → stem lanes
    const int stemLaneCount = stemContainer.getNumChildComponents();
    const int stemLaneHeight = 40;
    const int stemsBlockHeight = stemsPanelVisible
        ? (18 + juce::jlimit (56, 220, juce::jmax (1, stemLaneCount) * stemLaneHeight + 8))
        : 0;
    const int waveHeight = 96;
    const int transportHeight = 48;
    const int bottomStackHeight = waveHeight + 4 + transportHeight
                                  + (stemsPanelVisible ? 8 + stemsBlockHeight : 0);

    auto bottom = bounds.removeFromBottom (bottomStackHeight);

    // Top of bottom stack: main waveform
    waveformDisplay.setBounds (bottom.removeFromTop (waveHeight));
    bottom.removeFromTop (4);

    // Directly under waveform: transport deck
    transportBar.setBounds (bottom.removeFromTop (transportHeight));

    if (stemsPanelVisible)
    {
        bottom.removeFromTop (8);
        auto stemsArea = bottom;
        stemsSectionLabel.setBounds (stemsArea.removeFromTop (16));
        stemViewport.setBounds (stemsArea);
        layoutStemLanes();
    }

    // Upper / middle: lyrics then tabs
    if (lyricsPanelVisible)
    {
        const auto lyricsHeight = juce::jlimit (100, 170, bounds.getHeight() / 4);
        lyricsSectionLabel.setBounds (bounds.removeFromTop (16));
        lyricsView.setBounds (bounds.removeFromTop (lyricsHeight));
        bounds.removeFromTop (6);
    }

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
        notationView.setSize (juce::jmax (notationViewport.getWidth(), notationView.getWidth()),
                              juce::jmax (notationViewport.getHeight(), notationView.getContentHeight()));
    }
}

void MainComponent::layoutStemLanes()
{
    constexpr int laneHeight = 40;
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
    return { "File", "Project", "View", "Stems", "Notation", "Lyrics", "Transport", "Help" };
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
        menu.addItem (toggleLyricsPanelCmd, "Show Lyrics Panel", true, lyricsPanelVisible);
    }
    else if (menuName == "Transport")
    {
        menu.addItem (detectTempoCmd, "Detect Tempo", true, false);
        menu.addItem (recordCmd, "Record / Stop", true, false);
    }
    else if (menuName == "Help")
    {
        menu.addItem (aiToolsCmd, "AI Tools Setup...", true, false);
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
        case recordCmd: toggleRecording(); break;
        case aiToolsCmd: showAiToolsSetup(); break;
        case toggleLyricsPanelCmd: toggleLyricsPanel(); break;
        case toggleNotationPanelCmd: toggleNotationPanel(); break;
        case toggleStemsPanelCmd: toggleStemsPanel(); break;
        case toggleMixerWindowCmd: toggleMixerWindow(); break;
        case aboutCmd:
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "JamStudio",
                                                    "JamStudio v0.9.6\n"
                                                    "Guitar practice workstation with stems, tabs, and lyrics.\n\n"
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
    if (source == &transportController.getStemMixer())
    {
        rebuildStemLanes();
        rebuildMixerWindow();
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

        const auto data = jamstudio::project::ProjectManager::captureState (
            currentSongFile, currentScoreFile, currentLyricsFile,
            currentScore, currentLyrics, transportController, transportBar);

        if (jamstudio::project::ProjectManager::saveProject (file, data))
        {
            currentProjectFile = file;
            recentProjects.add (file);
            setStatus ("Project saved: " + file.getFileName());
        }
        else
        {
            setStatus ("Failed to save project.");
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

    if (! jamstudio::project::ProjectManager::applyState (data, transportController, transportBar,
                                                        currentScore, currentLyrics,
                                                        currentSongFile, currentScoreFile, currentLyricsFile, error))
    {
        setStatus ("Failed to restore project: " + error);
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

    setStatus ("Project loaded: " + file.getFileName()
               + (loadedParts.isEmpty() ? "" : " (" + loadedParts.joinIntoString (" + ") + ")"));
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
            lyricsPanelVisible = true;
            applyPanelVisibility();
            toolbarTabs.setActiveTab (jamstudio::ui::ToolbarTabs::Tab::lyrics);

            const auto wordInfo = currentLyrics.hasWordTimings() ? " (word-level if present)" : " (line-synced LRC)";
            setStatus ("Online lyrics applied: " + juce::String (currentLyrics.getNumLines())
                       + " lines" + wordInfo + " — " + currentLyrics.getTitle());
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
                       + " BPM — Lyrics / Tabs / Stems / Mixer ready.");
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
        setStatus ("Tabs panel open — import MusicXML or run AI Tab.");
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
        setStatus ("Tabs panel open — import MusicXML or run AI Tab.");
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
        setStatus ("Tabs panel open — import MusicXML or run AI Tab.");
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
    setStatus ("Full page tabs open — use Print or Export PNG for a printable copy.");
}

void MainComponent::setActiveScorePart (const int partIndex)
{
    if (currentScore.isEmpty())
        return;

    // Keep the user's Tab/Sheet choice — don't force guitar-only tab mode per part.
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
    // Prefer guitar for AI tab generation — this app is for guitar practice.
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

    toolbarTabs.setToolsEnabled (false);
    separationProgress.setVisible (true);
    separationProgress.setProgress (0.0f, message);
    separationProgress.setCancelCallback ([this, generation, onCancel = std::move (onCancel)]
    {
        // Only act if this is still the active job (avoids stale callbacks).
        if (generation != backgroundTaskGeneration.load())
            return;

        if (onCancel)
            onCancel();

        // End UI immediately — do not quit the app. Completion callback may also fire later.
        endBackgroundTask();
        setStatus ("Cancelled.");
    });
    setStatus (message);
    resized();
}

void MainComponent::endBackgroundTask()
{
    if (! backgroundTaskActive)
        return;

    backgroundTaskActive = false;
    toolbarTabs.setToolsEnabled (true);
    separationProgress.reset();
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
                // Cancel is a normal outcome — keep the app open and show status only.
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

            separationProgress.setProgress (progress, message);
            setStatus (message);
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

            separationProgress.setProgress (progress, message);
            setStatus (message);
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
                           ? "Separation complete — Guitar stem ready. Solo Guitar to learn the part, "
                             "or mute Guitar to play along with the band."
                           : "Separation complete. " + juce::String (result.stemFiles.size())
                                 + " stems loaded.");
        },
        [this, jobGeneration] (const float progress, const juce::String& message)
        {
            if (jobGeneration != backgroundTaskGeneration.load() || ! backgroundTaskActive)
                return;

            separationProgress.setProgress (progress, message);
            setStatus (message);
        });
}

void MainComponent::toggleRecording()
{
    if (audioRecorder.isRecording())
    {
        const auto savedFile = audioRecorder.stopRecording();
        toolbarTabs.setRecordingActive (false);

        if (! savedFile.existsAsFile())
        {
            setStatus ("Recording stopped, but no audio was captured.");
            return;
        }

        const auto exportResult = recordingExporter.exportRecording (savedFile);
        const auto takeResult = recordingTakeManager.addTake (savedFile);

        for (const auto& pruned : takeResult.prunedFiles)
            transportController.getStemMixer().removeStemByFile (pruned);

        loadRecordingAsStem (savedFile, takeResult.take.displayName);

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
        setStatus ("Recording... play along with the backing and press Stop when finished.");
    }
    else
    {
        setStatus ("Could not start recording. Check that an audio input device is available.");
    }
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
                         [this] (const int index, const bool muted, const bool solo, const float volume)
                         {
                             auto& stemMixer = transportController.getStemMixer();
                             stemMixer.setStemMuted (index, muted);
                             stemMixer.setStemSolo (index, solo);
                             stemMixer.setStemVolume (index, volume);
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

} // namespace jamstudio::app