#include "HelpBrowserDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

HelpBrowserDialog::HelpBrowserDialog()
{
    titleLabel.setText ("JamStudio Instructions", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    searchLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (searchLabel);

    searchBox.setTextToShowWhenEmpty ("Search functions… e.g. mixer, stage, dock, stems",
                                      JamStudioTheme::getColours().textSecondary);
    searchBox.addListener (this);
    searchBox.setFont (juce::FontOptions (15.0f));
    addAndMakeVisible (searchBox);

    categoryLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    addAndMakeVisible (categoryLabel);

    categoryBox.addItem ("All categories", 1);
    int id = 2;
    for (const auto& c : HelpCatalog::getCategories())
        categoryBox.addItem (c, id++);
    categoryBox.setSelectedId (1, juce::dontSendNotification);
    categoryBox.onChange = [this] { refreshResults(); };
    addAndMakeVisible (categoryBox);

    resultsList.setModel (this);
    resultsList.setRowHeight (40);
    resultsList.setColour (juce::ListBox::backgroundColourId,
                           JamStudioTheme::getColours().panelBackground);
    addAndMakeVisible (resultsList);

    detailTitle.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    detailTitle.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (detailTitle);

    detailBody.setMultiLine (true);
    detailBody.setReadOnly (true);
    detailBody.setScrollbarsShown (true);
    detailBody.setCaretVisible (false);
    detailBody.setFont (juce::FontOptions (14.0f));
    detailBody.setColour (juce::TextEditor::backgroundColourId,
                          JamStudioTheme::getColours().panelBackground);
    detailBody.setColour (juce::TextEditor::outlineColourId,
                          JamStudioTheme::getColours().border);
    addAndMakeVisible (detailBody);

    resultCount.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (resultCount);

    closeButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (closeButton);

    refreshResults();
    setSize (820, 560);
}

void HelpBrowserDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().windowBackground);
}

void HelpBrowserDialog::resized()
{
    auto a = getLocalBounds().reduced (14);
    titleLabel.setBounds (a.removeFromTop (28));
    a.removeFromTop (8);

    auto searchRow = a.removeFromTop (32);
    searchLabel.setBounds (searchRow.removeFromLeft (56));
    categoryBox.setBounds (searchRow.removeFromRight (180));
    categoryLabel.setBounds (searchRow.removeFromRight (70));
    searchRow.removeFromRight (8);
    searchBox.setBounds (searchRow);
    a.removeFromTop (6);
    resultCount.setBounds (a.removeFromTop (18));
    a.removeFromTop (6);

    auto bottom = a.removeFromBottom (40);
    closeButton.setBounds (bottom.removeFromRight (100).reduced (2));

    auto left = a.removeFromLeft (juce::jmax (220, a.getWidth() * 2 / 5));
    resultsList.setBounds (left);
    a.removeFromLeft (10);

    detailTitle.setBounds (a.removeFromTop (28));
    a.removeFromTop (4);
    detailBody.setBounds (a);
}

void HelpBrowserDialog::textEditorTextChanged (juce::TextEditor&)
{
    refreshResults();
}

void HelpBrowserDialog::textEditorReturnKeyPressed (juce::TextEditor&)
{
    refreshResults();
    if (filtered.size() > 0)
    {
        resultsList.selectRow (0);
        showSelectedTopic();
    }
}

void HelpBrowserDialog::refreshResults()
{
    juce::String cat;
    if (categoryBox.getSelectedId() > 1)
        cat = categoryBox.getText();

    filtered = HelpCatalog::search (searchBox.getText(), cat);
    resultsList.updateContent();
    resultsList.deselectAllRows();
    resultCount.setText (juce::String (filtered.size()) + " topic"
                             + (filtered.size() == 1 ? "" : "s"),
                         juce::dontSendNotification);

    if (filtered.isEmpty())
    {
        detailTitle.setText ("No matches", juce::dontSendNotification);
        detailBody.setText ("Try another search word, or set Category to All categories.");
    }
    else
    {
        resultsList.selectRow (0);
        showSelectedTopic();
    }
}

void HelpBrowserDialog::showSelectedTopic()
{
    const auto row = resultsList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, filtered.size()))
        return;

    const auto& t = filtered.getReference (row);
    detailTitle.setText (t.title + "  ·  " + t.category, juce::dontSendNotification);
    detailBody.setText (t.body);
    detailBody.moveCaretToTop (false);
}

int HelpBrowserDialog::getNumRows()
{
    return filtered.size();
}

void HelpBrowserDialog::paintListBoxItem (const int row,
                                          juce::Graphics& g,
                                          const int width,
                                          const int height,
                                          const bool selected)
{
    if (selected)
        g.fillAll (JamStudioTheme::getColours().accent.withAlpha (0.22f));
    else if (row % 2 == 1)
        g.fillAll (JamStudioTheme::getColours().panelBackground.brighter (0.03f));

    if (! juce::isPositiveAndBelow (row, filtered.size()))
        return;

    const auto& t = filtered.getReference (row);
    g.setColour (JamStudioTheme::getColours().text);
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText (t.title, 10, 2, width - 16, height / 2 + 2, juce::Justification::centredLeft);

    g.setColour (JamStudioTheme::getColours().textSecondary);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (t.category, 10, height / 2 - 2, width - 16, height / 2, juce::Justification::centredLeft);
}

void HelpBrowserDialog::listBoxItemClicked (const int, const juce::MouseEvent&)
{
    showSelectedTopic();
}

void HelpBrowserDialog::selectedRowsChanged (const int)
{
    showSelectedTopic();
}

void HelpBrowserDialog::show (juce::Component* centreAround, const juce::String& initialQuery)
{
    auto* browser = new HelpBrowserDialog();
    if (initialQuery.isNotEmpty())
    {
        browser->searchBox.setText (initialQuery, juce::dontSendNotification);
        browser->refreshResults();
    }

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (browser);
    opts.dialogTitle = "Instructions";
    opts.dialogBackgroundColour = JamStudioTheme::getColours().windowBackground;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.componentToCentreAround = centreAround;
    opts.launchAsync();

    // Focus search after the dialog is up.
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<HelpBrowserDialog> (browser)]
    {
        if (safe != nullptr)
            safe->searchBox.grabKeyboardFocus();
    });
}

} // namespace jamstudio::ui
