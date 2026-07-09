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
    void showPage (int pageIndex);

    ModeChosenCallback onModeChosen;
    PracticeChoiceCallback onPracticeChoice;

    juce::Label titleLabel;
    juce::Label subtitleLabel;

    juce::Component modePage;
    juce::TextButton practiceButton { "1. Practice" };
    juce::TextButton performanceButton { "2. Performance" };
    juce::TextButton recordingButton { "3. Recording" };
    juce::Label practiceDesc;
    juce::Label performanceDesc;
    juce::Label recordingDesc;

    juce::Component practicePage;
    juce::Label practiceTitle;
    juce::TextButton openProjectButton { "Open Existing Project" };
    juce::TextButton newSongButton { "New Practice from Song" };
    juce::TextButton practiceBackButton { "Back" };
    juce::Label practiceHint;

    int currentPage = 0;
};

} // namespace jamstudio::ui
