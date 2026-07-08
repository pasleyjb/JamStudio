#include "SeparationProgressBar.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

SeparationProgressBar::SeparationProgressBar()
    : progressBar (progressValue)
{
    statusLabel.setJustificationType (juce::Justification::centredLeft);
    cancelButton.onClick = [this]
    {
        if (cancelCallback != nullptr)
            cancelCallback();
    };
    addChildComponent (progressBar);
    addChildComponent (statusLabel);
    addChildComponent (cancelButton);
    setVisible (false);
}

void SeparationProgressBar::setVisible (const bool shouldBeVisible)
{
    juce::Component::setVisible (shouldBeVisible);
    progressBar.setVisible (shouldBeVisible);
    statusLabel.setVisible (shouldBeVisible);
    cancelButton.setVisible (shouldBeVisible);
}

void SeparationProgressBar::setProgress (const float progress, const juce::String& statusText)
{
    progressValue = juce::jlimit (0.0, 1.0, static_cast<double> (progress));
    progressBar.setPercentageDisplay (true);
    statusLabel.setText (statusText, juce::dontSendNotification);
    repaint();
}

void SeparationProgressBar::setCancelCallback (std::function<void()> callback)
{
    cancelCallback = std::move (callback);
}

void SeparationProgressBar::reset()
{
    progressValue = 0.0;
    statusLabel.setText ({}, juce::dontSendNotification);
    cancelCallback = nullptr;
    setVisible (false);
}

void SeparationProgressBar::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void SeparationProgressBar::resized()
{
    auto bounds = getLocalBounds().reduced (2);
    auto header = bounds.removeFromTop (20);
    cancelButton.setBounds (header.removeFromRight (72).reduced (1));
    statusLabel.setBounds (header);
    progressBar.setBounds (bounds.removeFromTop (22).reduced (0, 2));
}

} // namespace jamstudio::ui