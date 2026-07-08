#pragma once

#include "../ai/DemucsSeparator.h"
#include "../audio/AudioRecorder.h"
#include "../audio/RecordingExporter.h"
#include "../audio/TransportController.h"
#include "../notation/NotationView.h"
#include "../notation/Score.h"
#include "../ui/SeparationProgressBar.h"
#include "../ui/StemStrip.h"
#include "../ui/TransportBar.h"
#include "../ui/WaveformDisplay.h"

namespace jamstudio::app
{

class MainComponent : public juce::Component,
                      public juce::ChangeListener
{
public:
    explicit MainComponent (juce::AudioDeviceManager& deviceManager);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

private:
    void openSong();
    void importScore();
    void separateStems();
    void toggleRecording();
    void loadRecordingAsStem (const juce::File& recordingFile);
    void loadStemsIntoMixer (const juce::Array<juce::File>& stemFiles);
    void rebuildStemStrips();
    void setStatus (const juce::String& message);
    [[nodiscard]] juce::File getDefaultRecordingFile() const;

    juce::AudioDeviceManager& audioDeviceManager;
    jamstudio::audio::TransportController transportController;
    jamstudio::audio::AudioRecorder audioRecorder;
    jamstudio::audio::RecordingExporter recordingExporter;
    jamstudio::ai::DemucsSeparator demucsSeparator;
    jamstudio::notation::Score currentScore;

    juce::AudioThumbnailCache thumbnailCache { 4 };

    juce::Label titleLabel { {}, "JamStudio" };
    juce::TextButton openSongButton { "Open Song..." };
    juce::TextButton separateButton { "Separate Stems" };
    juce::TextButton importScoreButton { "Import Score..." };
    juce::TextButton recordButton { "Record" };
    juce::Label statusLabel;
    jamstudio::ui::SeparationProgressBar separationProgress;
    jamstudio::ui::WaveformDisplay waveformDisplay;
    juce::Viewport notationViewport;
    jamstudio::notation::NotationView notationView;
    jamstudio::ui::TransportBar transportBar;
    juce::Viewport stemViewport;
    juce::Component stemContainer;

    juce::File currentSongFile;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

} // namespace jamstudio::app