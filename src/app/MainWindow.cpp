#include "MainWindow.h"

#include "AppMenuBar.h"
#include "../ui/BrandAssets.h"
#include "../ui/JamStudioTheme.h"

namespace jamstudio::app
{

MainWindow::MainWindow (juce::AudioDeviceManager& deviceManager)
    : DocumentWindow ("JamStudio",
                      jamstudio::ui::JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);

    if (const auto icon = jamstudio::ui::BrandAssets::loadWindowIcon (256); icon.isValid())
        setIcon (icon);

    auto* content = new MainComponent (deviceManager);
    setContentOwned (content, true);

    menuBarModel = std::make_unique<AppMenuBar> (juce::Component::SafePointer<MainComponent> (content));
    setMenuBar (menuBarModel.get());

    setResizable (true, true);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
    toFront (true);

    // Native title bars on this desktop often leave isActiveWindow() false,
    // which disables the menu bar and makes every menu click do nothing.
    if (auto* menu = getMenuBarComponent())
        menu->setEnabled (true);
}

void MainWindow::activeWindowStatusChanged()
{
    DocumentWindow::activeWindowStatusChanged();

    if (auto* menu = getMenuBarComponent())
        menu->setEnabled (true);
}

MainWindow::~MainWindow()
{
    setMenuBar (nullptr);
    menuBarModel.reset();
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace jamstudio::app