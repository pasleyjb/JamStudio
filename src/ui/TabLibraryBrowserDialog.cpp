#include "TabLibraryBrowserDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

class TabLibraryBrowserDialog::ResultsListModel : public juce::ListBoxModel
{
public:
    explicit ResultsListModel (TabLibraryBrowserDialog& ownerIn)
        : owner (ownerIn)
    {
    }

    int getNumRows() override
    {
        return owner.visibleEntries.size();
    }

    void paintListBoxItem (const int rowNumber,
                           juce::Graphics& g,
                           const int width,
                           const int height,
                           const bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (rowNumber, owner.visibleEntries.size()))
            return;

        const auto& entry = owner.visibleEntries.getReference (rowNumber);
        const auto colours = JamStudioTheme::getColours();

        if (rowIsSelected)
            g.fillAll (colours.accent.withAlpha (0.18f));

        g.setColour (colours.text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (entry.title, 8, 2, width - 16, 18, juce::Justification::centredLeft, true);

        g.setFont (juce::FontOptions (12.0f));
        g.setColour (colours.textSecondary);
        const auto subtitle = entry.artist.isNotEmpty() ? entry.artist : entry.sourceName;
        g.drawText (subtitle, 8, 20, width - 16, 16, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (const int row, const juce::MouseEvent&) override
    {
        if (row >= 0)
        {
            owner.resultsList.selectRow (row);
            owner.importSelected();
        }
    }

    void selectedRowsChanged (const int lastRowSelected) override
    {
        owner.detailLabel.setText (lastRowSelected >= 0 && lastRowSelected < owner.visibleEntries.size()
                                     ? owner.visibleEntries.getReference (lastRowSelected).description
                                     : juce::String(),
                                 juce::dontSendNotification);
    }

private:
    TabLibraryBrowserDialog& owner;
};

TabLibraryBrowserDialog::TabLibraryBrowserDialog (ImportCallback onImportIn)
    : importCallback (std::move (onImportIn))
{
    resultsModel = std::make_unique<ResultsListModel> (*this);
    resultsList.setModel (resultsModel.get());

    titleLabel.setText ("Browse Tab Library", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setText ("Search the online JamStudio tab catalog and import MusicXML scores directly into your project.",
                        juce::dontSendNotification);
    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    searchEditor.setTextToShowWhenEmpty ("Song title, artist, or tag...", juce::Colours::grey);
    searchEditor.onTextChange = [this] { updateResults(); };
    searchEditor.onReturnKey = [this] { updateResults(); };
    addAndMakeVisible (searchEditor);
    addAndMakeVisible (searchLabel);

    resultsList.setRowHeight (42);
    addAndMakeVisible (resultsList);

    detailLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (detailLabel);

    refreshButton.onClick = [this] { reloadCatalog(); };
    addAndMakeVisible (refreshButton);

    importButton.onClick = [this] { importSelected(); };
    addAndMakeVisible (importButton);

    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (cancelButton);

    setSize (640, 520);
    reloadCatalog();
}

void TabLibraryBrowserDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void TabLibraryBrowserDialog::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (8);
    introLabel.setBounds (bounds.removeFromTop (40));
    bounds.removeFromTop (8);
    statusLabel.setBounds (bounds.removeFromTop (20));
    bounds.removeFromTop (8);

    auto searchRow = bounds.removeFromTop (28);
    searchLabel.setBounds (searchRow.removeFromLeft (56));
    searchEditor.setBounds (searchRow);

    bounds.removeFromTop (8);
    resultsList.setBounds (bounds.removeFromTop (220));
    bounds.removeFromTop (8);
    detailLabel.setBounds (bounds.removeFromTop (48));
    bounds.removeFromTop (12);

    auto buttonRow = bounds.removeFromTop (30);
    cancelButton.setBounds (buttonRow.removeFromRight (90));
    buttonRow.removeFromRight (8);
    importButton.setBounds (buttonRow.removeFromRight (90));
    buttonRow.removeFromRight (8);
    refreshButton.setBounds (buttonRow.removeFromRight (90));
}

void TabLibraryBrowserDialog::reloadCatalog()
{
    statusLabel.setText ("Loading tab library...", juce::dontSendNotification);
    importButton.setEnabled (false);
    refreshButton.setEnabled (false);
    searchEditor.setEnabled (false);

    libraryClient.fetchCatalogAsync ([this] (const jamstudio::notation::TabLibraryFetchResult& result)
    {
        refreshButton.setEnabled (true);
        searchEditor.setEnabled (true);

        if (! result.success)
        {
            statusLabel.setText (result.errorMessage, juce::dontSendNotification);
            visibleEntries.clear();
            resultsList.updateContent();
            importButton.setEnabled (false);
            return;
        }

        catalog = result.catalog;

        const auto sourceHint = catalog.loadedFromNetwork ? "online catalog" : "bundled catalog";
        statusLabel.setText ("Loaded " + juce::String (catalog.entries.size()) + " scores from " + sourceHint + ".",
                             juce::dontSendNotification);
        updateResults();
    });
}

void TabLibraryBrowserDialog::updateResults()
{
    visibleEntries = jamstudio::notation::TabLibraryClient::search (catalog, searchEditor.getText());
    resultsList.updateContent();

    if (visibleEntries.isEmpty())
    {
        importButton.setEnabled (false);
        detailLabel.setText (catalog.entries.isEmpty() ? juce::String() : "No scores matched your search.",
                            juce::dontSendNotification);
        return;
    }

    resultsList.selectRow (0);
    importButton.setEnabled (true);
}

const jamstudio::notation::TabLibraryEntry* TabLibraryBrowserDialog::getSelectedEntry() const
{
    const auto selectedRow = resultsList.getSelectedRow();

    if (juce::isPositiveAndBelow (selectedRow, visibleEntries.size()))
        return &visibleEntries.getReference (selectedRow);

    return nullptr;
}

void TabLibraryBrowserDialog::importSelected()
{
    const auto* entry = getSelectedEntry();

    if (entry == nullptr)
        return;

    statusLabel.setText ("Downloading \"" + entry->title + "\"...", juce::dontSendNotification);
    importButton.setEnabled (false);
    refreshButton.setEnabled (false);
    searchEditor.setEnabled (false);

    libraryClient.downloadScoreAsync (*entry, catalog,
        [this, entryCopy = *entry] (const jamstudio::notation::TabLibraryDownloadResult& result)
        {
            refreshButton.setEnabled (true);
            searchEditor.setEnabled (true);
            importButton.setEnabled (true);

            if (! result.success)
            {
                statusLabel.setText (result.errorMessage, juce::dontSendNotification);
                return;
            }

            if (importCallback)
                importCallback (result.downloadedFile, entryCopy);

            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (1);
        });
}

void TabLibraryBrowserDialog::show (juce::Component* parent, ImportCallback onImport)
{
    auto dialog = std::make_unique<TabLibraryBrowserDialog> (std::move (onImport));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Tab Library";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog.release());
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = true;
    options.resizable = true;

    options.launchAsync();
}

} // namespace jamstudio::ui