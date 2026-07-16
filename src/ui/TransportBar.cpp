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
    recordButton.setTooltip ("Open Studio (Ardour on Linux): hand off interface + session pack. "
                             "Use Transport menu for Audacity if preferred.");
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

    inputMonitorButton.setClickingTogglesState (true);
    inputMonitorButton.setIndicatorColour (juce::Colour (0xff44cc88));
    inputMonitorButton.setTooltip (
        "Input monitor — hear yourself (guitar / singing mic) mixed with the stems.\n"
        "ON by default. Turn OFF if you get feedback with open mics + speakers.\n"
        "Use the channel box: In 1 = first jack, All = guitar + vocal together.\n"
        "Scarlett Direct Monitor is still useful for zero-latency headphones.");
    inputMonitorButton.onClick = [this]
    {
        if (inputMonitorSetEnabled)
            inputMonitorSetEnabled (inputMonitorButton.getToggleState());
        inputMonitorButton.setIndicatorActive (inputMonitorButton.getToggleState());
        const bool on = inputMonitorButton.getToggleState();
        inputMonitorGainSlider.setEnabled (on);
        inputMonitorChannelBox.setEnabled (on);
    };
    addAndMakeVisible (inputMonitorButton);

    inputMonitorChannelBox.setTooltip (
        "Which input to hear:\n"
        "In 1 / In 2 = one jack\n"
        "1+2 = guitar + vocal only (recommended — quieter)\n"
        "All gated = every jack, empty ones muted by noise gate");
    inputMonitorChannelBox.onChange = [this]
    {
        if (! inputMonitorSetChannel)
            return;
        const auto id = inputMonitorChannelBox.getSelectedId();
        // id 1..N = channel 0..N-1; 100 = first two; 101 = all gated
        if (id == 100)
            inputMonitorSetChannel (-1); // first two
        else if (id == 101)
            inputMonitorSetChannel (-2); // all gated
        else if (id >= 1)
            inputMonitorSetChannel (id - 1);
    };
    addAndMakeVisible (inputMonitorChannelBox);
    setInputMonitorChannelCount (2);

    inputMonitorGainSlider.setRange (0.0, 1.25, 0.01);
    inputMonitorGainSlider.setValue (0.45, juce::dontSendNotification);
    inputMonitorGainSlider.setTooltip ("How loud you (guitar / vocal) are in the monitor mix. "
                                       "Lower this if you hear hiss.");
    inputMonitorGainSlider.onValueChange = [this]
    {
        if (inputMonitorSetGain)
            inputMonitorSetGain (static_cast<float> (inputMonitorGainSlider.getValue()));
    };
    addAndMakeVisible (inputMonitorGainSlider);

    startTimerHz (15);
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

void TransportBar::setInputMonitorCallbacks (std::function<bool()> isEnabled,
                                             std::function<void (bool)> setEnabled,
                                             std::function<float()> getGain,
                                             std::function<void (float)> setGain,
                                             std::function<int()> getChannel,
                                             std::function<void (int)> setChannel)
{
    inputMonitorIsEnabled = std::move (isEnabled);
    inputMonitorSetEnabled = std::move (setEnabled);
    inputMonitorGetGain = std::move (getGain);
    inputMonitorSetGain = std::move (setGain);
    inputMonitorGetChannel = std::move (getChannel);
    inputMonitorSetChannel = std::move (setChannel);
    syncInputMonitorUi();
}

void TransportBar::setInputMonitorChannelCount (const int numOpenInputs)
{
    inputMonitorChannelCount = juce::jlimit (1, 16, juce::jmax (1, numOpenInputs));
    const auto keep = inputMonitorChannelBox.getSelectedId();
    inputMonitorChannelBox.clear (juce::dontSendNotification);
    for (int i = 0; i < inputMonitorChannelCount; ++i)
        inputMonitorChannelBox.addItem ("In " + juce::String (i + 1), i + 1);
    inputMonitorChannelBox.addItem ("1+2", 100);
    if (inputMonitorChannelCount > 2)
        inputMonitorChannelBox.addItem ("All gated", 101);
    if (keep > 0)
        inputMonitorChannelBox.setSelectedId (keep, juce::dontSendNotification);
    else
        inputMonitorChannelBox.setSelectedId (100, juce::dontSendNotification); // default 1+2
}

void TransportBar::syncInputMonitorUi()
{
    const bool on = inputMonitorIsEnabled ? inputMonitorIsEnabled() : true;
    const float gain = inputMonitorGetGain ? inputMonitorGetGain() : 0.45f;
    inputMonitorButton.setToggleState (on, juce::dontSendNotification);
    inputMonitorButton.setIndicatorActive (on);
    inputMonitorGainSlider.setValue (gain, juce::dontSendNotification);
    inputMonitorGainSlider.setEnabled (on);
    inputMonitorChannelBox.setEnabled (on);

    if (inputMonitorGetChannel)
    {
        const int ch = inputMonitorGetChannel();
        if (ch == -2)
            inputMonitorChannelBox.setSelectedId (101, juce::dontSendNotification);
        else if (ch < 0)
            inputMonitorChannelBox.setSelectedId (100, juce::dontSendNotification);
        else
            inputMonitorChannelBox.setSelectedId (ch + 1, juce::dontSendNotification);
    }
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
    inputMeterBounds = bounds.removeFromLeft (40).reduced (0, 10);
    bounds.removeFromLeft (2);
    inputMonitorButton.setBounds (bounds.removeFromLeft (40).reduced (1));
    inputMonitorChannelBox.setBounds (bounds.removeFromLeft (58).reduced (1));
    inputMonitorGainSlider.setBounds (bounds.removeFromLeft (48).reduced (1, 8));
    bounds.removeFromLeft (6);

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
    // Record indicator only changes on start/stop — skip every tick.

    if (inputLevelProvider)
        setInputLevel (inputLevelProvider());

    const auto position = transportController.getPosition();
    const auto length = transportController.getLengthInSeconds();
    const auto text = formatTime (position) + " / " + formatTime (length);
    if (positionLabel.getText() != text)
        positionLabel.setText (text, juce::dontSendNotification);
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
