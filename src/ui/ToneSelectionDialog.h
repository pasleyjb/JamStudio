#pragma once

#include "../amp/AmpModelLibrary.h"
#include "../amp/AmpProcessor.h"

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Tone Selection Mode - pick a NAM amp, tune gains, load into live monitor. */
class ToneSelectionDialog : public juce::Component,
                            private juce::Timer
{
public:
    using ToneLoadedCallback = std::function<void (const jamstudio::amp::AmpToneInfo& tone)>;

    static void show (juce::Component* parent,
                      jamstudio::amp::AmpProcessor& ampProcessor,
                      ToneLoadedCallback onLoaded = nullptr);

    ToneSelectionDialog (jamstudio::amp::AmpProcessor& ampProcessor,
                         ToneLoadedCallback onLoaded);

    ~ToneSelectionDialog() override;

private:
    class ToneListModel;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void rescanLibrary();
    void updateFilteredList();
    void applySelectedTone();
    void importToneFile();
    void openUserLibraryFolder();
    void refreshControlsFromAmp();
    [[nodiscard]] const jamstudio::amp::AmpToneInfo* getSelectedTone() const;

    jamstudio::amp::AmpProcessor& amp;
    ToneLoadedCallback loadedCallback;
    jamstudio::amp::AmpModelLibrary library;
    juce::Array<jamstudio::amp::AmpToneInfo> visibleTones;

    juce::Label titleLabel { {}, "Tone Selection" };
    juce::Label introLabel;
    juce::Label statusLabel;
    juce::Label searchLabel { {}, "Search" };
    juce::TextEditor searchEditor;
    juce::ListBox toneList;
    std::unique_ptr<ToneListModel> listModel;
    juce::Label detailLabel;

    juce::ToggleButton enabledToggle { "Amp monitoring on" };
    juce::ToggleButton bypassToggle { "Bypass (dry)" };
    juce::Label inputLabel { {}, "Input" };
    juce::Label outputLabel { {}, "Output" };
    juce::Slider inputGain;
    juce::Slider outputGain;
    juce::Label meterLabel;

    juce::TextButton refreshButton { "Refresh" };
    juce::TextButton importButton { "Import .nam..." };
    juce::TextButton openFolderButton { "Open Folder" };
    juce::TextButton useToneButton { "Use This Tone" };
    juce::TextButton closeButton { "Close" };

    std::unique_ptr<juce::FileChooser> fileChooser;
    bool loading = false;
};

} // namespace jamstudio::ui
