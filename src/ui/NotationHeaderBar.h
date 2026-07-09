#pragma once

#include "../notation/Score.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Title, part picker, and tab/sheet mode toggles above the notation viewport. */
class NotationHeaderBar : public juce::Component
{
public:
    using ModeChangedCallback = std::function<void (jamstudio::notation::NotationMode)>;
    using PartChangedCallback = std::function<void (int partIndex)>;
    using FullPageCallback = std::function<void()>;

    NotationHeaderBar();

    void setTitle (const juce::String& title);
    void setNotationMode (jamstudio::notation::NotationMode mode);
    void setModeChangedCallback (ModeChangedCallback callback);
    void setPartChangedCallback (PartChangedCallback callback);
    void setFullPageCallback (FullPageCallback callback);
    void setHasScore (bool hasScore);
    void setParts (const juce::StringArray& partNames, int activePartIndex);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updateModeButtons();
    void handleTabButton();
    void handleSheetButton();

    juce::Label titleLabel;
    juce::Label partLabel { {}, "Part" };
    juce::ComboBox partSelector;
    IndicatorButton fullPageButton { "fullPage", "Full Page" };
    IndicatorButton tabButton { "tabView", "Tab" };
    IndicatorButton sheetButton { "sheetView", "Sheet" };
    jamstudio::notation::NotationMode currentMode = jamstudio::notation::NotationMode::standard;
    ModeChangedCallback onModeChanged;
    PartChangedCallback onPartChanged;
    FullPageCallback onFullPage;
};

} // namespace jamstudio::ui