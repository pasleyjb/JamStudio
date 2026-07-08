#pragma once

#include "../notation/Score.h"
#include "IndicatorButton.h"

namespace jamstudio::ui
{

/** Title and tab/sheet mode toggle above the notation viewport. */
class NotationHeaderBar : public juce::Component
{
public:
    using ModeChangedCallback = std::function<void (jamstudio::notation::NotationMode)>;

    NotationHeaderBar();

    void setTitle (const juce::String& title);
    void setNotationMode (jamstudio::notation::NotationMode mode);
    void setModeChangedCallback (ModeChangedCallback callback);
    void setHasScore (bool hasScore);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void updateModeButtons();
    void notifyModeChanged (jamstudio::notation::NotationMode mode);

    juce::Label titleLabel;
    IndicatorButton tabButton { "tabView", "Tab" };
    IndicatorButton sheetButton { "sheetView", "Sheet" };
    jamstudio::notation::NotationMode currentMode = jamstudio::notation::NotationMode::standard;
    ModeChangedCallback onModeChanged;
};

} // namespace jamstudio::ui