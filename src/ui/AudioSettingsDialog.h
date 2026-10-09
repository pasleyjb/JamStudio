#pragma once

#include "../audio/AudioInterfaceManager.h"

namespace jamstudio::ui
{

/** Configure plug-and-play / manual audio interface routing. */
class AudioSettingsDialog : public juce::Component,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    static void show (juce::Component* parent, jamstudio::audio::AudioInterfaceManager& manager);

private:
    explicit AudioSettingsDialog (jamstudio::audio::AudioInterfaceManager& manager);
    ~AudioSettingsDialog() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void timerCallback() override;

    void refreshDeviceLists();
    void refreshStatus();
    void applyFromUi();
    void dismiss();

    jamstudio::audio::AudioInterfaceManager& manager;

    juce::Label titleLabel;
    juce::Label introLabel;

    juce::Label modeLabel { {}, "Routing mode" };
    juce::ComboBox modeBox;

    juce::ToggleButton preferComputerToggle {
        "When a multi-input interface is connected, monitor on computer speakers (not the interface)"
    };

    juce::Label destinationsHeading { {}, "Where each mix goes" };
    juce::Label fohLabel { {}, "FOH (house)" };
    juce::ComboBox fohBox;
    juce::Label monALabel { {}, "Mon A" };
    juce::ComboBox monABox;
    juce::Label monBLabel { {}, "Mon B" };
    juce::ComboBox monBBox;

    juce::Label inputLabel { {}, "Input device" };
    juce::ComboBox inputBox;
    juce::Label outputLabel { {}, "Output device" };
    juce::ComboBox outputBox;

    juce::Label maxInLabel { {}, "Open input channels" };
    juce::ComboBox maxInBox;
    juce::Label maxOutLabel { {}, "Open output channels" };
    juce::ComboBox maxOutBox;

    juce::Label statusHeading { {}, "Active routing" };
    juce::Label statusLabel;
    juce::Label jackNoteLabel;

    juce::TextButton rescanButton { "Rescan devices" };
    juce::TextButton applyButton { "Apply" };
    juce::TextButton closeButton { "Close" };

    juce::StringArray inputNames;
    juce::StringArray outputNames;
    juce::StringArray fohSinkNames;
    juce::StringArray monASinkNames;
    juce::StringArray monBSinkNames;

    void fillBusBox (juce::ComboBox& box, juce::StringArray& names, const juce::String& selectedSink);
    [[nodiscard]] juce::String sinkFromBox (const juce::ComboBox& box, const juce::StringArray& names) const;
};

} // namespace jamstudio::ui
