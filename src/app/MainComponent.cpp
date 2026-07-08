#include "MainComponent.h"

#include "../audio/StemType.h"
#include "../audio/TempoDetector.h"
#include "../ui/AiToolsSetupDialog.h"
#include "../ui/JamStudioLookAndFeel.h"
#include "../notation/LrcParser.h"
#include "../notation/MusicXmlParser.h"
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
          [this] { importScore(); },
          [this] { transcribeTab(); },
          [this] { importLyrics(); },
          [this] { transcribeLyrics(); },
          [this] { toggleRecording(); }
      }),
      waveformDisplay (transportController.getFormatManager(), thumbnailCache, transportController),
      lyricsView (transportController),
      notationView (transportController),
      transportBar (transportController)
{
    setSize (1200, 860);
    refreshTheme();

    juce::Desktop::getInstance().addDarkModeSettingListener (this);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    juce::StringArray readyHints;
    readyHints.add ("Open a song from File or the Project toolbar");

    if (demucsSeparator.isAvailable())
        readyHints.add ("separate stems");

    if (whisperTranscriber.isAvailable())
        readyHints.add ("AI lyrics");

    if (basicPitchTranscriber.isAvailable())
        readyHints.add ("AI tab");

    setStatus ("Ready. " + readyHints.joinIntoString (", ") + ".");
    addAndMakeVisible (toolbarTabs);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (separationProgress);
    addAndMakeVisible (waveformDisplay);
    addAndMakeVisible (lyricsView);

    notationViewport.setViewedComponent (&notationView, false);
    notationViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (notationViewport);

    addAndMakeVisible (transportBar);

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
}

MainComponent::~MainComponent()
{
    fileChooser.reset();
    audioRecorder.stopRecording();
    audioDeviceManager.removeAudioCallback (&audioRecorder);
    demucsSeparator.cancel();
    whisperTranscriber.cancel();
    basicPitchTranscriber.cancel();
    transportController.getStemMixer().removeChangeListener (this);
    transportController.removeChangeListener (this);
    juce::Desktop::getInstance().removeDarkModeSettingListener (this);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (jamstudio::ui::JamStudioTheme::getColours().windowBackground);
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (6);

    toolbarTabs.setBounds (bounds.removeFromTop (58));
    bounds.removeFromTop (4);
    statusLabel.setBounds (bounds.removeFromTop (22));

    if (separationProgress.isVisible())
    {
        bounds.removeFromTop (2);
        separationProgress.setBounds (bounds.removeFromTop (44));
    }

    bounds.removeFromTop (6);
    waveformDisplay.setBounds (bounds.removeFromTop (128));
    bounds.removeFromTop (4);
    transportBar.setBounds (bounds.removeFromTop (44));
    bounds.removeFromTop (6);
    lyricsView.setBounds (bounds.removeFromTop (120));
    bounds.removeFromTop (6);
    notationViewport.setBounds (bounds.removeFromTop (juce::jmax (140, bounds.getHeight() / 3)));
    bounds.removeFromTop (6);

    stemViewport.setBounds (bounds);

    constexpr int stripHeight = 48;
    const auto containerWidth = juce::jmax (stemViewport.getMaximumVisibleWidth(), stemViewport.getWidth());
    stemContainer.setSize (containerWidth, stemContainer.getNumChildComponents() * stripHeight + 4);

    auto stripBounds = stemContainer.getLocalBounds().reduced (2);
    int y = stripBounds.getY();

    for (int i = 0; i < stemContainer.getNumChildComponents(); ++i)
    {
        if (auto* strip = stemContainer.getChildComponent (i))
        {
            strip->setBounds (stripBounds.getX(), y, stripBounds.getWidth(), stripHeight - 2);
            y += stripHeight;
        }
    }
}

juce::StringArray MainComponent::buildMenuBarNames()
{
    return { "File", "Project", "Stems", "Notation", "Lyrics", "Transport", "Help" };
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
    else if (menuName == "Stems")
    {
        menu.addItem (separateStemsCmd, "Separate Stems", demucsSeparator.isAvailable(), false);
    }
    else if (menuName == "Notation")
    {
        menu.addItem (importScoreCmd, "Import MusicXML...", true, false);
        menu.addItem (aiTabCmd, "AI Tab Transcription", basicPitchTranscriber.isAvailable(), false);
    }
    else if (menuName == "Lyrics")
    {
        menu.addItem (importLyricsCmd, "Import LRC...", true, false);
        menu.addItem (aiLyricsCmd, "AI Vocal Transcription", whisperTranscriber.isAvailable(), false);
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
        case importScoreCmd: importScore(); break;
        case aiTabCmd: transcribeTab(); break;
        case importLyricsCmd: importLyrics(); break;
        case aiLyricsCmd: transcribeLyrics(); break;
        case detectTempoCmd:
            if (currentSongFile.existsAsFile())
                detectTempoFromSong (currentSongFile, true);
            else
                setStatus ("Open a song before detecting tempo.");
            break;
        case recordCmd: toggleRecording(); break;
        case aiToolsCmd: showAiToolsSetup(); break;
        case aboutCmd:
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "JamStudio",
                                                    "JamStudio v0.9.1\nStem separation, synced notation, lyrics, and recording.");
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
        rebuildStemStrips();

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
        notationView.setScore (currentScore);
    else
        notationView.clear();

    if (! currentLyrics.isEmpty())
        lyricsView.setLyrics (currentLyrics);
    else
        lyricsView.clear();

    rebuildStemStrips();
    resized();
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
        setStatus ("Lyrics loaded: " + file.getFileName() + " (" + juce::String (currentLyrics.getNumLines()) + " lines)");
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
        transportController.stop();
        waveformDisplay.setSourceFile (file);

        if (transportController.getStemMixer().loadStems ({ file }))
        {
            detectTempoFromSong (file, false);
            setStatus ("Loaded: " + file.getFileName()
                       + " @ " + juce::String (static_cast<int> (transportBar.getBpm())) + " BPM");
            rebuildStemStrips();
            resized();
        }
        else
        {
            setStatus ("Failed to load: " + file.getFileName());
        }
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

        jamstudio::notation::Score importedScore;
        juce::String error;

        if (! jamstudio::notation::MusicXmlParser::parseFile (file, importedScore, error))
        {
            setStatus ("Score import failed: " + error);
            return;
        }

        currentScoreFile = file;
        currentScore = importedScore;
        notationView.setScore (currentScore);
        transportController.getMetronome().setBpm (currentScore.getTempo());
        auto message = "Score loaded: " + file.getFileName() + " (" + juce::String (currentScore.getNumMeasures()) + " measures)";

        if (currentScore.hasLyrics())
            message += " with synced lyrics";

        setStatus (message);
        resized();
    });
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
    for (const auto stemType : { jamstudio::audio::StemType::other,
                                 jamstudio::audio::StemType::bass,
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
    toolbarTabs.setToolsEnabled (false);
    separationProgress.setVisible (true);
    separationProgress.setProgress (0.0f, message);
    separationProgress.setCancelCallback ([onCancel = std::move (onCancel)] { if (onCancel) onCancel(); });
    setStatus (message);
    resized();
}

void MainComponent::endBackgroundTask()
{
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
                         [this] { whisperTranscriber.cancel(); endBackgroundTask(); setStatus ("Transcription cancelled."); });

    whisperTranscriber.transcribeAsync (vocalsFile,
        [this] (const jamstudio::ai::TranscriptionResult& result)
        {
            endBackgroundTask();

            if (! result.success)
            {
                setStatus (result.errorMessage);
                return;
            }

            currentLyricsFile = juce::File();
            currentLyrics = result.lyrics;
            lyricsView.setLyrics (currentLyrics);

            const auto wordInfo = currentLyrics.hasWordTimings() ? " with word-level timing" : "";
            setStatus ("AI lyrics ready: " + juce::String (currentLyrics.getNumLines()) + " lines" + wordInfo + ".");
        },
        [this] (const float progress, const juce::String& message)
        {
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
                         [this] { basicPitchTranscriber.cancel(); endBackgroundTask(); setStatus ("Transcription cancelled."); });

    basicPitchTranscriber.transcribeAsync (melodicFile,
        [this] (const jamstudio::ai::PitchTranscriptionResult& result)
        {
            endBackgroundTask();

            if (! result.success)
            {
                setStatus (result.errorMessage);
                return;
            }

            currentScoreFile = juce::File();
            currentScore = result.score;
            notationView.setScore (currentScore);
            transportController.getMetronome().setBpm (currentScore.getTempo());
            setStatus ("AI tab ready: " + juce::String (currentScore.getNumMeasures()) + " measures.");
            resized();
        },
        [this] (const float progress, const juce::String& message)
        {
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

    beginBackgroundTask ("Separating stems...",
                         [this] { demucsSeparator.cancel(); endBackgroundTask(); setStatus ("Separation cancelled."); });

    demucsSeparator.separateAsync (currentSongFile,
        [this] (const jamstudio::ai::SeparationResult& result)
        {
            endBackgroundTask();

            if (! result.success)
            {
                setStatus (result.errorMessage);
                return;
            }

            loadStemsIntoMixer (result.stemFiles);
            setStatus ("Separation complete. " + juce::String (result.stemFiles.size()) + " stems loaded.");
        },
        [this] (const float progress, const juce::String& message)
        {
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
        loadRecordingAsStem (savedFile);

        juce::StringArray exportedPaths;
        exportedPaths.add (savedFile.getFullPathName());

        if (exportResult.oggFile.existsAsFile())
            exportedPaths.add (exportResult.oggFile.getFullPathName());

        if (exportResult.mp3File.existsAsFile())
            exportedPaths.add (exportResult.mp3File.getFullPathName());

        setStatus ("Recording added as stem. Exported: " + exportedPaths.joinIntoString (", "));
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

void MainComponent::loadRecordingAsStem (const juce::File& recordingFile)
{
    if (! recordingFile.existsAsFile())
        return;

    if (transportController.getStemMixer().loadStem (recordingFile))
        rebuildStemStrips();
}

void MainComponent::loadStemsIntoMixer (const juce::Array<juce::File>& stemFiles)
{
    transportController.stop();
    transportController.getStemMixer().loadStems (stemFiles);

    if (stemFiles.size() > 0)
    {
        waveformDisplay.setSourceFile (stemFiles.getReference (0));

        if (currentSongFile.existsAsFile())
            detectTempoFromSong (currentSongFile, false);
        else
            detectTempoFromSong (stemFiles.getReference (0), false);
    }

    rebuildStemStrips();
    resized();
}

void MainComponent::rebuildStemStrips()
{
    stemContainer.removeAllChildren();

    auto& mixer = transportController.getStemMixer();

    for (int i = 0; i < mixer.getNumStems(); ++i)
    {
        if (const auto* stem = mixer.getStem (i))
        {
            auto strip = std::make_unique<jamstudio::ui::StemStrip> (
                i, *stem,
                transportController.getFormatManager(),
                thumbnailCache,
                transportController,
                [this] (const int index, const bool muted, const bool solo, const float volume)
                {
                    auto& stemMixer = transportController.getStemMixer();
                    stemMixer.setStemMuted (index, muted);
                    stemMixer.setStemSolo (index, solo);
                    stemMixer.setStemVolume (index, volume);
                });

            stemContainer.addAndMakeVisible (strip.release());
        }
    }

    resized();
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