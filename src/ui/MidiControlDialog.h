#pragma once

#include "../midi/MidiControlSurface.h"

namespace jamstudio::ui
{

/** Configure USB / class-compliant MIDI controllers for the mixer & transport. */
class MidiControlDialog : public juce::Component,
                          private juce::Timer,
                          private juce::ListBoxModel
{
public:
    static void show (juce::Component* parent, jamstudio::midi::MidiControlSurface& surface);

private:
    explicit MidiControlDialog (jamstudio::midi::MidiControlSurface& surface);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;

    void refreshDevices();
    void refreshProfileList();
    void refreshBindingsList();
    void applySelectedProfile();
    void startLearnForSelectedTarget();
    void sendTestCc();
    void dismiss();

    jamstudio::midi::MidiControlSurface& surface;
    juce::Array<jamstudio::midi::MidiMappingProfile> profiles;
    juce::StringArray deviceIdentifiers;

    juce::Label titleLabel;
    juce::Label introLabel;
    juce::ToggleButton enabledToggle { "Enable MIDI control surface" };
    juce::Label deviceLabel { {}, "MIDI input" };
    juce::ComboBox deviceBox;
    juce::Label profileLabel { {}, "Controller profile" };
    juce::ComboBox profileBox;
    juce::Label targetLabel { {}, "Learn target" };
    juce::ComboBox targetBox;
    juce::TextButton learnButton { "MIDI Learn" };
    juce::TextButton clearLearnButton { "Clear custom map" };
    juce::TextButton testButton { "Send test CC0=100" };
    juce::TextButton refreshButton { "Refresh devices" };
    juce::Label activityLabel;
    juce::Label bindingsLabel { {}, "Active bindings" };
    juce::ListBox bindingsList;
    juce::TextButton closeButton { "Close" };
};

} // namespace jamstudio::ui
