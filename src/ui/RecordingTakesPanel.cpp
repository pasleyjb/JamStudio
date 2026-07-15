#include "RecordingTakesPanel.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

RecordingTakesPanel::RecordingTakesPanel()
{
    title.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    title.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (title);

    hint.setFont (juce::FontOptions (11.0f));
    hint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (hint);

    openExternalButton.setButtonText ("Open Studio");
    openExternalButton.setTooltip ("Linux: prepare Ardour session pack, hand off audio interface, launch Ardour. "
                                   "Elsewhere: bounce mix and open external recorder.");
    openExternalButton.onClick = [this]
    {
        if (onOpenExternal)
            onOpenExternal();
    };
    addAndMakeVisible (openExternalButton);

    importButton.setTooltip ("Import a finished take WAV/AIFF from your external DAW into the mixer");
    importButton.onClick = [this]
    {
        if (onImportTake)
            onImportTake();
    };
    addAndMakeVisible (importButton);

    list.setModel (this);
    list.setRowHeight (28);
    list.setColour (juce::ListBox::backgroundColourId, JamStudioTheme::getColours().panelBackground);
    addAndMakeVisible (list);

    loadButton.onClick = [this] { loadSelected(); };
    addAndMakeVisible (loadButton);

    emptyLabel.setJustificationType (juce::Justification::centred);
    emptyLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (emptyLabel);
}

void RecordingTakesPanel::setTakeManager (const jamstudio::audio::RecordingTakeManager* manager)
{
    takeManager = manager;
    refresh();
}

void RecordingTakesPanel::setLoadTakeCallback (LoadTakeCallback cb)
{
    onLoadTake = std::move (cb);
}

void RecordingTakesPanel::setOpenExternalCallback (VoidCallback cb)
{
    onOpenExternal = std::move (cb);
}

void RecordingTakesPanel::setImportTakeCallback (VoidCallback cb)
{
    onImportTake = std::move (cb);
}

void RecordingTakesPanel::setPreferredRecorderName (const juce::String& name)
{
    // Keep the product name "Open Studio" unless we have a clear studio host.
    if (name.containsIgnoreCase ("Ardour") || name.containsIgnoreCase ("Studio"))
        openExternalButton.setButtonText ("Open Studio (Ardour)");
    else if (name.isNotEmpty() && ! name.containsIgnoreCase ("Audacity"))
        openExternalButton.setButtonText ("Open " + name);
    else
        openExternalButton.setButtonText ("Open Studio (Ardour)");

    openExternalButton.setTooltip (
        "Prepare session pack, release the audio interface, and open Ardour. "
        "Use Transport → Open External Recorder for Audacity.");
}

void RecordingTakesPanel::refresh()
{
    list.updateContent();
    const auto n = takeManager != nullptr ? takeManager->getNumTakes() : 0;
    emptyLabel.setVisible (n == 0);
    list.setVisible (n > 0);
    loadButton.setEnabled (n > 0);
    repaint();
}

void RecordingTakesPanel::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.setColour (colours.panelBackground);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (1.0f), 8.0f, 1.0f);

    g.setColour (juce::Colour (0xffff3344));
    g.fillRoundedRectangle (getLocalBounds().removeFromTop (3).toFloat().reduced (6.0f, 0.0f), 1.5f);
}

void RecordingTakesPanel::resized()
{
    auto a = getLocalBounds().reduced (10, 8);
    title.setBounds (a.removeFromTop (18));
    a.removeFromTop (2);
    hint.setBounds (a.removeFromTop (32));
    a.removeFromTop (4);
    openExternalButton.setBounds (a.removeFromTop (32).reduced (0, 2));
    a.removeFromTop (4);
    importButton.setBounds (a.removeFromTop (30).reduced (0, 2));
    a.removeFromTop (6);
    loadButton.setBounds (a.removeFromBottom (30).reduced (0, 2));
    a.removeFromBottom (4);
    list.setBounds (a);
    emptyLabel.setBounds (a);
}

int RecordingTakesPanel::getNumRows()
{
    return takeManager != nullptr ? takeManager->getNumTakes() : 0;
}

void RecordingTakesPanel::paintListBoxItem (const int row,
                                            juce::Graphics& g,
                                            const int width,
                                            const int height,
                                            const bool selected)
{
    if (selected)
        g.fillAll (JamStudioTheme::getColours().accent.withAlpha (0.22f));

    if (takeManager == nullptr)
        return;

    if (const auto* take = takeManager->getTake (row))
    {
        g.setColour (JamStudioTheme::getColours().text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (take->displayName, 8, 0, width - 16, height, juce::Justification::centredLeft);
    }
}

void RecordingTakesPanel::listBoxItemDoubleClicked (const int row, const juce::MouseEvent&)
{
    juce::ignoreUnused (row);
    loadSelected();
}

void RecordingTakesPanel::loadSelected()
{
    if (takeManager == nullptr || onLoadTake == nullptr)
        return;

    const auto row = list.getSelectedRow();
    if (const auto* take = takeManager->getTake (row >= 0 ? row : takeManager->getNumTakes() - 1))
        onLoadTake (*take);
}

} // namespace jamstudio::ui
