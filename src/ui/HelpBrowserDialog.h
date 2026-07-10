#pragma once

#include "HelpCatalog.h"

namespace jamstudio::ui
{

/** Searchable instructions browser (Help → Instructions…). */
class HelpBrowserDialog : public juce::Component,
                          private juce::TextEditor::Listener,
                          private juce::ListBoxModel
{
public:
    HelpBrowserDialog();

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* centreAround,
                      const juce::String& initialQuery = {});

private:
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void refreshResults();
    void showSelectedTopic();

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void selectedRowsChanged (int lastRowSelected) override;

    juce::Label titleLabel;
    juce::Label searchLabel { {}, "Search" };
    juce::TextEditor searchBox;
    juce::Label categoryLabel { {}, "Category" };
    juce::ComboBox categoryBox;
    juce::ListBox resultsList;
    juce::Label detailTitle;
    juce::TextEditor detailBody;
    juce::TextButton closeButton { "Close" };
    juce::Label resultCount;

    juce::Array<HelpTopic> filtered;
};

} // namespace jamstudio::ui
