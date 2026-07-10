#include "StartupWizard.h"

#include "BrandAssets.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

//==============================================================================
StartupWizard::IconCardButton::IconCardButton (const juce::String& name,
                                               const CardIcon icon,
                                               const juce::String& captionText,
                                               const juce::String& detailText)
    : juce::Button (name),
      iconType (icon),
      caption (captionText),
      detail (detailText)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (detailText.isNotEmpty() ? detailText : captionText);
}

void StartupWizard::IconCardButton::setCustomIcon (juce::Image image)
{
    customIcon = std::move (image);
    repaint();
}

void StartupWizard::IconCardButton::paintButton (juce::Graphics& g,
                                                 const bool shouldDrawButtonAsHighlighted,
                                                 const bool shouldDrawButtonAsDown)
{
    const auto colours = JamStudioTheme::getColours();
    auto bounds = getLocalBounds().toFloat().reduced (2.0f);

    // Free-floating tile (soft shadow, no parent window chrome)
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 16.0f);

    auto face = colours.buttonFace;

    if (shouldDrawButtonAsDown)
        face = face.darker (0.18f);
    else if (shouldDrawButtonAsHighlighted)
        face = face.brighter (0.08f);

    g.setGradientFill (juce::ColourGradient (face.brighter (0.16f),
                                             bounds.getTopLeft(),
                                             face.darker (0.16f),
                                             bounds.getBottomRight(),
                                             false));
    g.fillRoundedRectangle (bounds, 16.0f);

    g.setColour (shouldDrawButtonAsHighlighted ? colours.accent.withAlpha (0.9f)
                                               : colours.border.withAlpha (0.85f));
    g.drawRoundedRectangle (bounds, 16.0f, shouldDrawButtonAsHighlighted ? 2.2f : 1.3f);

    if (shouldDrawButtonAsHighlighted)
    {
        g.setColour (colours.accent.withAlpha (0.10f));
        g.fillRoundedRectangle (bounds.reduced (1.5f), 12.0f);
    }

    // Layout: icon upper 58%, caption lower band
    auto inner = bounds.reduced (bounds.getWidth() * 0.12f, bounds.getHeight() * 0.10f);
    auto iconArea = inner.removeFromTop (inner.getHeight() * 0.58f);
    auto textArea = inner;

    auto iconColour = shouldDrawButtonAsHighlighted ? colours.accent : colours.text;

    if (shouldDrawButtonAsDown)
        iconColour = iconColour.darker (0.12f);

    // Keep icon drawing square inside iconArea
    const auto side = juce::jmin (iconArea.getWidth(), iconArea.getHeight());
    auto iconSquare = juce::Rectangle<float> (side, side).withCentre (iconArea.getCentre());

    if (customIcon.isValid())
    {
        // Fit custom art (e.g. Practice.jpg headstock) with a little padding.
        auto dest = iconSquare.reduced (side * 0.04f);

        if (shouldDrawButtonAsDown)
            dest = dest.translated (0.0f, 1.0f);

        g.setOpacity (shouldDrawButtonAsHighlighted ? 1.0f : 0.96f);
        g.drawImage (customIcon, dest, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
        g.setOpacity (1.0f);

        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (colours.accent.withAlpha (0.12f));
            g.fillRoundedRectangle (dest, 6.0f);
        }
    }
    else
    {
        drawIcon (g, iconSquare.reduced (side * 0.06f), iconColour);
    }

    g.setColour (colours.text);
    g.setFont (juce::FontOptions (juce::jlimit (13.0f, 18.0f, bounds.getHeight() * 0.11f),
                                  juce::Font::bold));
    g.drawText (caption, textArea.removeFromTop (textArea.getHeight() * 0.55f),
                juce::Justification::centred, true);

    if (detail.isNotEmpty())
    {
        g.setColour (colours.textSecondary);
        g.setFont (juce::FontOptions (juce::jlimit (10.0f, 13.0f, bounds.getHeight() * 0.08f)));
        g.drawFittedText (detail, textArea.toNearestInt(), juce::Justification::centredTop, 2);
    }
}

void StartupWizard::IconCardButton::drawIcon (juce::Graphics& g,
                                              juce::Rectangle<float> area,
                                              const juce::Colour colour) const
{
    g.setColour (colour);
    const auto cx = area.getCentreX();
    const auto cy = area.getCentreY();
    const auto w = area.getWidth();
    const auto h = area.getHeight();

    switch (iconType)
    {
        case CardIcon::practice:
        {
            // Fallback only when Practice.jpg is missing - simple open-book headstock outline.
            juce::Path head;
            const auto tipY = area.getY() + h * 0.06f;
            const auto peakY = area.getY() + h * 0.20f;
            const auto nutY = area.getY() + h * 0.58f;
            const auto headW = w * 0.42f;
            const auto neckW = w * 0.20f;
            head.startNewSubPath (cx - neckW * 0.5f, nutY);
            head.lineTo (cx - headW, area.getY() + h * 0.42f);
            head.lineTo (cx - headW * 0.9f, peakY);
            head.lineTo (cx, tipY);
            head.lineTo (cx + headW * 0.9f, peakY);
            head.lineTo (cx + headW, area.getY() + h * 0.42f);
            head.lineTo (cx + neckW * 0.5f, nutY);
            head.closeSubPath();
            g.fillPath (head);
            g.fillRect (cx - neckW * 0.5f, nutY, neckW, area.getBottom() - nutY - h * 0.06f);
            break;
        }

        case CardIcon::stageShowBuilder:
        {
            // Film frame + play triangle
            g.drawRoundedRectangle (area.reduced (w * 0.12f, h * 0.18f), 4.0f, juce::jmax (2.0f, w * 0.06f));
            for (int i = 0; i < 4; ++i)
            {
                const float y = area.getY() + h * (0.22f + i * 0.16f);
                g.fillRect (area.getX() + w * 0.16f, y, w * 0.10f, h * 0.08f);
                g.fillRect (area.getRight() - w * 0.26f, y, w * 0.10f, h * 0.08f);
            }
            juce::Path tri;
            tri.addTriangle (cx - w * 0.06f, cy - h * 0.14f,
                             cx - w * 0.06f, cy + h * 0.14f,
                             cx + w * 0.16f, cy);
            g.fillPath (tri);
            break;
        }

        case CardIcon::performance:
        {
            // Play triangle inside a ring (stage / go live)
            g.drawEllipse (area.reduced (w * 0.06f), juce::jmax (2.0f, w * 0.07f));
            juce::Path play;
            const auto inset = area.reduced (w * 0.28f, h * 0.24f);
            play.addTriangle (inset.getX() + inset.getWidth() * 0.05f, inset.getY(),
                              inset.getX() + inset.getWidth() * 0.05f, inset.getBottom(),
                              inset.getRight(), inset.getCentreY());
            g.fillPath (play);
            break;
        }

        case CardIcon::recording:
        {
            // Outer ring + solid record disc
            g.drawEllipse (area.reduced (w * 0.04f), juce::jmax (2.0f, w * 0.08f));
            g.setColour (juce::Colour (0xffef4444));
            g.fillEllipse (area.reduced (w * 0.26f));
            break;
        }

        case CardIcon::openProject:
        {
            // Folder
            juce::Path folder;
            const auto top = area.getY() + h * 0.18f;
            folder.startNewSubPath (area.getX() + w * 0.08f, top + h * 0.12f);
            folder.lineTo (area.getX() + w * 0.08f, top);
            folder.lineTo (area.getX() + w * 0.38f, top);
            folder.lineTo (area.getX() + w * 0.46f, top + h * 0.12f);
            folder.lineTo (area.getRight() - w * 0.08f, top + h * 0.12f);
            folder.lineTo (area.getRight() - w * 0.08f, area.getBottom() - h * 0.12f);
            folder.lineTo (area.getX() + w * 0.08f, area.getBottom() - h * 0.12f);
            folder.closeSubPath();
            g.fillPath (folder);
            break;
        }

        case CardIcon::newSong:
        {
            // Audio disc + note
            g.drawEllipse (area.reduced (w * 0.08f), juce::jmax (2.0f, w * 0.07f));
            g.fillEllipse (area.reduced (w * 0.38f));
            juce::Path note;
            const auto nx = cx + w * 0.12f;
            note.addEllipse (nx - w * 0.10f, cy + h * 0.08f, w * 0.18f, h * 0.14f);
            note.addRectangle (nx + w * 0.05f, cy - h * 0.28f, w * 0.06f, h * 0.40f);
            g.fillPath (note);
            break;
        }

        case CardIcon::back:
        {
            juce::Path chevron;
            const auto midY = cy;
            chevron.startNewSubPath (cx + w * 0.18f, area.getY() + h * 0.18f);
            chevron.lineTo (cx - w * 0.18f, midY);
            chevron.lineTo (cx + w * 0.18f, area.getBottom() - h * 0.18f);
            g.strokePath (chevron, juce::PathStrokeType (juce::jmax (2.5f, w * 0.10f),
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
            break;
        }
    }
}

//==============================================================================
StartupWizard::StartupWizard()
    : practiceButton ("practice", CardIcon::practice, "Practice", "Stems - tabs - lyrics"),
      performanceButton ("performance", CardIcon::performance, "Performance", "Play along"),
      stageShowButton ("stageShow", CardIcon::stageShowBuilder, "Stage Show", "Videos & slides"),
      recordingButton ("recording", CardIcon::recording, "Recording", "Track yourself"),
      openProjectButton ("openProject", CardIcon::openProject, "Open Project", "Saved .jamstudio"),
      newSongButton ("newSong", CardIcon::newSong, "New from Song", "Auto setup"),
      practiceBackButton ("practiceBack", CardIcon::back, "Back", {}),
      recOpenProjectButton ("recOpenProject", CardIcon::openProject, "Open Project", "Backing + stems"),
      recOpenBackingButton ("recOpenBacking", CardIcon::newSong, "Open Backing", "Song file"),
      recEmptyButton ("recEmpty", CardIcon::recording, "Empty Session", "Record only"),
      recordingBackButton ("recordingBack", CardIcon::back, "Back", {})
{
    wizardBackground = BrandAssets::loadWizardBackground();
    practiceButton.setCustomIcon (BrandAssets::loadPracticeIcon());

    titleLabel.setVisible (false);
    subtitleLabel.setVisible (false);

    addAndMakeVisible (modePage);

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
    stageShowButton.onClick = [this]
    {
        if (onModeChosen)
            onModeChosen (Mode::stageShowBuilder);
    };
    recordingButton.onClick = [this]
    {
        if (onModeChosen)
            onModeChosen (Mode::recording);
        showRecordingPage();
    };

    modePage.addAndMakeVisible (practiceButton);
    modePage.addAndMakeVisible (performanceButton);
    modePage.addAndMakeVisible (stageShowButton);
    modePage.addAndMakeVisible (recordingButton);

    // Practice page
    addChildComponent (practicePage);
    practiceTitle.setText ("Practice setup", juce::dontSendNotification);
    practiceTitle.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    practiceTitle.setJustificationType (juce::Justification::centred);
    practiceTitle.setColour (juce::Label::textColourId, juce::Colours::white);
    practicePage.addAndMakeVisible (practiceTitle);

    practiceHint.setText ("Open a saved project, or pick a song to stem and fetch tabs & lyrics.",
                          juce::dontSendNotification);
    practiceHint.setJustificationType (juce::Justification::centred);
    practiceHint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.88f));
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

    // Recording page
    addChildComponent (recordingPage);
    recordingTitle.setText ("Recording setup", juce::dontSendNotification);
    recordingTitle.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    recordingTitle.setJustificationType (juce::Justification::centred);
    recordingTitle.setColour (juce::Label::textColourId, juce::Colours::white);
    recordingPage.addAndMakeVisible (recordingTitle);

    recordingHint.setText ("Load a backing track, then Open Audacity (or your DAW) to record with plugins/amp sims. Import the take when done.",
                           juce::dontSendNotification);
    recordingHint.setJustificationType (juce::Justification::centred);
    recordingHint.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.88f));
    recordingPage.addAndMakeVisible (recordingHint);

    recOpenProjectButton.onClick = [this]
    {
        if (onRecordingChoice)
            onRecordingChoice (RecordingChoice::openProject);
    };
    recOpenBackingButton.onClick = [this]
    {
        if (onRecordingChoice)
            onRecordingChoice (RecordingChoice::openBackingTrack);
    };
    recEmptyButton.onClick = [this]
    {
        if (onRecordingChoice)
            onRecordingChoice (RecordingChoice::emptySession);
    };
    recordingBackButton.onClick = [this] { showModePage(); };

    recordingPage.addAndMakeVisible (recOpenProjectButton);
    recordingPage.addAndMakeVisible (recOpenBackingButton);
    recordingPage.addAndMakeVisible (recEmptyButton);
    recordingPage.addAndMakeVisible (recordingBackButton);

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

void StartupWizard::setRecordingChoiceCallback (RecordingChoiceCallback cb)
{
    onRecordingChoice = std::move (cb);
}

void StartupWizard::showModePage()
{
    showPage (0);
}

void StartupWizard::showPracticePage()
{
    showPage (1);
}

void StartupWizard::showRecordingPage()
{
    showPage (2);
}

void StartupWizard::showPage (const int pageIndex)
{
    currentPage = pageIndex;
    modePage.setVisible (pageIndex == 0);
    practicePage.setVisible (pageIndex == 1);
    recordingPage.setVisible (pageIndex == 2);
    titleLabel.setVisible (false);
    subtitleLabel.setVisible (false);
    resized();
    repaint();
}

void StartupWizard::paint (juce::Graphics& g)
{
    const auto colours = JamStudioTheme::getColours();
    const auto bounds = getLocalBounds();

    // Full-bleed wizard background photo (cover-fit), free-floating UI on top.
    if (wizardBackground.isValid())
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (wizardBackground, bounds.toFloat(),
                     juce::RectanglePlacement::centred | juce::RectanglePlacement::fillDestination);

        // Darken so floating tiles + white title stay readable.
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillRect (bounds);

        // Soft vignette toward edges
        g.setGradientFill (juce::ColourGradient (juce::Colours::transparentBlack,
                                                 bounds.getCentreX(), bounds.getCentreY(),
                                                 juce::Colours::black.withAlpha (0.35f),
                                                 0.0f, 0.0f, true));
        g.fillRect (bounds);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (colours.windowBackground,
                                                 0.0f, 0.0f,
                                                 colours.windowBackground.darker (0.12f),
                                                 0.0f, static_cast<float> (getHeight()),
                                                 false));
        g.fillAll();
    }

}

void StartupWizard::layoutHorizontalCards (juce::Rectangle<int> area,
                                           const std::vector<juce::Component*>& cards,
                                           const int gap)
{
    if (cards.empty() || area.isEmpty())
        return;

    const int n = static_cast<int> (cards.size());
    const int maxSide = juce::jmin (area.getHeight(),
                                    (area.getWidth() - gap * (n - 1)) / n);
    // Large free-floating tiles (slightly smaller when 4 modes).
    const int side = juce::jlimit (110, 220, maxSide);
    const int totalW = n * side + (n - 1) * gap;
    auto row = juce::Rectangle<int> (totalW, side).withCentre (area.getCentre());

    for (auto* card : cards)
    {
        if (card != nullptr)
            card->setBounds (row.removeFromLeft (side));
        row.removeFromLeft (gap);
    }
}

void StartupWizard::resized()
{
    // Free-floating tiles centered - no welcome text taking vertical space.
    auto outer = getLocalBounds().reduced (juce::jmax (20, getWidth() / 16),
                                           juce::jmax (20, getHeight() / 14));

    titleLabel.setBounds ({});
    subtitleLabel.setBounds ({});

    modePage.setBounds (outer);
    practicePage.setBounds (outer);
    recordingPage.setBounds (outer);

    // Mode: large equal squares floating horizontally
    {
        auto area = modePage.getLocalBounds().reduced (8, 8);
        layoutHorizontalCards (area,
                               { &practiceButton, &performanceButton, &stageShowButton, &recordingButton },
                               20);
    }

    // Practice page
    {
        auto area = practicePage.getLocalBounds().reduced (8, 8);
        practiceTitle.setBounds (area.removeFromTop (32));
        area.removeFromTop (4);
        practiceHint.setBounds (area.removeFromTop (36));
        area.removeFromTop (10);

        layoutHorizontalCards (area,
                               { &practiceBackButton, &openProjectButton, &newSongButton },
                               28);
    }

    // Recording page
    {
        auto area = recordingPage.getLocalBounds().reduced (8, 8);
        recordingTitle.setBounds (area.removeFromTop (32));
        area.removeFromTop (4);
        recordingHint.setBounds (area.removeFromTop (40));
        area.removeFromTop (10);

        layoutHorizontalCards (area,
                               { &recordingBackButton, &recOpenProjectButton,
                                 &recOpenBackingButton, &recEmptyButton },
                               18);
    }
}

} // namespace jamstudio::ui
