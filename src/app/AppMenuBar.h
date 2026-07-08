#pragma once

#include <JuceHeader.h>

namespace jamstudio::app
{

class MainComponent;

/** Menu bar model kept separate from the main content component to avoid lifetime issues. */
class AppMenuBar : public juce::MenuBarModel
{
public:
    explicit AppMenuBar (juce::Component::SafePointer<MainComponent> target);

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

private:
    juce::Component::SafePointer<MainComponent> owner;
};

} // namespace jamstudio::app