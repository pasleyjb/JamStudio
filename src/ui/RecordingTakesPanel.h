#pragma once

#include "../audio/RecordingTakeManager.h"
#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Side panel listing recording takes for review / re-load to mixer. */
class RecordingTakesPanel : public juce::Component,
                            private juce::ListBoxModel
{
public:
    using LoadTakeCallback = std::function<void (const jamstudio::audio::RecordingTakeManager::Take& take)>;
    using VoidCallback = std::function<void()>;

    RecordingTakesPanel();

    void setTakeManager (const jamstudio::audio::RecordingTakeManager* manager);
    void setLoadTakeCallback (LoadTakeCallback cb);
    void setOpenExternalCallback (VoidCallback cb);
    void setImportTakeCallback (VoidCallback cb);
    void setPreferredRecorderName (const juce::String& name);
    void refresh();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void loadSelected();

    const jamstudio::audio::RecordingTakeManager* takeManager = nullptr;
    LoadTakeCallback onLoadTake;
    VoidCallback onOpenExternal;
    VoidCallback onImportTake;

    juce::Label title { {}, "RECORDING" };
    juce::Label hint { {}, "Use external DAW for amp sims / plugins" };
    juce::TextButton openExternalButton { "Open Audacity" };
    juce::TextButton importButton { "Import Take..." };
    juce::ListBox list;
    juce::TextButton loadButton { "Load to Mixer" };
    juce::Label emptyLabel { {}, "No takes yet — record in external app, then Import" };
};

} // namespace jamstudio::ui
