#include "MainWindow.h"

#include "../ui/JamStudioTheme.h"

namespace jamstudio::app
{

MainWindow::MainWindow (juce::AudioDeviceManager& deviceManager)
    : DocumentWindow ("JamStudio",
                      jamstudio::ui::JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);

    auto* content = new MainComponent (deviceManager);
    setContentOwned (content, true);
    setMenuBar (content);

    setResizable (true, true);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    setMenuBar (nullptr);
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace jamstudio::app