#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Shows progress, status text, and optional cancel during AI / separation jobs. */
class SeparationProgressBar : public juce::Component
{
public:
    SeparationProgressBar();

    void setVisible (bool shouldBeVisible) override;
    void setProgress (float progress, const juce::String& statusText);
    void setCancelCallback (std::function<void()> callback);
    void reset();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::ProgressBar progressBar;
    double progressValue = 0.0;
    juce::Label statusLabel;
    juce::TextButton cancelButton { "Cancel" };
    std::function<void()> cancelCallback;
};

} // namespace jamstudio::ui