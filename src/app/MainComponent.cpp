#include "MainComponent.h"

#include "../notation/MusicXmlParser.h"

namespace jamstudio::app
{

MainComponent::MainComponent (juce::AudioDeviceManager& deviceManager)
    : audioDeviceManager (deviceManager),
      transportController (deviceManager),
      recordingExporter (transportController.getFormatManager()),
      waveformDisplay (transportController.getFormatManager(), thumbnailCache, transportController),
      notationView (transportController),
      transportBar (transportController)
{
    setSize (1024, 780);

    titleLabel.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    openSongButton.onClick = [this] { openSong(); };
    addAndMakeVisible (openSongButton);

    separateButton.onClick = [this] { separateStems(); };
    addAndMakeVisible (separateButton);

    importScoreButton.onClick = [this] { importScore(); };
    addAndMakeVisible (importScoreButton);

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
    titleLabel.setBounds (header.removeFromLeft (160));
    recordButton.setBounds (header.removeFromRight (90).reduced (2));
    importScoreButton.setBounds (header.removeFromRight (120).reduced (2));
    separateButton.setBounds (header.removeFromRight (140).reduced (2));
    openSongButton.setBounds (header.removeFromRight (120).reduced (2));

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
    notationViewport.setBounds (bounds.removeFromTop (180));
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

        currentScore = importedScore;
        notationView.setScore (currentScore);
        transportController.getMetronome().setBpm (currentScore.getTempo());
        setStatus ("Score loaded: " + file.getFileName() + " (" + juce::String (currentScore.getNumMeasures()) + " measures)");
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