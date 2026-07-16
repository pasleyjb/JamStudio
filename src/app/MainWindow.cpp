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
    // Stay invisible until size + position are final (avoids a flash at 0,0).
    setVisible (false);
    setUsingNativeTitleBar (true);

    if (const auto icon = jamstudio::ui::BrandAssets::loadWindowIcon (256); icon.isValid())
        setIcon (icon);

    auto* content = new MainComponent (deviceManager);
    setContentOwned (content, true);

    menuBarModel = std::make_unique<AppMenuBar> (juce::Component::SafePointer<MainComponent> (content));
    setMenuBar (menuBarModel.get());

    setResizable (true, true);

    // Prefer a solid default size then centre on the primary display.
    const int w = juce::jmax (1100, content->getWidth());
    const int h = juce::jmax (720, content->getHeight());
    setSize (w, h);
    centreWithSize (w, h);

    setVisible (true);
    toFront (true);
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
