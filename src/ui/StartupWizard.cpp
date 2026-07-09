#include "StartupWizard.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

StartupWizard::StartupWizard()
{
    titleLabel.setText ("Welcome to JamStudio", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    titleLabel.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (titleLabel);

    subtitleLabel.setText ("What do you want to do?", juce::dontSendNotification);
    subtitleLabel.setFont (juce::FontOptions (16.0f));
    subtitleLabel.setJustificationType (juce::Justification::centred);
    subtitleLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    addAndMakeVisible (subtitleLabel);

    // Mode page
    addAndMakeVisible (modePage);
    auto styleModeButton = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, JamStudioTheme::getColours().buttonFace);
        b.setColour (juce::TextButton::textColourOffId, JamStudioTheme::getColours().text);
    };

    practiceButton.onClick = [this]
    {
        if (onModeChosen)
            onModeChosen (Mode::practice);
        showPracticePage();
    };
    performanceButton.onClick = [this]
    {
        if (onModeChosen)
            onModeChosen (Mode::performance);
    };
    recordingButton.onClick = [this]
    {
        if (onModeChosen)
            onModeChosen (Mode::recording);
    };

    styleModeButton (practiceButton);
    styleModeButton (performanceButton);
    styleModeButton (recordingButton);
    modePage.addAndMakeVisible (practiceButton);
    modePage.addAndMakeVisible (performanceButton);
    modePage.addAndMakeVisible (recordingButton);

    practiceDesc.setText ("Learn a song: auto stems, tabs & lyrics → saved project",
                          juce::dontSendNotification);
    performanceDesc.setText ("Play along with a full mix and live mixer control",
                             juce::dontSendNotification);
    recordingDesc.setText ("Record yourself over backing tracks",
                           juce::dontSendNotification);

    for (auto* l : { &practiceDesc, &performanceDesc, &recordingDesc })
    {
        l->setJustificationType (juce::Justification::centred);
        l->setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        modePage.addAndMakeVisible (*l);
    }

    // Practice page
    addChildComponent (practicePage);
    practiceTitle.setText ("Practice setup", juce::dontSendNotification);
    practiceTitle.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    practiceTitle.setJustificationType (juce::Justification::centred);
    practicePage.addAndMakeVisible (practiceTitle);

    practiceHint.setText ("Open a saved project, or choose a song file to stem + fetch tabs & lyrics automatically.",
                          juce::dontSendNotification);
    practiceHint.setJustificationType (juce::Justification::centred);
    practiceHint.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
    practicePage.addAndMakeVisible (practiceHint);

    openProjectButton.onClick = [this]
    {
        if (onPracticeChoice)
            onPracticeChoice (PracticeChoice::openProject);
    };
    newSongButton.onClick = [this]
    {
        if (onPracticeChoice)
            onPracticeChoice (PracticeChoice::newFromSong);
    };
    practiceBackButton.onClick = [this] { showModePage(); };

    practicePage.addAndMakeVisible (openProjectButton);
    practicePage.addAndMakeVisible (newSongButton);
    practicePage.addAndMakeVisible (practiceBackButton);

    showModePage();
}

void StartupWizard::setModeChosenCallback (ModeChosenCallback cb)
{
    onModeChosen = std::move (cb);
}

void StartupWizard::setPracticeChoiceCallback (PracticeChoiceCallback cb)
{
    onPracticeChoice = std::move (cb);
}

void StartupWizard::showModePage()
{
    showPage (0);
}

void StartupWizard::showPracticePage()
{
    showPage (1);
}

void StartupWizard::showPage (const int pageIndex)
{
    currentPage = pageIndex;
    modePage.setVisible (pageIndex == 0);
    practicePage.setVisible (pageIndex == 1);
    subtitleLabel.setText (pageIndex == 0 ? "What do you want to do?"
                                          : "How do you want to practice?",
                           juce::dontSendNotification);
    resized();
    repaint();
}

void StartupWizard::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    g.fillAll (colours.windowBackground.withAlpha (0.97f));

    auto card = getLocalBounds().reduced (juce::jmax (40, getWidth() / 8),
                                          juce::jmax (40, getHeight() / 10)).toFloat();
    g.setColour (colours.panelBackground);
    g.fillRoundedRectangle (card, 12.0f);
    g.setColour (colours.border);
    g.drawRoundedRectangle (card, 12.0f, 1.5f);
}

void StartupWizard::resized()
{
    auto outer = getLocalBounds().reduced (juce::jmax (48, getWidth() / 8),
                                           juce::jmax (48, getHeight() / 10));
    titleLabel.setBounds (outer.removeFromTop (40));
    outer.removeFromTop (6);
    subtitleLabel.setBounds (outer.removeFromTop (28));
    outer.removeFromTop (16);

    modePage.setBounds (outer);
    practicePage.setBounds (outer);

    // Mode buttons
    {
        auto area = modePage.getLocalBounds().reduced (24, 8);
        const auto btnH = 56;
        const auto gap = 28;

        practiceButton.setBounds (area.removeFromTop (btnH));
        practiceDesc.setBounds (area.removeFromTop (24));
        area.removeFromTop (gap);

        performanceButton.setBounds (area.removeFromTop (btnH));
        performanceDesc.setBounds (area.removeFromTop (24));
        area.removeFromTop (gap);

        recordingButton.setBounds (area.removeFromTop (btnH));
        recordingDesc.setBounds (area.removeFromTop (24));
    }

    // Practice page
    {
        auto area = practicePage.getLocalBounds().reduced (24, 8);
        practiceTitle.setBounds (area.removeFromTop (36));
        area.removeFromTop (8);
        practiceHint.setBounds (area.removeFromTop (48));
        area.removeFromTop (20);

        openProjectButton.setBounds (area.removeFromTop (52));
        area.removeFromTop (12);
        newSongButton.setBounds (area.removeFromTop (52));
        area.removeFromTop (20);
        practiceBackButton.setBounds (area.removeFromTop (40).withSizeKeepingCentre (120, 36));
    }
}

} // namespace jamstudio::ui
