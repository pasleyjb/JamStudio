#include "AppMenuBar.h"

#include "MainComponent.h"

namespace jamstudio::app
{

AppMenuBar::AppMenuBar (juce::Component::SafePointer<MainComponent> target)
    : owner (std::move (target))
{
}

juce::StringArray AppMenuBar::getMenuBarNames()
{
    if (auto* component = owner.getComponent())
        return component->buildMenuBarNames();

    return {};
}

juce::PopupMenu AppMenuBar::getMenuForIndex (const int topLevelMenuIndex, const juce::String& menuName)
{
    if (auto* component = owner.getComponent())
        return component->buildMenuForIndex (topLevelMenuIndex, menuName);

    return {};
}

void AppMenuBar::menuItemSelected (const int menuItemID, const int topLevelMenuIndex)
{
    if (auto* component = owner.getComponent())
        component->handleMenuCommand (menuItemID, topLevelMenuIndex);
}

} // namespace jamstudio::app