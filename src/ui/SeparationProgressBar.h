#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Shows progress and status text during stem separation. */
class SeparationProgressBar : public juce::Component
{
public:
    SeparationProgressBar();

    void setVisible (bool shouldBeVisible) override;
    void setProgress (float progress, const juce::String& statusText);
    void reset();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    juce::Label statusLabel;
};

} // namespace jamstudio::ui