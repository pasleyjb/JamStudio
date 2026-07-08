#pragma once

#include "MainComponent.h"

namespace jamstudio::app
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (juce::AudioDeviceManager& deviceManager);

    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace jamstudio::app