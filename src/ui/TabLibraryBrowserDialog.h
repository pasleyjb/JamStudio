#pragma once

#include "../notation/TabLibraryClient.h"

namespace jamstudio::ui
{

/** Search and import guitar tabs from the online JamStudio library. */
class TabLibraryBrowserDialog : public juce::Component
{
public:
    using ImportCallback = std::function<void (const juce::File& downloadedFile,
                                               const jamstudio::notation::TabLibraryEntry& entry)>;

    static void show (juce::Component* parent, ImportCallback onImport);

    explicit TabLibraryBrowserDialog (ImportCallback onImport);

private:
    class ResultsListModel;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void reloadCatalog();
    void updateResults();
    void importSelected();
    [[nodiscard]] const jamstudio::notation::TabLibraryEntry* getSelectedEntry() const;

    ImportCallback importCallback;
    jamstudio::notation::TabLibraryClient libraryClient;
    jamstudio::notation::TabLibraryCatalog catalog;
    juce::Array<jamstudio::notation::TabLibraryEntry> visibleEntries;

    juce::Label titleLabel;
    juce::Label introLabel;
    juce::Label statusLabel;
    juce::TextEditor searchEditor;
    juce::Label searchLabel { {}, "Search" };
    juce::ListBox resultsList;
    std::unique_ptr<ResultsListModel> resultsModel;
    juce::TextButton refreshButton { "Refresh" };
    juce::TextButton importButton { "Import" };
    juce::TextButton cancelButton { "Cancel" };
    juce::Label detailLabel;
};

} // namespace jamstudio::ui