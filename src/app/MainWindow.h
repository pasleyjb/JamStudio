#pragma once

#include "MainComponent.h"

#include <memory>

namespace jamstudio::app
{

class AppMenuBar;

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (juce::AudioDeviceManager& deviceManager);
    ~MainWindow() override;

    void closeButtonPressed() override;

private:
    std::unique_ptr<AppMenuBar> menuBarModel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace jamstudio::app