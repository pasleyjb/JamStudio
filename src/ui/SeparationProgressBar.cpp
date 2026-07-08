#include "SeparationProgressBar.h"

namespace jamstudio::ui
{

SeparationProgressBar::SeparationProgressBar()
    : progressBar (progressValue)
{
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addChildComponent (progressBar);
    addChildComponent (statusLabel);
    setVisible (false);
}

void SeparationProgressBar::setVisible (const bool shouldBeVisible)
{
    juce::Component::setVisible (shouldBeVisible);
    progressBar.setVisible (shouldBeVisible);
    statusLabel.setVisible (shouldBeVisible);
}

void SeparationProgressBar::setProgress (const float progress, const juce::String& statusText)
{
    progressValue = juce::jlimit (0.0, 1.0, static_cast<double> (progress));
    progressBar.setPercentageDisplay (true);
    statusLabel.setText (statusText, juce::dontSendNotification);
    repaint();
}

void SeparationProgressBar::reset()
{
    progressValue = 0.0;
    statusLabel.setText ({}, juce::dontSendNotification);
    setVisible (false);
}

void SeparationProgressBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e1e));
}

void SeparationProgressBar::resized()
{
    auto bounds = getLocalBounds().reduced (2);
    statusLabel.setBounds (bounds.removeFromTop (20));
    progressBar.setBounds (bounds.removeFromTop (22).reduced (0, 2));
}

} // namespace jamstudio::ui