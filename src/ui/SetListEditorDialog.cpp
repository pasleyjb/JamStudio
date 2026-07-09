#include "SetListEditorDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

namespace
{
class StemPrefEditor : public juce::Component
{
public:
    explicit StemPrefEditor (juce::Array<jamstudio::performance::StemMixPref> prefs)
        : working (std::move (prefs))
    {
        if (working.isEmpty())
            working = jamstudio::performance::SetList::leadGuitarSingerDefaults();

        title.setText ("Stage mix for this song (by stem name)", juce::dontSendNotification);
        title.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (title);

        for (int i = 0; i < working.size(); ++i)
        {
            auto row = std::make_unique<Row>();
            row->name.setText (working.getReference (i).stemName, juce::dontSendNotification);
            row->mute.setButtonText ("Mute");
            row->mute.setToggleState (working.getReference (i).muted, juce::dontSendNotification);
            row->volume.setRange (0.0, 1.0, 0.01);
            row->volume.setValue (working.getReference (i).volume, juce::dontSendNotification);
            row->volume.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
            addAndMakeVisible (row->name);
            addAndMakeVisible (row->mute);
            addAndMakeVisible (row->volume);
            rows.add (std::move (row));
        }

        ok.setButtonText ("OK");
        cancel.setButtonText ("Cancel");
        addAndMakeVisible (ok);
        addAndMakeVisible (cancel);
        setSize (420, 80 + working.size() * 36 + 50);
    }

    juce::Array<jamstudio::performance::StemMixPref> working;
    juce::Label title;
    juce::TextButton ok, cancel;

    struct Row
    {
        juce::Label name;
        juce::ToggleButton mute;
        juce::Slider volume { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    };

    juce::OwnedArray<Row> rows;

    void resized() override
    {
        auto a = getLocalBounds().reduced (12);
        title.setBounds (a.removeFromTop (28));
        a.removeFromTop (8);
        auto buttons = a.removeFromBottom (36);
        ok.setBounds (buttons.removeFromRight (90).reduced (4));
        cancel.setBounds (buttons.removeFromRight (90).reduced (4));

        for (auto* row : rows)
        {
            auto line = a.removeFromTop (32);
            row->name.setBounds (line.removeFromLeft (90));
            row->mute.setBounds (line.removeFromLeft (70));
            row->volume.setBounds (line.reduced (4, 4));
        }
    }

    juce::Array<jamstudio::performance::StemMixPref> collect() const
    {
        juce::Array<jamstudio::performance::StemMixPref> out;

        for (int i = 0; i < rows.size(); ++i)
        {
            jamstudio::performance::StemMixPref p;
            p.stemName = rows[i]->name.getText();
            p.muted = rows[i]->mute.getToggleState();
            p.volume = static_cast<float> (rows[i]->volume.getValue());
            p.solo = false;
            out.add (p);
        }

        return out;
    }
};
} // namespace

//==============================================================================
class SetListEditorDialog::AvailableListModel : public juce::ListBoxModel
{
public:
    explicit AvailableListModel (SetListEditorDialog& o) : owner (o) {}

    int getNumRows() override { return owner.availableProjects.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (selected)
            g.fillAll (JamStudioTheme::getColours().accent.withAlpha (0.25f));

        if (! juce::isPositiveAndBelow (row, owner.availableProjects.size()))
            return;

        g.setColour (JamStudioTheme::getColours().text);
        g.setFont (juce::FontOptions (13.0f));
        g.drawText (owner.availableProjects.getReference (row).getFileNameWithoutExtension(),
                    8, 0, width - 12, height, juce::Justification::centredLeft);
    }

    void listBoxItemDoubleClicked (int, const juce::MouseEvent&) override
    {
        owner.addSelectedProject();
    }

    SetListEditorDialog& owner;
};

class SetListEditorDialog::SetListModel : public juce::ListBoxModel
{
public:
    explicit SetListModel (SetListEditorDialog& o) : owner (o) {}

    int getNumRows() override { return owner.setList.songs.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (selected)
            g.fillAll (JamStudioTheme::getColours().accent.withAlpha (0.25f));

        if (! juce::isPositiveAndBelow (row, owner.setList.songs.size()))
            return;

        const auto& song = owner.setList.songs.getReference (row);
        g.setColour (JamStudioTheme::getColours().text);
        g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
        g.drawText (juce::String (row + 1) + ". " + song.displayName,
                    8, 0, width - 12, height / 2 + 2, juce::Justification::centredLeft);

        g.setColour (JamStudioTheme::getColours().textSecondary);
        g.setFont (juce::FontOptions (11.0f));
        const auto mixNote = song.stemPrefs.isEmpty()
                                 ? "uses default stage mix"
                                 : (juce::String (song.stemPrefs.size()) + " stem prefs");
        g.drawText (mixNote, 24, height / 2 - 2, width - 28, height / 2, juce::Justification::centredLeft);
    }

    SetListEditorDialog& owner;
};

//==============================================================================
SetListEditorDialog::SetListEditorDialog()
{
    setList.defaultStemPrefs = jamstudio::performance::SetList::leadGuitarSingerDefaults();

    titleLabel.setText ("Performance — build your set list", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    addAndMakeVisible (setNameLabel);
    setNameEditor.setText (setList.name);
    addAndMakeVisible (setNameEditor);

    mixHint.setText ("Default stage mix (lead guitar + singer): Guitar down, Vocals mute, Drums/Bass full. "
                     "Edit per song after adding projects.",
                     juce::dontSendNotification);
    mixHint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (mixHint);

    availableLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    setLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    addAndMakeVisible (availableLabel);
    addAndMakeVisible (setLabel);

    availableModel = std::make_unique<AvailableListModel> (*this);
    setModel = std::make_unique<SetListModel> (*this);
    availableList.setModel (availableModel.get());
    setListBox.setModel (setModel.get());
    availableList.setRowHeight (26);
    setListBox.setRowHeight (40);
    addAndMakeVisible (availableList);
    addAndMakeVisible (setListBox);

    addButton.onClick = [this] { addSelectedProject(); };
    removeButton.onClick = [this] { removeSelectedSong(); };
    upButton.onClick = [this] { moveSong (-1); };
    downButton.onClick = [this] { moveSong (1); };
    applyDefaultMixButton.onClick = [this] { applyDefaultMixToSelected(); };
    editMixButton.onClick = [this] { editSelectedStemPrefs(); };
    saveButton.onClick = [this] { saveSetList(); };
    startButton.onClick = [this] { startPerformance(); };
    cancelButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };

    for (auto* b : { &addButton, &removeButton, &upButton, &downButton,
                     &applyDefaultMixButton, &editMixButton, &saveButton, &startButton, &cancelButton })
        addAndMakeVisible (*b);

    startButton.setColour (juce::TextButton::buttonColourId,
                           JamStudioTheme::getColours().accent.darker (0.1f));

    refreshAvailable();

    juce::String err;
    jamstudio::performance::SetList loaded;

    if (jamstudio::performance::SetListManager::loadSetList (
            jamstudio::performance::SetListManager::defaultSetListFile(), loaded, err))
        loadInitialSetList (loaded);

    setSize (780, 560);
}

void SetListEditorDialog::setStartCallback (StartCallback cb)
{
    onStart = std::move (cb);
}

void SetListEditorDialog::loadInitialSetList (const jamstudio::performance::SetList& list)
{
    setList = list;
    setNameEditor.setText (setList.name, false);
    refreshSetList();
}

void SetListEditorDialog::refreshAvailable()
{
    availableProjects = jamstudio::performance::SetListManager::listAvailableProjects();
    availableList.updateContent();
}

void SetListEditorDialog::refreshSetList()
{
    setListBox.updateContent();
    setListBox.repaint();
}

void SetListEditorDialog::addSelectedProject()
{
    const auto row = availableList.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, availableProjects.size()))
        return;

    const auto file = availableProjects.getReference (row);
    jamstudio::performance::SetListSong song;
    song.projectPath = file.getFullPathName();
    song.displayName = file.getFileNameWithoutExtension();
    song.stemPrefs = setList.defaultStemPrefs; // copy stage mix template
    song.showTabs = true;
    song.showLyrics = true;
    song.preferredPartHint = "Guitar";
    setList.songs.add (song);
    refreshSetList();
    setListBox.selectRow (setList.songs.size() - 1);
}

void SetListEditorDialog::removeSelectedSong()
{
    const auto row = setListBox.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, setList.songs.size()))
        return;

    setList.songs.remove (row);
    refreshSetList();
}

void SetListEditorDialog::moveSong (const int delta)
{
    const auto row = setListBox.getSelectedRow();
    const auto target = row + delta;

    if (! juce::isPositiveAndBelow (row, setList.songs.size())
        || ! juce::isPositiveAndBelow (target, setList.songs.size()))
        return;

    setList.songs.swap (row, target);
    refreshSetList();
    setListBox.selectRow (target);
}

void SetListEditorDialog::applyDefaultMixToSelected()
{
    const auto row = setListBox.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, setList.songs.size()))
        return;

    setList.songs.getReference (row).stemPrefs = setList.defaultStemPrefs;
    refreshSetList();
}

void SetListEditorDialog::editSelectedStemPrefs()
{
    const auto row = setListBox.getSelectedRow();

    if (! juce::isPositiveAndBelow (row, setList.songs.size()))
        return;

    auto& song = setList.songs.getReference (row);
    auto* editor = new StemPrefEditor (song.stemPrefs.isEmpty() ? setList.defaultStemPrefs
                                                                : song.stemPrefs);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (editor);
    opts.dialogTitle = "Stem mix — " + song.displayName;
    opts.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = false;
    opts.componentToCentreAround = this;

    editor->ok.onClick = [this, editor, row]
    {
        if (juce::isPositiveAndBelow (row, setList.songs.size()))
        {
            setList.songs.getReference (row).stemPrefs = editor->collect();
            refreshSetList();
        }

        if (auto* dw = editor->findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    };
    editor->cancel.onClick = [editor]
    {
        if (auto* dw = editor->findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };

    opts.launchAsync();
}

void SetListEditorDialog::saveSetList()
{
    setList.name = setNameEditor.getText().trim();

    if (setList.name.isEmpty())
        setList.name = "My Set";

    const auto file = jamstudio::performance::SetListManager::getSetListsDirectory()
                          .getChildFile (juce::File::createLegalFileName (setList.name) + ".setlist");

    if (jamstudio::performance::SetListManager::saveSetList (file, setList))
        jamstudio::performance::SetListManager::saveSetList (
            jamstudio::performance::SetListManager::defaultSetListFile(), setList);
}

void SetListEditorDialog::startPerformance()
{
    setList.name = setNameEditor.getText().trim();

    if (setList.name.isEmpty())
        setList.name = "My Set";

    if (setList.songs.isEmpty())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                "Set list empty",
                                                "Add at least one JamStudio project to the set list.");
        return;
    }

    saveSetList();

    if (onStart)
        onStart (setList);

    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (1);
}

void SetListEditorDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().windowBackground);
}

void SetListEditorDialog::resized()
{
    auto area = getLocalBounds().reduced (14);
    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto nameRow = area.removeFromTop (28);
    setNameLabel.setBounds (nameRow.removeFromLeft (70));
    setNameEditor.setBounds (nameRow);
    area.removeFromTop (8);
    mixHint.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);

    auto bottom = area.removeFromBottom (44);
    cancelButton.setBounds (bottom.removeFromLeft (100).reduced (2));
    saveButton.setBounds (bottom.removeFromLeft (120).reduced (2));
    startButton.setBounds (bottom.removeFromRight (160).reduced (2));

    auto midButtons = area.removeFromBottom (36);
    applyDefaultMixButton.setBounds (midButtons.removeFromLeft (280).reduced (2));
    editMixButton.setBounds (midButtons.removeFromLeft (140).reduced (2));
    area.removeFromBottom (6);

    auto cols = area;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 40);
    auto mid = cols.removeFromLeft (80);
    auto right = cols;

    availableLabel.setBounds (left.removeFromTop (20));
    availableList.setBounds (left);

    setLabel.setBounds (right.removeFromTop (20));
    setListBox.setBounds (right);

    mid = mid.withSizeKeepingCentre (72, 200);
    addButton.setBounds (mid.removeFromTop (36).reduced (2));
    mid.removeFromTop (8);
    removeButton.setBounds (mid.removeFromTop (36).reduced (2));
    mid.removeFromTop (12);
    upButton.setBounds (mid.removeFromTop (36).reduced (2));
    mid.removeFromTop (8);
    downButton.setBounds (mid.removeFromTop (36).reduced (2));
}

void SetListEditorDialog::show (juce::Component* centreAround, StartCallback onStart)
{
    auto* editor = new SetListEditorDialog();
    editor->setStartCallback (std::move (onStart));

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (editor);
    opts.dialogTitle = "Performance set list";
    opts.dialogBackgroundColour = JamStudioTheme::getColours().windowBackground;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.componentToCentreAround = centreAround;
    opts.launchAsync();
}

} // namespace jamstudio::ui
