#include "OnlineLyricsDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

OnlineLyricsDialog::OnlineLyricsDialog (jamstudio::notation::SongMetadata meta,
                                        LyricsChosenCallback chosen)
    : metadata (std::move (meta)),
      onChosen (std::move (chosen))
{
    titleLabel.setText ("Find Synced Lyrics Online", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    queryLabel.setText ("Matches use song metadata (and filename). Edit and search again if needed.",
                        juce::dontSendNotification);
    queryLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (queryLabel);

    artistEditor.setTextToShowWhenEmpty ("Artist", JamStudioTheme::getColours().textSecondary);
    artistEditor.setText (metadata.artist, juce::dontSendNotification);
    addAndMakeVisible (artistEditor);

    titleEditor.setTextToShowWhenEmpty ("Title", JamStudioTheme::getColours().textSecondary);
    titleEditor.setText (metadata.title, juce::dontSendNotification);
    addAndMakeVisible (titleEditor);

    searchButton.onClick = [this] { runSearch(); };
    addAndMakeVisible (searchButton);

    resultsList.setModel (this);
    resultsList.setRowHeight (28);
    addAndMakeVisible (resultsList);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    applyButton.onClick = [this] { applySelection(); };
    addAndMakeVisible (applyButton);

    cancelButton.onClick = [this] { dismiss(); };
    addAndMakeVisible (cancelButton);

    setSize (640, 480);
    runSearch();
}

void OnlineLyricsDialog::show (juce::Component* parent,
                               jamstudio::notation::SongMetadata metadata,
                               LyricsChosenCallback onChosen)
{
    auto* dialog = new OnlineLyricsDialog (std::move (metadata), std::move (onChosen));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Online Lyrics";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog);
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = false;
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.launchAsync();
}

void OnlineLyricsDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void OnlineLyricsDialog::resized()
{
    auto bounds = getLocalBounds().reduced (14);
    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (4);
    queryLabel.setBounds (bounds.removeFromTop (36));
    bounds.removeFromTop (6);

    auto searchRow = bounds.removeFromTop (30);
    artistEditor.setBounds (searchRow.removeFromLeft (searchRow.getWidth() / 3).reduced (0, 1));
    searchRow.removeFromLeft (6);
    titleEditor.setBounds (searchRow.removeFromLeft (searchRow.getWidth() * 2 / 3).reduced (0, 1));
    searchRow.removeFromLeft (6);
    searchButton.setBounds (searchRow.reduced (0, 1));
    bounds.removeFromTop (8);

    auto buttons = bounds.removeFromBottom (34);
    cancelButton.setBounds (buttons.removeFromRight (100));
    buttons.removeFromRight (8);
    applyButton.setBounds (buttons.removeFromRight (120));
    bounds.removeFromBottom (6);
    statusLabel.setBounds (bounds.removeFromBottom (22));
    bounds.removeFromBottom (4);
    resultsList.setBounds (bounds);
}

int OnlineLyricsDialog::getNumRows()
{
    return results.size();
}

void OnlineLyricsDialog::paintListBoxItem (const int rowNumber, juce::Graphics& g,
                                           const int width, const int height, const bool rowIsSelected)
{
    const auto colours = JamStudioTheme::getColours();

    if (rowIsSelected)
        g.fillAll (colours.accent.withAlpha (0.25f));
    else if (rowNumber % 2 == 0)
        g.fillAll (colours.trackBackground.withAlpha (0.35f));

    if (! juce::isPositiveAndBelow (rowNumber, results.size()))
        return;

    const auto& item = results.getReference (rowNumber);
    g.setColour (colours.text);
    g.setFont (juce::FontOptions (13.0f));
    g.drawText (item.displayLine(), 8, 0, width - 16, height, juce::Justification::centredLeft);
}

void OnlineLyricsDialog::listBoxItemDoubleClicked (const int row, const juce::MouseEvent&)
{
    juce::ignoreUnused (row);
    applySelection();
}

void OnlineLyricsDialog::setBusy (const bool isBusy, const juce::String& status)
{
    busy = isBusy;
    searchButton.setEnabled (! busy);
    applyButton.setEnabled (! busy && resultsList.getSelectedRow() >= 0);
    statusLabel.setText (status, juce::dontSendNotification);
}

void OnlineLyricsDialog::runSearch()
{
    metadata.artist = artistEditor.getText().trim();
    metadata.title = titleEditor.getText().trim();

    if (! metadata.hasSearchableFields())
    {
        setBusy (false, "Enter an artist and/or title to search.");
        return;
    }

    setBusy (true, "Searching lrclib.net for \"" + metadata.displayLabel() + "\"...");
    results.clear();
    resultsList.updateContent();

    client.searchAsync (metadata, [safe = juce::Component::SafePointer<OnlineLyricsDialog> (this)]
                                  (juce::Array<jamstudio::notation::OnlineLyricsCandidate> found,
                                   juce::String error)
    {
        if (safe == nullptr)
            return;

        safe->results = std::move (found);
        safe->resultsList.updateContent();

        if (safe->results.isEmpty())
        {
            safe->setBusy (false, error.isNotEmpty() ? error : "No matches found.");
            return;
        }

        safe->resultsList.selectRow (0);
        safe->setBusy (false, juce::String (safe->results.size()) + " match(es). Pick one and click Use Selected.");
        safe->applyButton.setEnabled (true);
    });
}

void OnlineLyricsDialog::applySelection()
{
    const auto row = resultsList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, results.size()))
    {
        setBusy (false, "Select a lyrics match first.");
        return;
    }

    const auto candidate = results.getReference (row);
    setBusy (true, "Downloading synced lyrics...");

    // Always fetch by id so we get full syncedLyrics text.
    if (candidate.id > 0)
    {
        client.fetchByIdAsync (candidate.id,
                               [safe = juce::Component::SafePointer<OnlineLyricsDialog> (this)]
                               (jamstudio::notation::LyricsTrack lyrics, juce::String error)
        {
            if (safe == nullptr)
                return;

            if (error.isNotEmpty() || lyrics.isEmpty())
            {
                safe->setBusy (false, error.isNotEmpty() ? error : "Download failed.");
                return;
            }

            if (safe->onChosen != nullptr)
                safe->onChosen (std::move (lyrics));

            safe->dismiss();
        });
        return;
    }

    // Fallback: parse whatever came in the search payload
    jamstudio::notation::LyricsTrack lyrics;
    juce::String error;

    if (! jamstudio::notation::OnlineLyricsClient::candidateToLyrics (candidate, lyrics, error))
    {
        setBusy (false, error);
        return;
    }

    if (onChosen != nullptr)
        onChosen (std::move (lyrics));

    dismiss();
}

void OnlineLyricsDialog::dismiss()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (0);
    else
        setVisible (false);
}

} // namespace jamstudio::ui
