#include "MidiControlDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

MidiControlDialog::MidiControlDialog (jamstudio::midi::MidiControlSurface& midiSurface)
    : surface (midiSurface)
{
    titleLabel.setText ("MIDI Control Surface", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setText (
        "Use any class-compliant USB mixer / DAW controller (nanoKONTROL, APC Mini, X-Touch Mini, "
        "Launch Control, MCU mode, etc.). Pick a profile or MIDI Learn each control.\n"
        "No hardware? Create a virtual MIDI port and send CCs (see tip at bottom).",
        juce::dontSendNotification);
    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    enabledToggle.setToggleState (surface.isEnabled(), juce::dontSendNotification);
    enabledToggle.onClick = [this]
    {
        surface.setEnabled (enabledToggle.getToggleState());
    };
    addAndMakeVisible (enabledToggle);

    addAndMakeVisible (deviceLabel);
    deviceBox.onChange = [this]
    {
        if (deviceBox.getSelectedId() <= 1)
            surface.setInputDeviceIdentifier ({});
        else
        {
            const auto idx = deviceBox.getSelectedId() - 2;
            if (juce::isPositiveAndBelow (idx, deviceIdentifiers.size()))
                surface.setInputDeviceIdentifier (deviceIdentifiers[idx]);
        }
    };
    addAndMakeVisible (deviceBox);

    addAndMakeVisible (profileLabel);
    profileBox.onChange = [this] { applySelectedProfile(); };
    addAndMakeVisible (profileBox);

    addAndMakeVisible (targetLabel);
    addAndMakeVisible (targetBox);

    learnButton.onClick = [this] { startLearnForSelectedTarget(); };
    addAndMakeVisible (learnButton);

    clearLearnButton.onClick = [this]
    {
        surface.clearCustomBindings();
        surface.setUseCustomProfile (false);
        refreshProfileList();
        refreshBindingsList();
        activityLabel.setText ("Custom map cleared - using built-in profile.", juce::dontSendNotification);
    };
    addAndMakeVisible (clearLearnButton);

    testButton.onClick = [this] { sendTestCc(); };
    addAndMakeVisible (testButton);

    refreshButton.onClick = [this]
    {
        surface.refreshMidiDevices();
        refreshDevices();
        activityLabel.setText ("Device list refreshed.", juce::dontSendNotification);
    };
    addAndMakeVisible (refreshButton);

    activityLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().accent);
    addAndMakeVisible (activityLabel);

    addAndMakeVisible (bindingsLabel);
    bindingsList.setModel (this);
    bindingsList.setRowHeight (22);
    addAndMakeVisible (bindingsList);

    closeButton.onClick = [this] { dismiss(); };
    addAndMakeVisible (closeButton);

    profiles = jamstudio::midi::MidiMappingProfile::builtInProfiles();
    const auto names = jamstudio::midi::allMidiTargetNames();
    for (int i = 0; i < names.size(); ++i)
        targetBox.addItem (names[i], i + 1);
    targetBox.setSelectedItemIndex (static_cast<int> (jamstudio::midi::MidiTarget::stemVolume0),
                                    juce::dontSendNotification);

    refreshDevices();
    refreshProfileList();
    refreshBindingsList();

    setSize (640, 560);
    startTimerHz (8);
}

void MidiControlDialog::show (juce::Component* parent, jamstudio::midi::MidiControlSurface& surface)
{
    auto* dialog = new MidiControlDialog (surface);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "MIDI Control Surface";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog);
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = false;
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.launchAsync();
}

void MidiControlDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void MidiControlDialog::resized()
{
    auto bounds = getLocalBounds().reduced (14);
    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (4);
    introLabel.setBounds (bounds.removeFromTop (56));
    bounds.removeFromTop (8);
    enabledToggle.setBounds (bounds.removeFromTop (24));
    bounds.removeFromTop (8);

    auto row = bounds.removeFromTop (28);
    deviceLabel.setBounds (row.removeFromLeft (90));
    deviceBox.setBounds (row);
    bounds.removeFromTop (6);

    row = bounds.removeFromTop (28);
    profileLabel.setBounds (row.removeFromLeft (120));
    profileBox.setBounds (row);
    bounds.removeFromTop (6);

    row = bounds.removeFromTop (28);
    targetLabel.setBounds (row.removeFromLeft (90));
    targetBox.setBounds (row.removeFromLeft (row.getWidth() / 2));
    row.removeFromLeft (6);
    learnButton.setBounds (row.removeFromLeft (110));
    row.removeFromLeft (6);
    clearLearnButton.setBounds (row);
    bounds.removeFromTop (8);

    row = bounds.removeFromTop (28);
    testButton.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (8);
    refreshButton.setBounds (row.removeFromLeft (130));
    bounds.removeFromTop (6);

    activityLabel.setBounds (bounds.removeFromTop (22));
    bounds.removeFromTop (6);
    bindingsLabel.setBounds (bounds.removeFromTop (18));
    bounds.removeFromTop (4);

    auto bottom = bounds.removeFromBottom (34);
    closeButton.setBounds (bottom.removeFromRight (100));
    bindingsList.setBounds (bounds);
}

void MidiControlDialog::timerCallback()
{
    const auto msg = surface.getLastMessageDescription();
    if (msg.isNotEmpty())
        activityLabel.setText ((surface.isLearning() ? "LEARNING - " : "Last MIDI: ") + msg,
                               juce::dontSendNotification);
}

void MidiControlDialog::refreshDevices()
{
    deviceBox.clear (juce::dontSendNotification);
    deviceIdentifiers.clear();
    deviceBox.addItem ("All MIDI inputs", 1);

    const auto devices = juce::MidiInput::getAvailableDevices();
    const auto current = surface.getInputDeviceIdentifier();
    auto selected = 1;

    for (int i = 0; i < devices.size(); ++i)
    {
        const auto& d = devices.getReference (i);
        deviceIdentifiers.add (d.identifier);
        deviceBox.addItem (d.name, i + 2);
        if (current == d.identifier)
            selected = i + 2;
    }

    if (current.isEmpty() || current == "all")
        selected = 1;

    deviceBox.setSelectedId (selected, juce::dontSendNotification);
}

void MidiControlDialog::refreshProfileList()
{
    profileBox.clear (juce::dontSendNotification);
    profiles = jamstudio::midi::MidiMappingProfile::builtInProfiles();

    auto selected = 1;
    const auto activeId = surface.getSettings().useCustomBindings
                              ? juce::String ("custom")
                              : surface.getSettings().profileId;

    for (int i = 0; i < profiles.size(); ++i)
    {
        profileBox.addItem (profiles.getReference (i).name, i + 1);
        if (profiles.getReference (i).id == activeId)
            selected = i + 1;
    }

    profileBox.addItem ("Custom (MIDI Learn)", profiles.size() + 1);
    if (activeId == "custom" || surface.getSettings().useCustomBindings)
        selected = profiles.size() + 1;

    profileBox.setSelectedId (selected, juce::dontSendNotification);
}

void MidiControlDialog::refreshBindingsList()
{
    bindingsList.updateContent();
    bindingsList.repaint();
}

void MidiControlDialog::applySelectedProfile()
{
    const auto idx = profileBox.getSelectedItemIndex();

    if (idx >= 0 && idx < profiles.size())
    {
        surface.setUseCustomProfile (false);
        surface.applyProfile (profiles.getReference (idx));
    }
    else
    {
        surface.setUseCustomProfile (true);
        auto custom = surface.getActiveProfile();
        custom.id = "custom";
        custom.name = "Custom (MIDI Learn)";
        surface.applyProfile (custom);
    }

    refreshBindingsList();
    activityLabel.setText ("Profile: " + profileBox.getText(), juce::dontSendNotification);
}

void MidiControlDialog::startLearnForSelectedTarget()
{
    const auto name = targetBox.getText();
    const auto target = jamstudio::midi::midiTargetFromString (name);

    if (target == jamstudio::midi::MidiTarget::none)
    {
        activityLabel.setText ("Pick a learn target first.", juce::dontSendNotification);
        return;
    }

    surface.setUseCustomProfile (true);
    surface.beginLearn (target);
    refreshProfileList();
    activityLabel.setText ("Move a control to learn: " + name, juce::dontSendNotification);
}

void MidiControlDialog::sendTestCc()
{
    // Simulates a USB fader without hardware - maps to Stem 1 Volume on Generic profile.
    surface.handleIncomingMessageForTest (juce::MidiMessage::controllerEvent (1, 0, 100));
    refreshBindingsList();
    activityLabel.setText ("Injected test CC0=100 (Stem 1 volume on Generic / nanoKONTROL profiles).",
                           juce::dontSendNotification);
}

int MidiControlDialog::getNumRows()
{
    return surface.getActiveProfile().bindings.size();
}

void MidiControlDialog::paintListBoxItem (const int rowNumber, juce::Graphics& g,
                                          const int width, const int height, const bool rowIsSelected)
{
    const auto colours = JamStudioTheme::getColours();
    if (rowIsSelected)
        g.fillAll (colours.accent.withAlpha (0.2f));
    else if (rowNumber % 2 == 0)
        g.fillAll (colours.trackBackground.withAlpha (0.3f));

    const auto profile = surface.getActiveProfile();
    if (! juce::isPositiveAndBelow (rowNumber, profile.bindings.size()))
        return;

    g.setColour (colours.text);
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (profile.bindings.getReference (rowNumber).describe(),
                8, 0, width - 16, height, juce::Justification::centredLeft);
}

void MidiControlDialog::dismiss()
{
    surface.saveSettings();
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (0);
}

} // namespace jamstudio::ui
