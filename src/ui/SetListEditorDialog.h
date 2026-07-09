#pragma once

#include "../performance/SetListData.h"
#include "../performance/SetListManager.h"

namespace jamstudio::ui
{

/** Build a live set from .jamstudio projects and stage stem mix prefs. */
class SetListEditorDialog : public juce::Component
{
public:
    using StartCallback = std::function<void (jamstudio::performance::SetList list)>;

    SetListEditorDialog();

    void setStartCallback (StartCallback cb);
    void loadInitialSetList (const jamstudio::performance::SetList& list);

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* centreAround, StartCallback onStart);

private:
    class AvailableListModel;
    class SetListModel;

    void refreshAvailable();
    void refreshSetList();
    void addSelectedProject();
    void removeSelectedSong();
    void moveSong (int delta);
    void applyDefaultMixToSelected();
    void editSelectedStemPrefs();
    void saveSetList();
    void startPerformance();

    jamstudio::performance::SetList setList;
    juce::Array<juce::File> availableProjects;

    juce::Label titleLabel;
    juce::Label setNameLabel { {}, "Set name" };
    juce::TextEditor setNameEditor;
    juce::Label availableLabel { {}, "JamStudio projects" };
    juce::Label setLabel { {}, "Set list (order = show order)" };
    juce::Label mixHint;

    juce::ListBox availableList;
    juce::ListBox setListBox;
    std::unique_ptr<AvailableListModel> availableModel;
    std::unique_ptr<SetListModel> setModel;

    juce::TextButton addButton { "Add →" };
    juce::TextButton removeButton { "Remove" };
    juce::TextButton upButton { "Move Up" };
    juce::TextButton downButton { "Move Down" };
    juce::TextButton applyDefaultMixButton { "Apply lead-guitar+singer mix to selected" };
    juce::TextButton editMixButton { "Edit stem mix…" };
    juce::TextButton saveButton { "Save set list" };
    juce::TextButton startButton { "Start Performance" };
    juce::TextButton cancelButton { "Cancel" };

    StartCallback onStart;
};

} // namespace jamstudio::ui
