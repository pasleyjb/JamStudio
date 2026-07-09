#include "TranscriptionCorrectionDialog.h"

#include "../notation/TranscriptionEditor.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

namespace
{
juce::String formatTimestamp (const double seconds)
{
    const auto totalMs = static_cast<int> (seconds * 1000.0);
    const auto mins = totalMs / 60000;
    const auto secs = (totalMs % 60000) / 1000;
    const auto ms = (totalMs % 1000) / 10;
    return juce::String::formatted ("%d:%02d.%02d", mins, secs, ms);
}
} // namespace

TranscriptionCorrectionDialog::TranscriptionCorrectionDialog (const Mode dialogMode,
                                                                jamstudio::notation::LyricsTrack lyrics,
                                                                jamstudio::notation::Score score,
                                                                LyricsCallback onLyricsApply,
                                                                ScoreCallback onScoreApply)
    : mode (dialogMode),
      lyricsDraft (std::move (lyrics)),
      scoreDraft (std::move (score)),
      lyricsCallback (std::move (onLyricsApply)),
      scoreCallback (std::move (onScoreApply))
{
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    errorLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    errorLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (errorLabel);

    addAndMakeVisible (contentTitleLabel);
    titleEditor.setMultiLine (false);
    titleEditor.setReturnKeyStartsNewLine (false);
    addAndMakeVisible (titleEditor);

    tempoSlider.setRange (40.0, 240.0, 1.0);
    addAndMakeVisible (tempoSlider);
    addAndMakeVisible (tempoLabel);

    addAndMakeVisible (lyricsViewport);
    lyricsViewport.setViewedComponent (&lyricsContainer, false);
    lyricsViewport.setScrollBarsShown (true, false);

    tabTextEditor.setMultiLine (true);
    tabTextEditor.setReturnKeyStartsNewLine (true);
    tabTextEditor.setFont (juce::FontOptions (13.0f).withStyle ("Monospaced"));
    addAndMakeVisible (tabTextEditor);

    applyButton.onClick = [this] { applyEdits(); };
    addAndMakeVisible (applyButton);

    cancelButton.onClick = [this] { dismiss(); };
    addAndMakeVisible (cancelButton);

    if (mode == Mode::lyrics)
    {
        titleLabel.setText ("Review AI Lyrics", juce::dontSendNotification);
        introLabel.setText ("Fix transcription mistakes before saving. Timestamps are preserved.",
                            juce::dontSendNotification);
        titleEditor.setText (lyricsDraft.getTitle(), juce::dontSendNotification);

        for (int i = 0; i < lyricsDraft.getNumLines(); ++i)
        {
            if (const auto* line = lyricsDraft.getLine (i))
            {
                auto* timeLabel = new juce::Label();
                timeLabel->setText (formatTimestamp (line->startSeconds), juce::dontSendNotification);
                timeLabel->setFont (juce::FontOptions (12.0f).withStyle ("Monospaced"));
                timeLabel->setJustificationType (juce::Justification::centredRight);
                lyricTimeLabels.add (timeLabel);
                lyricsContainer.addAndMakeVisible (timeLabel);

                auto* editor = new juce::TextEditor();
                editor->setMultiLine (false);
                editor->setReturnKeyStartsNewLine (false);
                editor->setText (line->text, juce::dontSendNotification);
                lyricLineEditors.add (editor);
                lyricsContainer.addAndMakeVisible (editor);
            }
        }

        const auto lineHeight = 30;
        lyricsContainer.setSize (480, juce::jmax (lineHeight, lyricsDraft.getNumLines() * lineHeight));
        tabTextEditor.setVisible (false);
        tempoSlider.setVisible (false);
        tempoLabel.setVisible (false);

        const auto height = juce::jlimit (320, 640, 180 + lyricsDraft.getNumLines() * lineHeight);
        setSize (560, height);
    }
    else
    {
        titleLabel.setText ("Review AI Tab", juce::dontSendNotification);
        introLabel.setText ("Edit title, tempo, and tab notes (fret/string per measure, e.g. M1: 3/2 5/1).",
                            juce::dontSendNotification);
        titleEditor.setText (scoreDraft.getTitle(), juce::dontSendNotification);
        tempoSlider.setValue (scoreDraft.getTempo(), juce::dontSendNotification);
        tabTextEditor.setText (jamstudio::notation::TranscriptionEditor::scoreToEditableTabText (scoreDraft),
                               juce::dontSendNotification);

        lyricsViewport.setVisible (false);
        setSize (620, 520);
    }
}

void TranscriptionCorrectionDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void TranscriptionCorrectionDialog::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (6);
    introLabel.setBounds (bounds.removeFromTop (36));
    bounds.removeFromTop (8);

    auto titleRow = bounds.removeFromTop (28);
    contentTitleLabel.setBounds (titleRow.removeFromLeft (44));
    titleRow.removeFromLeft (8);
    titleEditor.setBounds (titleRow);

    bounds.removeFromTop (8);

    if (mode == Mode::score)
    {
        auto tempoRow = bounds.removeFromTop (28);
        tempoLabel.setBounds (tempoRow.removeFromLeft (52));
        tempoSlider.setBounds (tempoRow);
        bounds.removeFromTop (8);
    }

    errorLabel.setBounds (bounds.removeFromTop (18));
    bounds.removeFromTop (4);

    auto buttonRow = bounds.removeFromBottom (32);
    cancelButton.setBounds (buttonRow.removeFromRight (90));
    buttonRow.removeFromRight (8);
    applyButton.setBounds (buttonRow.removeFromRight (90));

    if (mode == Mode::lyrics)
    {
        lyricsViewport.setBounds (bounds);

        const auto lineHeight = 30;
        const auto width = juce::jmax (lyricsViewport.getWidth(), 480);
        lyricsContainer.setSize (width, juce::jmax (lineHeight, lyricLineEditors.size() * lineHeight));

        auto lineBounds = lyricsContainer.getLocalBounds();

        for (int i = 0; i < lyricLineEditors.size(); ++i)
        {
            auto row = lineBounds.removeFromTop (lineHeight).reduced (0, 2);
            lyricTimeLabels[i]->setBounds (row.removeFromLeft (72));
            row.removeFromLeft (6);
            lyricLineEditors[i]->setBounds (row);
        }
    }
    else
    {
        tabTextEditor.setBounds (bounds);
    }
}

void TranscriptionCorrectionDialog::applyEdits()
{
    errorLabel.setText ({}, juce::dontSendNotification);

    if (mode == Mode::lyrics)
    {
        juce::StringArray lineTexts;

        for (auto* editor : lyricLineEditors)
            lineTexts.add (editor->getText());

        auto corrected = lyricsDraft;
        jamstudio::notation::TranscriptionEditor::applyLyricLineTexts (corrected,
                                                                         titleEditor.getText().trim(),
                                                                         lineTexts);

        if (corrected.isEmpty())
        {
            errorLabel.setText ("At least one lyric line is required.", juce::dontSendNotification);
            return;
        }

        if (lyricsCallback != nullptr)
            lyricsCallback (corrected);

        dismiss();
        return;
    }

    jamstudio::notation::Score corrected;
    juce::String error;

    if (! jamstudio::notation::TranscriptionEditor::scoreFromEditableTabText (tabTextEditor.getText(),
                                                                              scoreDraft,
                                                                              corrected,
                                                                              error))
    {
        errorLabel.setText (error, juce::dontSendNotification);
        return;
    }

    corrected.setTitle (titleEditor.getText().trim());
    corrected.setTempo (tempoSlider.getValue());

    if (scoreCallback != nullptr)
        scoreCallback (corrected);

    dismiss();
}

void TranscriptionCorrectionDialog::dismiss()
{
    // Only close this dialog — never request application quit.
    if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
    {
        dw->exitModalState (0);
        return;
    }

    setVisible (false);
}

void TranscriptionCorrectionDialog::showLyrics (juce::Component* parent,
                                                jamstudio::notation::LyricsTrack draft,
                                                LyricsCallback onApply)
{
    auto* dialog = new TranscriptionCorrectionDialog (Mode::lyrics,
                                                      std::move (draft),
                                                      jamstudio::notation::Score {},
                                                      std::move (onApply),
                                                      nullptr);

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Review AI Lyrics";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog);
    options.componentToCentreAround = parent;
    // Non-native title bar: on some Linux desktops a native dialog close can quit the app.
    options.useNativeTitleBar = false;
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.launchAsync();
}

void TranscriptionCorrectionDialog::showScore (juce::Component* parent,
                                             jamstudio::notation::Score draft,
                                             ScoreCallback onApply)
{
    auto* dialog = new TranscriptionCorrectionDialog (Mode::score,
                                                      jamstudio::notation::LyricsTrack {},
                                                      std::move (draft),
                                                      nullptr,
                                                      std::move (onApply));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Review AI Tab";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog);
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = false;
    options.escapeKeyTriggersCloseButton = true;
    options.resizable = true;
    options.launchAsync();
}

} // namespace jamstudio::ui