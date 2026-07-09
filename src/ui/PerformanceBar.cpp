#include "PerformanceBar.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

PerformanceBar::PerformanceBar()
{
    setLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    setLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (setLabel);

    songLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    songLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().text);
    addAndMakeVisible (songLabel);

    phaseLabel.setFont (juce::FontOptions (13.0f));
    phaseLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().accent);
    addAndMakeVisible (phaseLabel);

    triggerButton.setColour (juce::TextButton::buttonColourId,
                             JamStudioTheme::getColours().accent.darker (0.15f));
    triggerButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    triggerButton.onClick = [this]
    {
        if (onTrigger)
            onTrigger();
    };
    addAndMakeVisible (triggerButton);

    setSetListInfo ("Set", -1, 0, {});
    setPhaseMessage ("Ready");
}

void PerformanceBar::setSetListInfo (const juce::String& setName,
                                     const int songIndex,
                                     const int songCount,
                                     const juce::String& songTitle)
{
    setLabel.setText ("SET: " + setName, juce::dontSendNotification);

    if (songCount <= 0)
        songLabel.setText ("No songs in set list", juce::dontSendNotification);
    else if (songIndex < 0)
        songLabel.setText ("Ready — press NEXT / START (or foot pedal)", juce::dontSendNotification);
    else
        songLabel.setText (juce::String (songIndex + 1) + " / " + juce::String (songCount)
                           + "  —  " + songTitle,
                           juce::dontSendNotification);
}

void PerformanceBar::setPhaseMessage (const juce::String& message)
{
    phaseLabel.setText (message, juce::dontSendNotification);
}

void PerformanceBar::setWaitingForTrigger (const bool shouldWait)
{
    waiting = shouldWait;
    triggerButton.setButtonText (waiting ? "START NEXT SONG" : "NEXT / START");
    triggerButton.setColour (juce::TextButton::buttonColourId,
                             waiting ? juce::Colour (0xff22aa55)
                                     : JamStudioTheme::getColours().accent.darker (0.15f));
    repaint();
}

void PerformanceBar::setTriggerCallback (TriggerCallback cb)
{
    onTrigger = std::move (cb);
}

void PerformanceBar::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.panelBackground.brighter (waiting ? 0.06f : 0.02f));
    g.setColour (waiting ? colours.accent : colours.border);
    g.drawRect (getLocalBounds(), waiting ? 2 : 1);
}

void PerformanceBar::resized()
{
    auto area = getLocalBounds().reduced (10, 6);
    triggerButton.setBounds (area.removeFromRight (200).reduced (2, 0));
    area.removeFromRight (12);

    setLabel.setBounds (area.removeFromTop (16));
    songLabel.setBounds (area.removeFromTop (22));
    phaseLabel.setBounds (area);
}

} // namespace jamstudio::ui
