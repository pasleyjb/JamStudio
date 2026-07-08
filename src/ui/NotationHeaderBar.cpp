#include "NotationHeaderBar.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

NotationHeaderBar::NotationHeaderBar()
{
    titleLabel.setText ("Notation", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    partSelector.onChange = [this]
    {
        if (onPartChanged != nullptr)
            onPartChanged (partSelector.getSelectedItemIndex());
    };
    addAndMakeVisible (partLabel);
    addAndMakeVisible (partSelector);

    tabButton.setClickingTogglesState (true);
    tabButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    tabButton.onClick = [this] { handleTabButton(); };
    addAndMakeVisible (tabButton);

    sheetButton.setClickingTogglesState (true);
    sheetButton.setIndicatorColour (JamStudioTheme::getColours().accent);
    sheetButton.onClick = [this] { handleSheetButton(); };
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

void NotationHeaderBar::setPartChangedCallback (PartChangedCallback callback)
{
    onPartChanged = std::move (callback);
}

void NotationHeaderBar::setHasScore (const bool hasScore)
{
    tabButton.setEnabled (hasScore);
    sheetButton.setEnabled (hasScore);
    partSelector.setEnabled (hasScore);
}

void NotationHeaderBar::setParts (const juce::StringArray& partNames, const int activePartIndex)
{
    partSelector.clear (juce::dontSendNotification);

    for (int i = 0; i < partNames.size(); ++i)
        partSelector.addItem (partNames[i], i + 1);

    const auto showSelector = partNames.size() > 1;
    partLabel.setVisible (showSelector);
    partSelector.setVisible (showSelector);

    if (partNames.isEmpty())
        return;

    partSelector.setSelectedItemIndex (juce::jlimit (0, partNames.size() - 1, activePartIndex),
                                       juce::dontSendNotification);
    resized();
}

void NotationHeaderBar::handleTabButton()
{
    if (tabButton.getToggleState())
    {
        sheetButton.setToggleState (false, juce::dontSendNotification);
        currentMode = jamstudio::notation::NotationMode::tab;
        updateModeButtons();

        if (onModeChanged != nullptr)
            onModeChanged (jamstudio::notation::NotationMode::tab);
    }
    else if (onModeChanged != nullptr)
    {
        currentMode = jamstudio::notation::NotationMode::hidden;
        updateModeButtons();
        onModeChanged (jamstudio::notation::NotationMode::hidden);
    }
}

void NotationHeaderBar::handleSheetButton()
{
    if (sheetButton.getToggleState())
    {
        tabButton.setToggleState (false, juce::dontSendNotification);
        currentMode = jamstudio::notation::NotationMode::standard;
        updateModeButtons();

        if (onModeChanged != nullptr)
            onModeChanged (jamstudio::notation::NotationMode::standard);
    }
    else if (onModeChanged != nullptr)
    {
        currentMode = jamstudio::notation::NotationMode::hidden;
        updateModeButtons();
        onModeChanged (jamstudio::notation::NotationMode::hidden);
    }
}

void NotationHeaderBar::updateModeButtons()
{
    const auto isHidden = currentMode == jamstudio::notation::NotationMode::hidden;
    const auto isTab = currentMode == jamstudio::notation::NotationMode::tab;
    const auto isSheet = currentMode == jamstudio::notation::NotationMode::standard;

    tabButton.setToggleState (isTab, juce::dontSendNotification);
    sheetButton.setToggleState (isSheet, juce::dontSendNotification);
    tabButton.setIndicatorActive (isTab);
    sheetButton.setIndicatorActive (isSheet);

    if (isHidden)
    {
        tabButton.setToggleState (false, juce::dontSendNotification);
        sheetButton.setToggleState (false, juce::dontSendNotification);
    }
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

    if (partSelector.isVisible())
    {
        partSelector.setBounds (bounds.removeFromRight (140));
        bounds.removeFromRight (4);
        partLabel.setBounds (bounds.removeFromRight (36));
        bounds.removeFromRight (8);
    }

    titleLabel.setBounds (bounds);
}

} // namespace jamstudio::ui