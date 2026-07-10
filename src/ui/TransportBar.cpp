#include "TransportBar.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

namespace
{
juce::String formatTime (const double seconds)
{
    const auto totalSeconds = juce::jmax (0, static_cast<int> (seconds));
    const auto minutes = totalSeconds / 60;
    const auto secs = totalSeconds % 60;
    return juce::String::formatted ("%d:%02d", minutes, secs);
}
} // namespace

TransportBar::TransportBar (jamstudio::audio::TransportController& transport)
    : transportController (transport)
{
    skipBackButton.onClick = [this]
    {
        transportController.skipBack (5.0);
        updateTransportIndicators();
    };
    addAndMakeVisible (skipBackButton);

    playButton.onClick = [this]
    {
        transportController.play();
        updateTransportIndicators();
    };
    addAndMakeVisible (playButton);

    pauseButton.onClick = [this]
    {
        transportController.pause();
        updateTransportIndicators();
    };
    addAndMakeVisible (pauseButton);

    stopButton.onClick = [this]
    {
        transportController.stop();
        updateTransportIndicators();
    };
    addAndMakeVisible (stopButton);

    skipForwardButton.onClick = [this]
    {
        transportController.skipForward (5.0);
        updateTransportIndicators();
    };
    addAndMakeVisible (skipForwardButton);

    recordButton.setIndicatorColour (juce::Colour (0xffff3344));
    recordButton.setTooltip ("Open external recorder (Audacity, etc.) with bounced mix for plugins / amp sims");
    recordButton.setButtonText ("EXT");
    recordButton.onClick = [this]
    {
        if (recordCallback != nullptr)
            recordCallback();
    };
    addAndMakeVisible (recordButton);

    positionSlider.setRange (0.0, 1.0, 0.001);
    positionSlider.onValueChange = [this]
    {
        const auto length = transportController.getLengthInSeconds();

        if (length > 0.0)
            transportController.setPosition (positionSlider.getValue() * length);
    };
    addAndMakeVisible (positionSlider);

    positionLabel.setJustificationType (juce::Justification::centredLeft);
    positionLabel.setFont (juce::FontOptions (13.0f).withStyle ("Monospaced"));
    addAndMakeVisible (positionLabel);

    masterVolumeSlider.setRange (0.0, 1.0, 0.01);
    masterVolumeSlider.setValue (transportController.getStemMixer().getMasterVolume(), juce::dontSendNotification);
    masterVolumeSlider.onValueChange = [this]
    {
        transportController.getStemMixer().setMasterVolume (static_cast<float> (masterVolumeSlider.getValue()));
    };
    addAndMakeVisible (masterLabel);
    addAndMakeVisible (masterVolumeSlider);

    metronomeButton.setClickingTogglesState (true);
    metronomeButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
    metronomeButton.onClick = [this]
    {
        transportController.getMetronome().setEnabled (metronomeButton.getToggleState());
        updateMetronomeIndicator();
    };
    addAndMakeVisible (metronomeButton);

    countInButton.setClickingTogglesState (true);
    countInButton.setIndicatorColour (JamStudioTheme::getColours().indicatorSolo);
    countInButton.setToggleState (transportController.isCountInEnabled(), juce::dontSendNotification);
    countInButton.setTooltip ("4-count intro before every play (all modes)");
    countInButton.onClick = [this]
    {
        transportController.setCountInEnabled (countInButton.getToggleState());
        updateCountInIndicator();
    };
    addAndMakeVisible (countInButton);

    detectTempoButton.onClick = [this]
    {
        if (detectTempoCallback != nullptr)
            detectTempoCallback();
    };
    addAndMakeVisible (detectTempoButton);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this]
    {
        transportController.getMetronome().setBpm (bpmSlider.getValue());
    };
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (bpmLabel);

    inputLabel.setFont (juce::FontOptions (10.0f, juce::Font::bold));
    inputLabel.setJustificationType (juce::Justification::centred);
    inputLabel.setColour (juce::Label::textColourId, juce::Colour (0xffff5566));
    addAndMakeVisible (inputLabel);

    startTimerHz (30);
}

void TransportBar::setDetectTempoCallback (DetectTempoCallback callback)
{
    detectTempoCallback = std::move (callback);
}

void TransportBar::setRecordCallback (RecordCallback callback)
{
    recordCallback = std::move (callback);
}

void TransportBar::setInputLevelProvider (std::function<float()> provider)
{
    inputLevelProvider = std::move (provider);
}

void TransportBar::setRecordingActive (const bool recording)
{
    recordingActive = recording;
    updateRecordIndicator();
}

void TransportBar::setInputLevel (const float level01)
{
    inputLevel = juce::jlimit (0.0f, 1.0f, level01);
    repaint (inputMeterBounds.expanded (2));
}

void TransportBar::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.panelBackground);
    g.setColour (colours.border.withAlpha (0.55f));
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));

    // Input level meter
    auto meter = inputMeterBounds.toFloat();
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (meter, 3.0f);
    const auto fill = meter.withWidth (meter.getWidth() * inputLevel);
    g.setColour (inputLevel > 0.9f ? juce::Colours::red
                                   : (recordingActive ? juce::Colour (0xffff3344)
                                                      : juce::Colour (0xff66cc88)));
    g.fillRoundedRectangle (fill, 3.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (meter, 3.0f, 1.0f);
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds().reduced (6, 4);

    const auto deckSize = juce::jmin (bounds.getHeight(), 40);
    skipBackButton.setBounds (bounds.removeFromLeft (deckSize).reduced (1));
    playButton.setBounds (bounds.removeFromLeft (deckSize).reduced (1));
    pauseButton.setBounds (bounds.removeFromLeft (deckSize).reduced (1));
    stopButton.setBounds (bounds.removeFromLeft (deckSize).reduced (1));
    skipForwardButton.setBounds (bounds.removeFromLeft (deckSize).reduced (1));
    bounds.removeFromLeft (6);
    recordButton.setBounds (bounds.removeFromLeft (56).reduced (1));
    bounds.removeFromLeft (6);
    inputLabel.setBounds (bounds.removeFromLeft (22));
    inputMeterBounds = bounds.removeFromLeft (56).reduced (0, 10);
    bounds.removeFromLeft (8);

    detectTempoButton.setBounds (bounds.removeFromRight (58).reduced (1));
    bpmSlider.setBounds (bounds.removeFromRight (100).reduced (1));
    bpmLabel.setBounds (bounds.removeFromRight (28));
    metronomeButton.setBounds (bounds.removeFromRight (58).reduced (1));
    countInButton.setBounds (bounds.removeFromRight (52).reduced (1));
    masterVolumeSlider.setBounds (bounds.removeFromRight (64).reduced (1));
    masterLabel.setBounds (bounds.removeFromRight (40));
    positionLabel.setBounds (bounds.removeFromRight (90));
    positionSlider.setBounds (bounds.reduced (1));
}

void TransportBar::timerCallback()
{
    updatePositionSlider();
    updateMetronomeIndicator();
    updateCountInIndicator();
    updateTransportIndicators();
    updateRecordIndicator();

    if (inputLevelProvider)
        setInputLevel (inputLevelProvider());

    const auto position = transportController.getPosition();
    const auto length = transportController.getLengthInSeconds();
    positionLabel.setText (formatTime (position) + " / " + formatTime (length), juce::dontSendNotification);
}

void TransportBar::updateTransportIndicators()
{
    const auto playing = transportController.isPlaying()
                         || transportController.getStageMedia().isPlaying();
    const auto counting = transportController.isCountingIn();
    playButton.setActive (playing || counting);
    pauseButton.setActive (! playing && ! counting && transportController.getPosition() > 0.0);
    stopButton.setActive (! playing && ! counting && transportController.getPosition() <= 0.001);
}

void TransportBar::updateRecordIndicator()
{
    // EXT = external DAW path (default). Internal record still available from Transport menu.
    if (recordingActive)
        recordButton.setButtonText ("STOP");
    else
        recordButton.setButtonText ("EXT");

    recordButton.setIndicatorActive (recordingActive, recordingActive);
}

void TransportBar::updateMetronomeIndicator()
{
    const auto enabled = metronomeButton.getToggleState();
    auto blink = false;

    if (enabled && (transportController.isPlaying() || transportController.isCountingIn()))
    {
        const auto bpm = transportController.getMetronome().getBpm();
        const auto beatPhase = transportController.isCountingIn()
                                   ? std::fmod (juce::Time::getMillisecondCounterHiRes() * bpm / 60000.0, 1.0)
                                   : std::fmod (transportController.getPosition() * bpm / 60.0, 1.0);
        blink = beatPhase < 0.15;
    }

    metronomeButton.setIndicatorActive (enabled, blink);
}

void TransportBar::updateCountInIndicator()
{
    const auto enabled = transportController.isCountInEnabled();
    countInButton.setToggleState (enabled, juce::dontSendNotification);

    auto blink = false;
    if (transportController.isCountingIn())
    {
        const auto bpm = transportController.getMetronome().getBpm();
        const auto phase = std::fmod (juce::Time::getMillisecondCounterHiRes() * bpm / 60000.0, 1.0);
        blink = phase < 0.35;
    }

    countInButton.setIndicatorActive (enabled, blink);
}

void TransportBar::updatePositionSlider()
{
    const auto length = transportController.getLengthInSeconds();

    if (length <= 0.0)
    {
        positionSlider.setValue (0.0, juce::dontSendNotification);
        return;
    }

    if (! positionSlider.isMouseButtonDown())
        positionSlider.setValue (transportController.getPosition() / length, juce::dontSendNotification);
}

bool TransportBar::isMetronomeEnabled() const noexcept
{
    return metronomeButton.getToggleState();
}

void TransportBar::setMetronomeEnabled (const bool enabled)
{
    metronomeButton.setToggleState (enabled, juce::dontSendNotification);
    transportController.getMetronome().setEnabled (enabled);
    updateMetronomeIndicator();
}

double TransportBar::getBpm() const noexcept
{
    return bpmSlider.getValue();
}

void TransportBar::setBpm (const double bpm)
{
    bpmSlider.setValue (bpm, juce::dontSendNotification);
    transportController.getMetronome().setBpm (bpm);
}

void TransportBar::setMasterVolume (const float volume)
{
    masterVolumeSlider.setValue (volume, juce::dontSendNotification);
    transportController.getStemMixer().setMasterVolume (volume);
}

float TransportBar::getMasterVolume() const noexcept
{
    return static_cast<float> (masterVolumeSlider.getValue());
}

} // namespace jamstudio::ui
