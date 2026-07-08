#include "TransportBar.h"

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
    playButton.onClick = [this] { transportController.play(); };
    addAndMakeVisible (playButton);

    pauseButton.onClick = [this] { transportController.pause(); };
    addAndMakeVisible (pauseButton);

    stopButton.onClick = [this] { transportController.stop(); };
    addAndMakeVisible (stopButton);

    positionSlider.setRange (0.0, 1.0, 0.001);
    positionSlider.onValueChange = [this]
    {
        const auto length = transportController.getLengthInSeconds();

        if (length > 0.0)
            transportController.setPosition (positionSlider.getValue() * length);
    };
    addAndMakeVisible (positionSlider);

    positionLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (positionLabel);

    metronomeButton.onClick = [this]
    {
        transportController.getMetronome().setEnabled (metronomeButton.getToggleState());
    };
    addAndMakeVisible (metronomeButton);

    bpmSlider.setRange (40.0, 240.0, 1.0);
    bpmSlider.setValue (120.0, juce::dontSendNotification);
    bpmSlider.onValueChange = [this]
    {
        transportController.getMetronome().setBpm (bpmSlider.getValue());
    };
    addAndMakeVisible (bpmSlider);
    addAndMakeVisible (bpmLabel);

    startTimerHz (15);
}

void TransportBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

void TransportBar::resized()
{
    auto bounds = getLocalBounds().reduced (8);

    auto transportRow = bounds.removeFromTop (36);
    playButton.setBounds (transportRow.removeFromLeft (70).reduced (2));
    pauseButton.setBounds (transportRow.removeFromLeft (70).reduced (2));
    stopButton.setBounds (transportRow.removeFromLeft (70).reduced (2));
    transportRow.removeFromLeft (8);
    positionLabel.setBounds (transportRow.removeFromRight (90));
    positionSlider.setBounds (transportRow.reduced (2));

    bounds.removeFromTop (6);

    auto metronomeRow = bounds.removeFromTop (36);
    metronomeButton.setBounds (metronomeRow.removeFromLeft (120).reduced (2));
    bpmLabel.setBounds (metronomeRow.removeFromLeft (40));
    bpmSlider.setBounds (metronomeRow.reduced (2));
}

void TransportBar::timerCallback()
{
    updatePositionSlider();

    const auto position = transportController.getPosition();
    const auto length = transportController.getLengthInSeconds();
    positionLabel.setText (formatTime (position) + " / " + formatTime (length), juce::dontSendNotification);
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

} // namespace jamstudio::ui