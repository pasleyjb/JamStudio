#pragma once

#include "../notation/OnlineLyricsClient.h"
#include "../notation/SongMetadata.h"

namespace jamstudio::ui
{

/** Search online synced LRC lyrics and let the user pick a match. */
class OnlineLyricsDialog : public juce::Component,
                           private juce::ListBoxModel
{
public:
    using LyricsChosenCallback = std::function<void (jamstudio::notation::LyricsTrack lyrics)>;

    static void show (juce::Component* parent,
                      jamstudio::notation::SongMetadata metadata,
                      LyricsChosenCallback onChosen);

private:
    OnlineLyricsDialog (jamstudio::notation::SongMetadata metadata, LyricsChosenCallback onChosen);

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getNumRows() override;
    void paintListBoxItem (int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

    void runSearch();
    void applySelection();
    void dismiss();
    void setBusy (bool busy, const juce::String& status);

    jamstudio::notation::SongMetadata metadata;
    LyricsChosenCallback onChosen;
    jamstudio::notation::OnlineLyricsClient client;
    juce::Array<jamstudio::notation::OnlineLyricsCandidate> results;

    juce::Label titleLabel;
    juce::Label queryLabel;
    juce::TextEditor artistEditor;
    juce::TextEditor titleEditor;
    juce::TextButton searchButton { "Search" };
    juce::ListBox resultsList;
    juce::Label statusLabel;
    juce::TextButton applyButton { "Use Selected" };
    juce::TextButton cancelButton { "Cancel" };
    bool busy = false;
};

} // namespace jamstudio::ui
