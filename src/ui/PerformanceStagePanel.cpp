#include "PerformanceStagePanel.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

// =============================================================================
// NamPathPanel
// =============================================================================

PerformanceStagePanel::NamPathPanel::NamPathPanel (
    const jamstudio::performance::LiveInstrumentRole r,
    jamstudio::audio::LiveToneEngine& eng,
    jamstudio::performance::ToneLibrary& lib)
    : role (r),
      engine (eng),
      library (lib)
{
    const auto accent = jamstudio::performance::liveInstrumentRoleColour (role);

    titleLabel.setText (jamstudio::performance::liveInstrumentRoleName (role) + "  ·  NAM",
                        juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    titleLabel.setColour (juce::Label::textColourId, accent);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    profileBox.setTextWhenNothingSelected ("Profile…");
    profileBox.onChange = [this]
    {
        const auto id = profileBox.getText();
        // item id maps via selected id string stored in item text? use itemData
        const auto sel = profileBox.getSelectedId();
        if (sel <= 0)
            return;
        const auto profiles = library.profilesForRole (role);
        if (juce::isPositiveAndBelow (sel - 1, profiles.size()))
        {
            loadProfile (profiles.getReference (sel - 1));
            if (onProfileSelected)
                onProfileSelected();
        }
    };
    addAndMakeVisible (profileBox);

    saveButton.setTooltip ("Update selected library profile from knobs");
    saveButton.onClick = [this]
    {
        auto p = captureProfile();
        if (p.id.isEmpty())
        {
            p = jamstudio::performance::ToneProfile::makeDefault (role);
            p.name = jamstudio::performance::liveInstrumentRoleShortName (role) + " Tone";
        }
        // Re-capture knobs onto p
        p = captureProfile();
        if (p.id.isEmpty())
            p.id = juce::Uuid().toDashedString();

        if (! library.updateProfile (p))
            library.addProfile (p);
        library.save();
        refreshProfileList();
        selectProfileId (p.id);
        if (onProfileSaved)
            onProfileSaved();
    };
    addAndMakeVisible (saveButton);

    saveAsButton.setTooltip ("Save knobs as a new named profile");
    saveAsButton.onClick = [this]
    {
        auto p = captureProfile();
        p.id = juce::Uuid().toDashedString();
        p.role = role;
        auto* aw = new juce::AlertWindow ("Save tone profile",
                                          "Name for this " + jamstudio::performance::liveInstrumentRoleName (role)
                                              + " tone:",
                                          juce::MessageBoxIconType::QuestionIcon);
        aw->addTextEditor ("name", p.name.isNotEmpty() ? p.name : "New Tone", "Profile name");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [this, aw, p] (const int result) mutable
            {
                if (result == 1)
                {
                    p.name = aw->getTextEditorContents ("name").trim();
                    if (p.name.isEmpty())
                        p.name = "New Tone";
                    library.addProfile (p);
                    library.save();
                    refreshProfileList();
                    selectProfileId (p.id);
                    loadProfile (p);
                    if (onProfileSaved)
                        onProfileSaved();
                }
                delete aw;
            }), true);
    };
    addAndMakeVisible (saveAsButton);

    enableToggle.setToggleState (true, juce::dontSendNotification);
    enableToggle.onClick = [this]
    {
        engine.setPathEnabled (role, enableToggle.getToggleState());
    };
    addAndMakeVisible (enableToggle);

    bypassToggle.onClick = [this] { pushParamsToEngine(); };
    addAndMakeVisible (bypassToggle);

    modelLabel.setText ("Model: built-in amp sim (NAM path ready)", juce::dontSendNotification);
    modelLabel.setFont (juce::FontOptions (10.0f));
    modelLabel.setColour (juce::Label::textColourId,
                          JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (modelLabel);

    styleKnob (inputGain, "Input");
    styleKnob (drive, "Drive");
    styleKnob (bass, "Bass");
    styleKnob (mid, "Mid");
    styleKnob (treble, "Treble");
    styleKnob (presence, "Pres");
    styleKnob (outputLevel, "Level");

    for (auto* s : { &inputGain, &drive, &bass, &mid, &treble, &presence, &outputLevel })
        bindKnob (*s);

    inMeterLabel.setFont (juce::FontOptions (9.0f));
    outMeterLabel.setFont (juce::FontOptions (9.0f));
    inMeterLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    outMeterLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (inMeterLabel);
    addAndMakeVisible (outMeterLabel);

    refreshProfileList();
    loadProfile (library.resolve (role, {}));
}

void PerformanceStagePanel::NamPathPanel::styleKnob (juce::Slider& s, const juce::String& name)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 14);
    s.setRange (0.0, 1.0, 0.01);
    s.setRotaryParameters (juce::MathConstants<float>::pi * 1.2f,
                           juce::MathConstants<float>::pi * 2.8f,
                           true);
    s.setMouseDragSensitivity (160);
    s.setPopupDisplayEnabled (true, true, this);
    s.setColour (juce::Slider::rotarySliderFillColourId,
                 jamstudio::performance::liveInstrumentRoleColour (role));
    s.setName (name);
    s.setTooltip (name);
    addAndMakeVisible (s);
}

void PerformanceStagePanel::NamPathPanel::bindKnob (juce::Slider& s)
{
    s.onValueChange = [this] { pushParamsToEngine(); };
}

void PerformanceStagePanel::NamPathPanel::setCompact (const bool shouldBeCompact)
{
    compact = shouldBeCompact;
    saveButton.setVisible (! compact);
    saveAsButton.setVisible (! compact);
    modelLabel.setVisible (! compact);
    profileBox.setEnabled (! compact);
    // Knobs stay visible but smaller via resized
    resized();
    repaint();
}

void PerformanceStagePanel::NamPathPanel::refreshProfileList()
{
    const auto keep = getSelectedProfileId();
    profileBox.clear (juce::dontSendNotification);
    const auto profiles = library.profilesForRole (role);
    for (int i = 0; i < profiles.size(); ++i)
        profileBox.addItem (profiles.getReference (i).name, i + 1);
    if (keep.isNotEmpty())
        selectProfileId (keep);
    else if (profiles.size() > 0)
        profileBox.setSelectedId (1, juce::dontSendNotification);
}

void PerformanceStagePanel::NamPathPanel::selectProfileId (const juce::String& id)
{
    currentProfileId = id;
    const auto profiles = library.profilesForRole (role);
    for (int i = 0; i < profiles.size(); ++i)
    {
        if (profiles.getReference (i).id == id)
        {
            profileBox.setSelectedId (i + 1, juce::dontSendNotification);
            return;
        }
    }
}

void PerformanceStagePanel::NamPathPanel::loadProfile (const jamstudio::performance::ToneProfile& profile)
{
    currentProfileId = profile.id;
    selectProfileId (profile.id);

    auto setSilent = [] (juce::Slider& s, double v)
    {
        auto cb = std::move (s.onValueChange);
        s.onValueChange = nullptr;
        s.setValue (v, juce::dontSendNotification);
        s.onValueChange = std::move (cb);
    };

    setSilent (inputGain, profile.inputGain);
    setSilent (drive, profile.drive);
    setSilent (bass, profile.bass);
    setSilent (mid, profile.mid);
    setSilent (treble, profile.treble);
    setSilent (presence, profile.presence);
    setSilent (outputLevel, profile.outputLevel);
    bypassToggle.setToggleState (profile.bypass, juce::dontSendNotification);

    if (profile.namModelPath.isNotEmpty())
        modelLabel.setText ("Model: " + juce::File (profile.namModelPath).getFileName(),
                            juce::dontSendNotification);
    else
        modelLabel.setText ("Model: built-in amp sim (NAM path ready)", juce::dontSendNotification);

    engine.applyProfile (role, profile);
    engine.setPathEnabled (role, enableToggle.getToggleState());
}

jamstudio::performance::ToneProfile PerformanceStagePanel::NamPathPanel::captureProfile() const
{
    jamstudio::performance::ToneProfile p;
    p.id = currentProfileId;
    p.role = role;
    p.name = profileBox.getText();
    if (p.name.isEmpty())
        p.name = jamstudio::performance::liveInstrumentRoleShortName (role) + " Tone";

    if (const auto* existing = library.findById (currentProfileId))
    {
        p.namModelPath = existing->namModelPath;
        p.cabIrPath = existing->cabIrPath;
        if (p.name.isEmpty())
            p.name = existing->name;
    }

    p.inputGain = (float) inputGain.getValue();
    p.drive = (float) drive.getValue();
    p.bass = (float) bass.getValue();
    p.mid = (float) mid.getValue();
    p.treble = (float) treble.getValue();
    p.presence = (float) presence.getValue();
    p.outputLevel = (float) outputLevel.getValue();
    p.bypass = bypassToggle.getToggleState();
    return p;
}

juce::String PerformanceStagePanel::NamPathPanel::getSelectedProfileId() const
{
    return currentProfileId;
}

void PerformanceStagePanel::NamPathPanel::pushParamsToEngine()
{
    engine.applyProfile (role, captureProfile());
    engine.setPathEnabled (role, enableToggle.getToggleState());
}

void PerformanceStagePanel::NamPathPanel::timerTick()
{
    inLevel = engine.getInputMeter (role);
    outLevel = engine.getOutputMeter (role);
    inMeterLabel.setText ("IN", juce::dontSendNotification);
    outMeterLabel.setText ("OUT", juce::dontSendNotification);
    repaint();
}

void PerformanceStagePanel::NamPathPanel::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    const auto accent = jamstudio::performance::liveInstrumentRoleColour (role);
    auto r = getLocalBounds().toFloat().reduced (2.0f);

    g.setColour (colours.panelBackground.brighter (compact ? 0.02f : 0.04f));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (accent.withAlpha (0.55f));
    g.drawRoundedRectangle (r, 10.0f, compact ? 1.0f : 1.5f);

    // Simple LED meters
    auto meterArea = r.removeFromRight (14.0f).reduced (3.0f, 10.0f);
    auto outM = meterArea.removeFromRight (5.0f);
    auto inM = meterArea.removeFromRight (5.0f);
    meterArea.removeFromRight (2.0f);

    auto drawMeter = [&g] (juce::Rectangle<float> area, float level, juce::Colour c)
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (area, 2.0f);
        const float h = area.getHeight() * juce::jlimit (0.0f, 1.0f, level);
        auto fill = area.removeFromBottom (h);
        g.setColour (c);
        g.fillRoundedRectangle (fill, 2.0f);
    };

    drawMeter (inM, inLevel, juce::Colour (0xff66ccff));
    drawMeter (outM, outLevel, accent);
}

void PerformanceStagePanel::NamPathPanel::resized()
{
    auto a = getLocalBounds().reduced (8);
    // leave room for meters painted on the right
    a.removeFromRight (16);

    titleLabel.setBounds (a.removeFromTop (18));
    a.removeFromTop (4);

    auto row = a.removeFromTop (compact ? 26 : 28);
    profileBox.setBounds (row.removeFromLeft (juce::jmax (80, row.getWidth() / (compact ? 2 : 3))));
    row.removeFromLeft (4);
    enableToggle.setBounds (row.removeFromLeft (44));
    bypassToggle.setBounds (row.removeFromLeft (64));
    if (! compact)
    {
        saveAsButton.setBounds (row.removeFromRight (70).reduced (1));
        saveButton.setBounds (row.removeFromRight (56).reduced (1));
    }

    if (! compact)
    {
        a.removeFromTop (4);
        modelLabel.setBounds (a.removeFromTop (16));
    }

    a.removeFromTop (6);
    auto knobs = a.removeFromTop (compact ? juce::jmin (a.getHeight(), 100) : juce::jmin (a.getHeight() - 18, 130));
    const int n = 7;
    const int w = juce::jmax (1, knobs.getWidth() / n);
    juce::Slider* order[] = { &inputGain, &drive, &bass, &mid, &treble, &presence, &outputLevel };
    for (int i = 0; i < n; ++i)
    {
        auto cell = (i + 1 < n) ? knobs.removeFromLeft (w) : knobs;
        order[i]->setBounds (cell.reduced (2));
        order[i]->setTextBoxStyle (juce::Slider::TextBoxBelow, false,
                                   juce::jmin (48, cell.getWidth() - 2), compact ? 12 : 14);
    }

    auto meterLabels = a.removeFromBottom (14);
    inMeterLabel.setBounds (meterLabels.removeFromLeft (30));
    outMeterLabel.setBounds (meterLabels.removeFromLeft (36));
}

// =============================================================================
// PerformanceStagePanel
// =============================================================================

PerformanceStagePanel::PerformanceStagePanel (jamstudio::audio::LiveToneEngine& eng,
                                              jamstudio::performance::ToneLibrary& lib)
    : engine (eng),
      library (lib)
{
    modeBadge.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    modeBadge.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (modeBadge);

    auto styleModeBtn = [] (juce::TextButton& b, bool active)
    {
        b.setColour (juce::TextButton::buttonColourId,
                     active ? JamStudioTheme::getColours().accent
                            : JamStudioTheme::getColours().buttonFace);
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    };

    setupModeButton.onClick = [this]
    {
        if (onBackToSetup)
            onBackToSetup();
        else
            setStageMode (PerformanceStageMode::setup);
    };
    liveModeButton.onClick = [this]
    {
        if (onGoLive)
            onGoLive();
        else
            setStageMode (PerformanceStageMode::live);
    };
    addAndMakeVisible (setupModeButton);
    addAndMakeVisible (liveModeButton);

    goLiveButton.setColour (juce::TextButton::buttonColourId, juce::Colour (0xff22aa55));
    goLiveButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    goLiveButton.onClick = [this]
    {
        if (onGoLive)
            onGoLive();
    };
    addAndMakeVisible (goLiveButton);

    backSetupButton.onClick = [this]
    {
        if (onBackToSetup)
            onBackToSetup();
    };
    addAndMakeVisible (backSetupButton);

    triggerButton.setColour (juce::TextButton::buttonColourId,
                             JamStudioTheme::getColours().accent.darker (0.1f));
    triggerButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white);
    triggerButton.onClick = [this]
    {
        if (onTrigger)
            onTrigger();
    };
    addAndMakeVisible (triggerButton);

    saveSongTonesButton.setTooltip ("Remember G1/G2/Bass profile choices on the current setlist song");
    saveSongTonesButton.onClick = [this]
    {
        if (onSaveSongTones)
            onSaveSongTones();
    };
    addAndMakeVisible (saveSongTonesButton);

    setLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
    setLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    songLabel.setFont (juce::FontOptions (16.0f, juce::Font::bold));
    phaseLabel.setFont (juce::FontOptions (13.0f));
    phaseLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().accent);
    upNextLabel.setFont (juce::FontOptions (12.0f));
    upNextLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    hintLabel.setFont (juce::FontOptions (11.0f));
    hintLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    hintLabel.setJustificationType (juce::Justification::centredLeft);

    for (auto* l : { &setLabel, &songLabel, &phaseLabel, &upNextLabel, &hintLabel })
        addAndMakeVisible (*l);

    for (int i = 0; i < jamstudio::performance::kNumLiveTonePaths; ++i)
    {
        const auto role = static_cast<jamstudio::performance::LiveInstrumentRole> (i);
        pathPanels[i] = std::make_unique<NamPathPanel> (role, engine, library);
        pathPanels[i]->onProfileSaved = [this] { refreshFromLibrary(); };
        addAndMakeVisible (*pathPanels[i]);
    }

    juce::ignoreUnused (styleModeBtn);
    setStageMode (PerformanceStageMode::setup);
    startTimerHz (24);
}

void PerformanceStagePanel::setStageMode (const PerformanceStageMode mode)
{
    stageMode = mode;
    updateModeChrome();
    if (onModeChanged)
        onModeChanged (mode);
    resized();
    repaint();
}

void PerformanceStagePanel::updateModeChrome()
{
    const bool setup = stageMode == PerformanceStageMode::setup;
    modeBadge.setText (setup ? "PERFORMANCE SETUP" : "ON STAGE · LIVE", juce::dontSendNotification);
    modeBadge.setColour (juce::Label::textColourId,
                         setup ? JamStudioTheme::getColours().accent
                               : juce::Colour (0xff44dd77));

    setupModeButton.setColour (juce::TextButton::buttonColourId,
                               setup ? JamStudioTheme::getColours().accent
                                     : JamStudioTheme::getColours().buttonFace);
    liveModeButton.setColour (juce::TextButton::buttonColourId,
                              ! setup ? juce::Colour (0xff22aa55)
                                      : JamStudioTheme::getColours().buttonFace);

    goLiveButton.setVisible (setup);
    backSetupButton.setVisible (! setup);
    saveSongTonesButton.setVisible (setup);
    triggerButton.setVisible (true);

    hintLabel.setText (setup
                           ? "Craft G1 / G2 / Bass tones, assign profiles to the song, open mixer for buses. "
                             "Lyrics & tabs stay off this page — use Karaoke / Stage FX for words."
                           : "Stage manager: START / NEXT loads the next cue (stems, mix, tones, media). "
                             "Keep eyes on the set — tweak tones only if needed.",
                       juce::dontSendNotification);

    for (auto& panel : pathPanels)
        if (panel)
            panel->setCompact (! setup);
}

void PerformanceStagePanel::setSetListInfo (const juce::String& setName,
                                           const int songIndex,
                                           const int songCount,
                                           const juce::String& songTitle)
{
    setLabel.setText ("SET: " + setName, juce::dontSendNotification);

    if (songCount <= 0)
        songLabel.setText ("No songs in set list", juce::dontSendNotification);
    else if (songIndex < 0)
        songLabel.setText ("Ready — load / start song 1", juce::dontSendNotification);
    else
        songLabel.setText (juce::String (songIndex + 1) + " / " + juce::String (songCount)
                           + "  —  " + songTitle,
                           juce::dontSendNotification);
}

void PerformanceStagePanel::setPhaseMessage (const juce::String& message)
{
    phaseLabel.setText (message, juce::dontSendNotification);
}

void PerformanceStagePanel::setWaitingForTrigger (const bool shouldWait)
{
    waiting = shouldWait;
    triggerButton.setButtonText (waiting ? "START NEXT SONG" : "START / NEXT");
    triggerButton.setColour (juce::TextButton::buttonColourId,
                             waiting ? juce::Colour (0xff22aa55)
                                     : JamStudioTheme::getColours().accent.darker (0.1f));
    repaint();
}

void PerformanceStagePanel::setUpNext (const juce::String& nextTitle)
{
    if (nextTitle.isEmpty())
        upNextLabel.setText ({}, juce::dontSendNotification);
    else
        upNextLabel.setText ("Up next: " + nextTitle, juce::dontSendNotification);
}

void PerformanceStagePanel::refreshFromLibrary()
{
    for (auto& panel : pathPanels)
        if (panel)
            panel->refreshProfileList();
}

void PerformanceStagePanel::loadPathFromProfile (
    const jamstudio::performance::LiveInstrumentRole role,
    const jamstudio::performance::ToneProfile& profile)
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths) && pathPanels[i])
        pathPanels[i]->loadProfile (profile);
}

void PerformanceStagePanel::selectProfileInCombo (
    const jamstudio::performance::LiveInstrumentRole role,
    const juce::String& profileId)
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths) && pathPanels[i])
        pathPanels[i]->selectProfileId (profileId);
}

jamstudio::performance::ToneProfile PerformanceStagePanel::capturePathProfile (
    const jamstudio::performance::LiveInstrumentRole role) const
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths) && pathPanels[i])
        return pathPanels[i]->captureProfile();
    return {};
}

juce::String PerformanceStagePanel::getSelectedProfileId (
    const jamstudio::performance::LiveInstrumentRole role) const
{
    const auto i = static_cast<int> (role);
    if (juce::isPositiveAndBelow (i, jamstudio::performance::kNumLiveTonePaths) && pathPanels[i])
        return pathPanels[i]->getSelectedProfileId();
    return {};
}

void PerformanceStagePanel::timerCallback()
{
    for (auto& panel : pathPanels)
        if (panel)
            panel->timerTick();
}

void PerformanceStagePanel::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.windowBackground);

    auto header = getLocalBounds().removeFromTop (52).toFloat().reduced (4.0f, 2.0f);
    g.setColour (colours.panelBackground.brighter (waiting ? 0.06f : 0.02f));
    g.fillRoundedRectangle (header, 8.0f);
    g.setColour (waiting ? colours.accent : colours.border);
    g.drawRoundedRectangle (header, 8.0f, waiting ? 2.0f : 1.0f);
}

void PerformanceStagePanel::resized()
{
    auto bounds = getLocalBounds().reduced (6);

    auto header = bounds.removeFromTop (48);
    modeBadge.setBounds (header.removeFromLeft (160).reduced (4, 8));
    setupModeButton.setBounds (header.removeFromLeft (72).reduced (2, 8));
    liveModeButton.setBounds (header.removeFromLeft (72).reduced (2, 8));
    header.removeFromLeft (8);

    if (goLiveButton.isVisible())
        goLiveButton.setBounds (header.removeFromRight (110).reduced (2, 8));
    if (backSetupButton.isVisible())
        backSetupButton.setBounds (header.removeFromRight (90).reduced (2, 8));
    if (saveSongTonesButton.isVisible())
        saveSongTonesButton.setBounds (header.removeFromRight (140).reduced (2, 8));

    triggerButton.setBounds (header.removeFromRight (150).reduced (2, 6));

    bounds.removeFromTop (4);
    auto info = bounds.removeFromTop (stageMode == PerformanceStageMode::setup ? 56 : 64);
    setLabel.setBounds (info.removeFromTop (16));
    songLabel.setBounds (info.removeFromTop (22));
    phaseLabel.setBounds (info.removeFromTop (18));
    if (stageMode == PerformanceStageMode::live)
        upNextLabel.setBounds (info);

    bounds.removeFromTop (4);
    hintLabel.setBounds (bounds.removeFromTop (32));
    bounds.removeFromTop (4);

    // Three NAM path columns
    const int gap = 6;
    const int colW = juce::jmax (1, (bounds.getWidth() - gap * 2) / 3);
    for (int i = 0; i < jamstudio::performance::kNumLiveTonePaths; ++i)
    {
        auto col = (i + 1 < jamstudio::performance::kNumLiveTonePaths)
                       ? bounds.removeFromLeft (colW)
                       : bounds;
        if (i + 1 < jamstudio::performance::kNumLiveTonePaths)
            bounds.removeFromLeft (gap);
        if (pathPanels[i])
            pathPanels[i]->setBounds (col);
    }
}

} // namespace jamstudio::ui
