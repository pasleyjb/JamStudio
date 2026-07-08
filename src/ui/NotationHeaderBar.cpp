#include "NotationHeaderBar.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

NotationHeaderBar::NotationHeaderBar()
{
    titleLabel.setText ("Notation", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    tabButton.setClickingTogglesState (true);
    tabButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    tabButton.onClick = [this]
    {
        notifyModeChanged (jamstudio::notation::NotationMode::tab);
    };
    addAndMakeVisible (tabButton);

    sheetButton.setClickingTogglesState (true);
    sheetButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    sheetButton.onClick = [this]
    {
        notifyModeChanged (jamstudio::notation::NotationMode::standard);
    };
    addAndMakeVisible (sheetButton);

    updateModeButtons();
}

void NotationHeaderBar::setTitle (const juce::String& title)
{
    titleLabel.setText (title.isNotEmpty() ? title : "Notation", juce::dontSendNotification);
}

void NotationHeaderBar::setNotationMode (const jamstudio::notation::NotationMode mode)
{
    currentMode = mode;
    updateModeButtons();
}

void NotationHeaderBar::setModeChangedCallback (ModeChangedCallback callback)
{
    onModeChanged = std::move (callback);
}

void NotationHeaderBar::setHasScore (const bool hasScore)
{
    tabButton.setEnabled (hasScore);
    sheetButton.setEnabled (hasScore);
}

void NotationHeaderBar::updateModeButtons()
{
    const auto isTab = currentMode == jamstudio::notation::NotationMode::tab;
    tabButton.setToggleState (isTab, juce::dontSendNotification);
    sheetButton.setToggleState (! isTab, juce::dontSendNotification);
    tabButton.setIndicatorActive (isTab);
    sheetButton.setIndicatorActive (! isTab);
}

void NotationHeaderBar::notifyModeChanged (const jamstudio::notation::NotationMode mode)
{
    currentMode = mode;
    updateModeButtons();

    if (onModeChanged != nullptr)
        onModeChanged (mode);
}

void NotationHeaderBar::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.panelBackground);
    g.setColour (colours.border);
    g.drawHorizontalLine (getHeight() - 1, 0.0f, static_cast<float> (getWidth()));
}

void NotationHeaderBar::resized()
{
    auto bounds = getLocalBounds().reduced (6, 2);
    tabButton.setBounds (bounds.removeFromRight (56));
    bounds.removeFromRight (4);
    sheetButton.setBounds (bounds.removeFromRight (64));
    bounds.removeFromRight (8);
    titleLabel.setBounds (bounds);
}

} // namespace jamstudio::ui