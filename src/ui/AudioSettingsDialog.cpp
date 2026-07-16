#include "AudioSettingsDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

void AudioSettingsDialog::show (juce::Component* parent, jamstudio::audio::AudioInterfaceManager& manager)
{
    auto* dialog = new AudioSettingsDialog (manager);

    juce::DialogWindow::LaunchOptions opts;
    opts.content.setOwned (dialog);
    opts.dialogTitle = "Audio Interface";
    opts.dialogBackgroundColour = JamStudioTheme::getColours().windowBackground;
    opts.escapeKeyTriggersCloseButton = true;
    opts.useNativeTitleBar = true;
    opts.resizable = true;
    opts.componentToCentreAround = parent;
    opts.launchAsync();
}

AudioSettingsDialog::AudioSettingsDialog (jamstudio::audio::AudioInterfaceManager& audioManager)
    : manager (audioManager)
{
    const auto& colours = JamStudioTheme::getColours();

    titleLabel.setText ("Audio Interface", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setText (
        "Plug-and-play: Scarlett multi-in (guitar + vocal) + PC speakers for monitor.\n"
        "Linux/PipeWire: enable Multi-input Pro Audio so the Scarlett is ONE device with "
        "many channels (In1=guitar, In2=mic…). HiFi \"Mic1/Mic2\" only allow one jack at a time.\n"
        "Avoid exclusive \"Direct hardware\". MON → All hears every open input.\n"
        "Same device: interface for in + out (stage multi-bus). Manual: pick devices below.",
        juce::dontSendNotification);
    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    addAndMakeVisible (modeLabel);
    modeBox.addItem ("Plug and play (recommended)", 1);
    modeBox.addItem ("Same device (interface in + out)", 2);
    modeBox.addItem ("Manual", 3);
    addAndMakeVisible (modeBox);

    preferComputerToggle.setToggleState (manager.getSettings().preferComputerSpeakersForMonitor,
                                         juce::dontSendNotification);
    addAndMakeVisible (preferComputerToggle);

    preferProAudioToggle.setToggleState (manager.getSettings().preferProAudioProfile,
                                         juce::dontSendNotification);
    preferProAudioToggle.setTooltip (
        "Sets the Scarlett PipeWire profile to pro-audio so all inputs open together. "
        "Turn off only if you need the desktop HiFi Mic1/Mic2 split ports.");
    addAndMakeVisible (preferProAudioToggle);

    addAndMakeVisible (inputLabel);
    addAndMakeVisible (inputBox);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (outputBox);

    addAndMakeVisible (maxInLabel);
    for (int n : { 1, 2, 4, 8, 16, 18 })
        maxInBox.addItem (juce::String (n), n);
    maxInBox.setSelectedId (manager.getSettings().maxInputChannels, juce::dontSendNotification);
    if (maxInBox.getSelectedId() <= 0)
        maxInBox.setSelectedId (8, juce::dontSendNotification);
    addAndMakeVisible (maxInBox);

    addAndMakeVisible (maxOutLabel);
    for (int n : { 2, 4, 6, 8, 10, 12 })
        maxOutBox.addItem (juce::String (n), n);
    maxOutBox.setSelectedId (manager.getSettings().maxOutputChannels, juce::dontSendNotification);
    if (maxOutBox.getSelectedId() <= 0)
        maxOutBox.setSelectedId (6, juce::dontSendNotification);
    addAndMakeVisible (maxOutBox);

    statusHeading.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    addAndMakeVisible (statusHeading);
    statusLabel.setJustificationType (juce::Justification::topLeft);
    statusLabel.setColour (juce::Label::textColourId, colours.accent);
    addAndMakeVisible (statusLabel);

    jackNoteLabel.setText (
        "Jack sense: USB audio interfaces do not tell the OS whether speakers/headphones are "
        "physically plugged into their outputs. JamStudio cannot detect empty Scarlett jacks - "
        "use Plug and play + 'monitor on computer speakers' for that workflow.",
        juce::dontSendNotification);
    jackNoteLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (jackNoteLabel);

    rescanButton.onClick = [this]
    {
        manager.scanHardware();
        refreshDeviceLists();
        manager.rescanAndApply();
        refreshStatus();
    };
    addAndMakeVisible (rescanButton);

    applyButton.onClick = [this] { applyFromUi(); };
    addAndMakeVisible (applyButton);

    closeButton.onClick = [this] { dismiss(); };
    addAndMakeVisible (closeButton);

    const auto mode = manager.getSettings().mode;
    modeBox.setSelectedId (mode == jamstudio::audio::AudioRoutingMode::sameDevice   ? 2
                           : mode == jamstudio::audio::AudioRoutingMode::manual     ? 3
                                                                                    : 1,
                           juce::dontSendNotification);

    modeBox.onChange = [this]
    {
        const bool manual = modeBox.getSelectedId() == 3;
        inputBox.setEnabled (manual);
        outputBox.setEnabled (manual);
        preferComputerToggle.setEnabled (modeBox.getSelectedId() == 1);
    };
    modeBox.onChange();

    manager.addChangeListener (this);
    refreshDeviceLists();
    refreshStatus();
    setSize (640, 620);
    startTimerHz (2);
}

AudioSettingsDialog::~AudioSettingsDialog()
{
    stopTimer();
    manager.removeChangeListener (this);
}

void AudioSettingsDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().windowBackground);
}

void AudioSettingsDialog::resized()
{
    auto r = getLocalBounds().reduced (16);
    titleLabel.setBounds (r.removeFromTop (28));
    r.removeFromTop (6);
    introLabel.setBounds (r.removeFromTop (88));
    r.removeFromTop (8);

    auto modeRow = r.removeFromTop (28);
    modeLabel.setBounds (modeRow.removeFromLeft (110));
    modeBox.setBounds (modeRow);
    r.removeFromTop (8);
    preferComputerToggle.setBounds (r.removeFromTop (32));
    r.removeFromTop (4);
    preferProAudioToggle.setBounds (r.removeFromTop (32));
    r.removeFromTop (8);

    auto inRow = r.removeFromTop (28);
    inputLabel.setBounds (inRow.removeFromLeft (110));
    inputBox.setBounds (inRow);
    r.removeFromTop (6);
    auto outRow = r.removeFromTop (28);
    outputLabel.setBounds (outRow.removeFromLeft (110));
    outputBox.setBounds (outRow);
    r.removeFromTop (10);

    auto chRow = r.removeFromTop (28);
    maxInLabel.setBounds (chRow.removeFromLeft (140));
    maxInBox.setBounds (chRow.removeFromLeft (80));
    chRow.removeFromLeft (16);
    maxOutLabel.setBounds (chRow.removeFromLeft (150));
    maxOutBox.setBounds (chRow.removeFromLeft (80));
    r.removeFromTop (12);

    statusHeading.setBounds (r.removeFromTop (22));
    statusLabel.setBounds (r.removeFromTop (48));
    r.removeFromTop (8);
    jackNoteLabel.setBounds (r.removeFromTop (64));

    auto buttons = r.removeFromBottom (32);
    closeButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    applyButton.setBounds (buttons.removeFromRight (90));
    buttons.removeFromRight (8);
    rescanButton.setBounds (buttons.removeFromRight (130));
}

void AudioSettingsDialog::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refreshStatus();
}

void AudioSettingsDialog::timerCallback()
{
    refreshStatus();
}

void AudioSettingsDialog::refreshDeviceLists()
{
    const auto snap = manager.getSnapshot();
    const auto settings = manager.getSettings();

    inputNames.clear();
    outputNames.clear();
    inputBox.clear (juce::dontSendNotification);
    outputBox.clear (juce::dontSendNotification);

    inputBox.addItem ("(Auto / none)", 1);
    outputBox.addItem ("(Auto / none)", 1);

    int inSel = 1;
    int outSel = 1;
    int id = 2;

    for (const auto& d : manager.listInputDevices())
    {
        inputNames.add (d.name);
        const auto label = d.name + "  [" + d.typeName + ", " + juce::String (d.maxInputChannels) + " in]";
        inputBox.addItem (label, id);
        if (d.name == settings.preferredInputName
            || (settings.preferredInputName.isEmpty() && d.name == snap.inputName))
            inSel = id;
        ++id;
    }

    id = 2;
    for (const auto& d : manager.listOutputDevices())
    {
        outputNames.add (d.name);
        const auto label = d.name + "  [" + d.typeName + ", " + juce::String (d.maxOutputChannels) + " out]";
        outputBox.addItem (label, id);
        if (d.name == settings.preferredOutputName
            || (settings.preferredOutputName.isEmpty() && d.name == snap.outputName))
            outSel = id;
        ++id;
    }

    // Plug-and-play / same-device: show Auto — greyed lists still listed sticky
    // names which made people think JACK (1 ch) was the active capture path.
    if (settings.mode != jamstudio::audio::AudioRoutingMode::manual)
    {
        inputBox.setSelectedId (1, juce::dontSendNotification);
        // Show real active output if we know it; still Auto-selectable.
        outputBox.setSelectedId (outSel > 1 ? outSel : 1, juce::dontSendNotification);
        if (settings.mode == jamstudio::audio::AudioRoutingMode::plugAndPlay)
            outputBox.setSelectedId (1, juce::dontSendNotification);
    }
    else
    {
        inputBox.setSelectedId (inSel, juce::dontSendNotification);
        outputBox.setSelectedId (outSel, juce::dontSendNotification);
    }
}

void AudioSettingsDialog::refreshStatus()
{
    statusLabel.setText (manager.getStatusSummary(), juce::dontSendNotification);
}

void AudioSettingsDialog::applyFromUi()
{
    jamstudio::audio::AudioInterfaceSettings s = manager.getSettings();

    switch (modeBox.getSelectedId())
    {
        case 2: s.mode = jamstudio::audio::AudioRoutingMode::sameDevice; break;
        case 3: s.mode = jamstudio::audio::AudioRoutingMode::manual; break;
        default: s.mode = jamstudio::audio::AudioRoutingMode::plugAndPlay; break;
    }

    s.preferComputerSpeakersForMonitor = preferComputerToggle.getToggleState();
    s.preferProAudioProfile = preferProAudioToggle.getToggleState();
    s.maxInputChannels = juce::jmax (1, maxInBox.getSelectedId());
    s.maxOutputChannels = juce::jmax (2, maxOutBox.getSelectedId());

    if (s.mode == jamstudio::audio::AudioRoutingMode::manual)
    {
        const auto inId = inputBox.getSelectedId();
        const auto outId = outputBox.getSelectedId();
        s.preferredInputName = (inId <= 1) ? juce::String()
                                           : inputNames[inId - 2];
        s.preferredOutputName = (outId <= 1) ? juce::String()
                                             : outputNames[outId - 2];
    }
    else
    {
        // Plug-and-play must not keep a sticky "JACK (1 ch)" preference — that
        // forced mono capture and ignored the Scarlett multi-in device.
        s.preferredInputName = {};
        s.preferredOutputName = {};
        s.deviceTypeName = {};
    }

    const auto err = manager.applySettings (s);
    refreshDeviceLists();
    refreshStatus();

    if (err.isNotEmpty())
        statusLabel.setText (manager.getStatusSummary() + "\nCould not fully apply: " + err,
                             juce::dontSendNotification);
}

void AudioSettingsDialog::dismiss()
{
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
        dw->exitModalState (0);
}

} // namespace jamstudio::ui
