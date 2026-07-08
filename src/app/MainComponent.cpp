#include "MainComponent.h"

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
      waveformDisplay (transportController.getFormatManager(), thumbnailCache, transportController),
      lyricsView (transportController),
      notationView (transportController),
      transportBar (transportController)
{
    setSize (1100, 820);

    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    openSongButton.onClick = [this] { openSong(); };
    addAndMakeVisible (openSongButton);

    saveProjectButton.onClick = [this] { saveProject(); };
    addAndMakeVisible (saveProjectButton);

    loadProjectButton.onClick = [this] { loadProject(); };
    addAndMakeVisible (loadProjectButton);

    recentProjectsButton.onClick = [this] { showRecentProjectsMenu(); };
    addAndMakeVisible (recentProjectsButton);

    separateButton.onClick = [this] { separateStems(); };
    addAndMakeVisible (separateButton);

    importScoreButton.onClick = [this] { importScore(); };
    addAndMakeVisible (importScoreButton);

    importLyricsButton.onClick = [this] { importLyrics(); };
    addAndMakeVisible (importLyricsButton);

    recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8b2f2f));
    recordButton.onClick = [this] { toggleRecording(); };
    addAndMakeVisible (recordButton);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    setStatus (demucsSeparator.isAvailable()
                   ? "Ready. Open a song, import a score, or separate stems with Demucs."
                   : "Ready. Open a song file. Install Demucs (pip install demucs) for stem separation.");
    addAndMakeVisible (statusLabel);

    addAndMakeVisible (separationProgress);
    addAndMakeVisible (waveformDisplay);
    addAndMakeVisible (lyricsView);

    notationViewport.setViewedComponent (&notationView, false);
    notationViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (notationViewport);

    addAndMakeVisible (transportBar);

    stemViewport.setViewedComponent (&stemContainer, false);
    stemViewport.setScrollBarsShown (false, true);
    addAndMakeVisible (stemViewport);

    audioDeviceManager.addAudioCallback (&audioRecorder);

    transportController.getStemMixer().addChangeListener (this);
    transportController.addChangeListener (this);
}

MainComponent::~MainComponent()
{
    audioRecorder.stopRecording();
    audioDeviceManager.removeAudioCallback (&audioRecorder);
    demucsSeparator.cancel();
    transportController.getStemMixer().removeChangeListener (this);
    transportController.removeChangeListener (this);
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff121212));
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced (12);

    auto header = bounds.removeFromTop (40);
    titleLabel.setBounds (header.removeFromLeft (140));
    recordButton.setBounds (header.removeFromRight (70).reduced (2));
    importLyricsButton.setBounds (header.removeFromRight (110).reduced (2));
    importScoreButton.setBounds (header.removeFromRight (110).reduced (2));
    separateButton.setBounds (header.removeFromRight (130).reduced (2));
    openSongButton.setBounds (header.removeFromRight (110).reduced (2));
    recentProjectsButton.setBounds (header.removeFromRight (80).reduced (2));
    loadProjectButton.setBounds (header.removeFromRight (110).reduced (2));
    saveProjectButton.setBounds (header.removeFromRight (110).reduced (2));

    bounds.removeFromTop (8);
    statusLabel.setBounds (bounds.removeFromTop (24));

    if (separationProgress.isVisible())
    {
        bounds.removeFromTop (4);
        separationProgress.setBounds (bounds.removeFromTop (48));
    }

    bounds.removeFromTop (8);
    waveformDisplay.setBounds (bounds.removeFromTop (100));
    bounds.removeFromTop (8);
    lyricsView.setBounds (bounds.removeFromTop (72));
    bounds.removeFromTop (8);
    notationViewport.setBounds (bounds.removeFromTop (160));
    bounds.removeFromTop (8);

    transportBar.setBounds (bounds.removeFromTop (90));
    bounds.removeFromTop (8);

    stemViewport.setBounds (bounds);

    const auto stripWidth = 110;
    const auto stripHeight = juce::jmax (180, stemViewport.getHeight() - 4);
    stemContainer.setSize (juce::jmax (stemViewport.getWidth(), stemContainer.getNumChildComponents() * stripWidth),
                           stripHeight);

    auto stripBounds = stemContainer.getLocalBounds().reduced (4);
    int x = stripBounds.getX();

    for (int i = 0; i < stemContainer.getNumChildComponents(); ++i)
    {
        if (auto* strip = stemContainer.getChildComponent (i))
        {
            strip->setBounds (x, stripBounds.getY(), stripWidth - 8, stripBounds.getHeight());
            x += stripWidth;
        }
    }
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
            currentSongFile, currentScoreFile, currentLyricsFile, transportController, transportBar);

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
    setStatus ("Project loaded: " + file.getFileName()
               + (currentScore.hasLyrics() || ! currentLyrics.isEmpty() ? " (with synced lyrics)" : ""));
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

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (recentProjectsButton),
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
            setStatus ("Loaded: " + file.getFileName());
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

void MainComponent::separateStems()
{
    if (! currentSongFile.existsAsFile())
    {
        setStatus ("Open a song file before separating stems.");
        return;
    }

    if (! demucsSeparator.isAvailable())
    {
        setStatus ("Demucs is not installed. Run: pip install demucs");
        return;
    }

    separateButton.setEnabled (false);
    separationProgress.setVisible (true);
    separationProgress.setProgress (0.0f, "Starting stem separation...");
    setStatus ("Separating stems...");
    resized();

    demucsSeparator.separateAsync (currentSongFile,
        [this] (const jamstudio::ai::SeparationResult& result)
        {
            separateButton.setEnabled (true);
            separationProgress.reset();
            resized();

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
        recordButton.setButtonText ("Record");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff8b2f2f));

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
        recordButton.setButtonText ("Stop");
        recordButton.setColour (juce::TextButton::buttonColourId, juce::Colours::red.darker());
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
        waveformDisplay.setSourceFile (stemFiles.getReference (0));

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