#pragma once

#include "../notation/LyricsTrack.h"
#include "../notation/Score.h"

namespace jamstudio::ui
{

/** Review and correct AI-generated lyrics or tab before applying. */
class TranscriptionCorrectionDialog : public juce::Component
{
public:
    using LyricsCallback = std::function<void (const jamstudio::notation::LyricsTrack&)>;
    using ScoreCallback = std::function<void (const jamstudio::notation::Score&)>;

    static void showLyrics (juce::Component* parent,
                            jamstudio::notation::LyricsTrack draft,
                            LyricsCallback onApply);

    static void showScore (juce::Component* parent,
                           jamstudio::notation::Score draft,
                           ScoreCallback onApply);

private:
    enum class Mode
    {
        lyrics,
        score
    };

    TranscriptionCorrectionDialog (Mode mode,
                                   jamstudio::notation::LyricsTrack lyricsDraft,
                                   jamstudio::notation::Score scoreDraft,
                                   LyricsCallback lyricsCallback,
                                   ScoreCallback scoreCallback);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void applyEdits();
    void dismiss();

    Mode mode;
    jamstudio::notation::LyricsTrack lyricsDraft;
    jamstudio::notation::Score scoreDraft;
    LyricsCallback lyricsCallback;
    ScoreCallback scoreCallback;

    juce::Label titleLabel;
    juce::Label introLabel;
    juce::Label errorLabel;
    juce::Label contentTitleLabel { {}, "Title" };
    juce::TextEditor titleEditor;
    juce::Slider tempoSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxLeft };
    juce::Label tempoLabel { {}, "Tempo" };
    juce::Viewport lyricsViewport;
    juce::Component lyricsContainer;
    juce::OwnedArray<juce::TextEditor> lyricLineEditors;
    juce::OwnedArray<juce::Label> lyricTimeLabels;
    juce::TextEditor tabTextEditor;
    juce::TextButton applyButton { "Apply" };
    juce::TextButton cancelButton { "Cancel" };
};

} // namespace jamstudio::ui