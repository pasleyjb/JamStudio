#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** First-run mode picker: Practice, Performance, or Recording. */
class StartupWizard : public juce::Component
{
public:
    enum class Mode
    {
        practice,
        performance,
        stageShowBuilder,
        recording
    };

    enum class PracticeChoice
    {
        openProject,
        newFromSong
    };

    using ModeChosenCallback = std::function<void (Mode)>;
    using PracticeChoiceCallback = std::function<void (PracticeChoice)>;

    StartupWizard();

    void setModeChosenCallback (ModeChosenCallback cb);
    void setPracticeChoiceCallback (PracticeChoiceCallback cb);
    void showModePage();
    void showPracticePage();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    enum class CardIcon
    {
        practice,         // guitar / learning
        performance,      // play / stage
        stageShowBuilder, // video / slideshow set
        recording,        // record disc
        openProject,      // folder
        newSong,          // disc / audio file
        back              // chevron
    };

    /** Square icon tile with caption under the glyph. */
    class IconCardButton : public juce::Button
    {
    public:
        IconCardButton (const juce::String& name,
                        CardIcon icon,
                        const juce::String& caption,
                        const juce::String& detail = {});

        void setCustomIcon (juce::Image image);

        void paintButton (juce::Graphics& g,
                          bool shouldDrawButtonAsHighlighted,
                          bool shouldDrawButtonAsDown) override;

    private:
        void drawIcon (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour colour) const;

        CardIcon iconType;
        juce::String caption;
        juce::String detail;
        juce::Image customIcon;
    };

    void showPage (int pageIndex);
    static void layoutHorizontalCards (juce::Rectangle<int> area,
                                       const std::vector<juce::Component*>& cards,
                                       int gap);

    ModeChosenCallback onModeChosen;
    PracticeChoiceCallback onPracticeChoice;

    juce::Image wizardBackground;
    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::Component modePage;
    IconCardButton practiceButton;
    IconCardButton performanceButton;
    IconCardButton stageShowButton;
    IconCardButton recordingButton;

    juce::Component practicePage;
    juce::Label practiceTitle;
    IconCardButton openProjectButton;
    IconCardButton newSongButton;
    IconCardButton practiceBackButton;
    juce::Label practiceHint;

    int currentPage = 0;
};

} // namespace jamstudio::ui
