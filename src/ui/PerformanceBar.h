#pragma once

#include "../performance/SetListData.h"

namespace jamstudio::ui
{

/** Stage control strip for Performance mode — next-song foot-pedal target. */
class PerformanceBar : public juce::Component
{
public:
    using TriggerCallback = std::function<void()>;

    PerformanceBar();

    void setSetListInfo (const juce::String& setName,
                         int songIndex,
                         int songCount,
                         const juce::String& songTitle);
    void setPhaseMessage (const juce::String& message);
    void setWaitingForTrigger (bool waiting);
    void setTriggerCallback (TriggerCallback cb);

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Large stage button — also wired as MIDI "Next Song / Foot Pedal". */
    juce::TextButton& getTriggerButton() noexcept { return triggerButton; }

private:
    juce::Label setLabel;
    juce::Label songLabel;
    juce::Label phaseLabel;
    juce::TextButton triggerButton { "NEXT / START" };
    TriggerCallback onTrigger;
    bool waiting = false;
};

} // namespace jamstudio::ui
