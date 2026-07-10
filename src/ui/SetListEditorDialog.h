#pragma once

#include "../performance/SetListData.h"
#include "../performance/SetListManager.h"

namespace jamstudio::ui
{

/** Build a live set from .jamstudio projects, stage mix, and stage media. */
class SetListEditorDialog : public juce::Component
{
public:
    using StartCallback = std::function<void (jamstudio::performance::SetList list)>;

    enum class EditorMode
    {
        performance,     // set list + stem mix
        stageShowBuilder // set list + pinned videos / slideshows
    };

    SetListEditorDialog();

    void setStartCallback (StartCallback cb);
    void setEditorMode (EditorMode mode);
    void loadInitialSetList (const jamstudio::performance::SetList& list);

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* centreAround,
                      StartCallback onStart,
                      EditorMode mode = EditorMode::performance);

private:
    class AvailableListModel;
    class SetListModel;

    void applyEditorModeChrome();
    void refreshAvailable();
    void refreshSetList();
    void addSelectedProject();
    void removeSelectedSong();
    void moveSong (int delta);
    void applyDefaultMixToSelected();
    void editSelectedStemPrefs();
    void assignVideoToSelected();
    void assignSlideshowToSelected();
    void clearStageMediaOnSelected();
    void saveSetList();
    void startPerformance();
    [[nodiscard]] juce::File currentSetListFile() const;

    EditorMode editorMode = EditorMode::performance;
    jamstudio::performance::SetList setList;
    juce::Array<juce::File> availableProjects;
    std::unique_ptr<juce::FileChooser> fileChooser;

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

    juce::TextButton addButton { "Add ->" };
    juce::TextButton removeButton { "Remove" };
    juce::TextButton upButton { "Move Up" };
    juce::TextButton downButton { "Move Down" };
    juce::TextButton applyDefaultMixButton { "Apply lead-guitar+singer mix to selected" };
    juce::TextButton editMixButton { "Edit stem mix..." };
    juce::TextButton assignVideoButton { "Pin video..." };
    juce::TextButton assignSlideshowButton { "Pin slideshow..." };
    juce::TextButton clearMediaButton { "Clear stage media" };
    juce::TextButton saveButton { "Save set list" };
    juce::TextButton startButton { "Start Performance" };
    juce::TextButton cancelButton { "Cancel" };

    StartCallback onStart;
};

} // namespace jamstudio::ui
