#include "ToneSelectionDialog.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

class ToneSelectionDialog::ToneListModel : public juce::ListBoxModel
{
public:
    explicit ToneListModel (ToneSelectionDialog& ownerIn) : owner (ownerIn) {}

    int getNumRows() override
    {
        return owner.visibleTones.size();
    }

    void paintListBoxItem (const int rowNumber,
                           juce::Graphics& g,
                           const int width,
                           const int height,
                           const bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (rowNumber, owner.visibleTones.size()))
            return;

        const auto& tone = owner.visibleTones.getReference (rowNumber);
        const auto colours = JamStudioTheme::getColours();

        if (rowIsSelected)
            g.fillAll (colours.accent.withAlpha (0.18f));

        g.setColour (colours.text);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawText (tone.displayName, 8, 2, width - 16, 18, juce::Justification::centredLeft, true);

        g.setFont (juce::FontOptions (12.0f));
        g.setColour (colours.textSecondary);
        const auto sizeKb = juce::String (tone.fileSizeBytes / 1024) + " KB";
        g.drawText (tone.sourceLabel + "  -  " + sizeKb,
                    8, 20, width - 16, 16, juce::Justification::centredLeft, true);
    }

    void listBoxItemDoubleClicked (const int row, const juce::MouseEvent&) override
    {
        if (row >= 0)
        {
            owner.toneList.selectRow (row);
            owner.applySelectedTone();
        }
    }

    void selectedRowsChanged (const int lastRowSelected) override
    {
        if (lastRowSelected >= 0 && lastRowSelected < owner.visibleTones.size())
        {
            const auto& t = owner.visibleTones.getReference (lastRowSelected);
            owner.detailLabel.setText (t.file.getFullPathName(), juce::dontSendNotification);
        }
        else
        {
            owner.detailLabel.setText ({}, juce::dontSendNotification);
        }
    }

private:
    ToneSelectionDialog& owner;
};

ToneSelectionDialog::ToneSelectionDialog (jamstudio::amp::AmpProcessor& ampProcessor,
                                          ToneLoadedCallback onLoaded)
    : amp (ampProcessor),
      loadedCallback (std::move (onLoaded))
{
    listModel = std::make_unique<ToneListModel> (*this);
    toneList.setModel (listModel.get());
    toneList.setRowHeight (42);

    titleLabel.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setText (
        "Choose a guitar amp tone (.nam). Double-click or press Use This Tone to load it "
        "into the live monitor. Put your models in Documents/JamStudio/AmpModels.",
        juce::dontSendNotification);
    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    statusLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (statusLabel);

    searchEditor.setTextToShowWhenEmpty ("Search tones...", juce::Colours::grey);
    searchEditor.onTextChange = [this] { updateFilteredList(); };
    searchEditor.onReturnKey = [this] { updateFilteredList(); };
    addAndMakeVisible (searchEditor);
    addAndMakeVisible (searchLabel);

    addAndMakeVisible (toneList);
    detailLabel.setJustificationType (juce::Justification::topLeft);
    detailLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (detailLabel);

    enabledToggle.onClick = [this]
    {
        amp.setEnabled (enabledToggle.getToggleState());
    };
    bypassToggle.onClick = [this]
    {
        amp.setBypass (bypassToggle.getToggleState());
    };
    addAndMakeVisible (enabledToggle);
    addAndMakeVisible (bypassToggle);

    auto setupGain = [] (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
        s.setRange (-24.0, 24.0, 0.1);
        s.setTextValueSuffix (" dB");
    };
    setupGain (inputGain);
    setupGain (outputGain);
    inputGain.setValue (amp.getInputGainDb(), juce::dontSendNotification);
    outputGain.setValue (amp.getOutputGainDb(), juce::dontSendNotification);
    inputGain.onValueChange = [this]
    {
        amp.setInputGainDb (static_cast<float> (inputGain.getValue()));
    };
    outputGain.onValueChange = [this]
    {
        amp.setOutputGainDb (static_cast<float> (outputGain.getValue()));
    };
    addAndMakeVisible (inputLabel);
    addAndMakeVisible (outputLabel);
    addAndMakeVisible (inputGain);
    addAndMakeVisible (outputGain);

    meterLabel.setJustificationType (juce::Justification::centredLeft);
    meterLabel.setFont (juce::FontOptions (12.0f));
    addAndMakeVisible (meterLabel);

    refreshButton.onClick = [this] { rescanLibrary(); };
    importButton.onClick = [this] { importToneFile(); };
    openFolderButton.onClick = [this]
    {
        jamstudio::amp::AmpModelLibrary::ensureUserLibrary (true);
        openUserLibraryFolder();
    };
    useToneButton.onClick = [this] { applySelectedTone(); };
    closeButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };

    addAndMakeVisible (refreshButton);
    addAndMakeVisible (importButton);
    addAndMakeVisible (openFolderButton);
    addAndMakeVisible (useToneButton);
    addAndMakeVisible (closeButton);

    setSize (720, 560);
    refreshControlsFromAmp();
    rescanLibrary();
    startTimerHz (8);
}

ToneSelectionDialog::~ToneSelectionDialog()
{
    stopTimer();
}

void ToneSelectionDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void ToneSelectionDialog::resized()
{
    auto area = getLocalBounds().reduced (16);
    titleLabel.setBounds (area.removeFromTop (28));
    area.removeFromTop (4);
    introLabel.setBounds (area.removeFromTop (44));
    area.removeFromTop (6);

    auto searchRow = area.removeFromTop (28);
    searchLabel.setBounds (searchRow.removeFromLeft (56));
    searchEditor.setBounds (searchRow);

    area.removeFromTop (8);
    auto bottom = area.removeFromBottom (160);
    area.removeFromBottom (8);

    toneList.setBounds (area.removeFromTop (area.getHeight() - 36));
    area.removeFromTop (4);
    detailLabel.setBounds (area);

    // Bottom controls
    auto toggles = bottom.removeFromTop (28);
    enabledToggle.setBounds (toggles.removeFromLeft (180));
    toggles.removeFromLeft (12);
    bypassToggle.setBounds (toggles.removeFromLeft (160));

    auto gainRow = bottom.removeFromTop (28);
    inputLabel.setBounds (gainRow.removeFromLeft (48));
    inputGain.setBounds (gainRow.removeFromLeft (gainRow.getWidth() / 2 - 8));
    gainRow.removeFromLeft (8);
    outputLabel.setBounds (gainRow.removeFromLeft (52));
    outputGain.setBounds (gainRow);

    meterLabel.setBounds (bottom.removeFromTop (22));
    bottom.removeFromTop (6);

    auto buttons = bottom.removeFromTop (32);
    const int bw = 110;
    refreshButton.setBounds (buttons.removeFromLeft (bw).reduced (2));
    importButton.setBounds (buttons.removeFromLeft (bw + 20).reduced (2));
    openFolderButton.setBounds (buttons.removeFromLeft (bw + 10).reduced (2));
    closeButton.setBounds (buttons.removeFromRight (bw).reduced (2));
    useToneButton.setBounds (buttons.removeFromRight (bw + 20).reduced (2));
}

void ToneSelectionDialog::timerCallback()
{
    refreshControlsFromAmp();

    if (! loading)
    {
        const auto inPeak = amp.getInputPeak();
        const auto outPeak = amp.getOutputPeak();
        const auto model = amp.getEngine().isLoaded()
                               ? amp.getEngine().getModelDisplayName()
                               : juce::String ("(none)");
        meterLabel.setText ("Active: " + model
                                + "   In peak: " + juce::String (inPeak, 3)
                                + "   Out peak: " + juce::String (outPeak, 3),
                            juce::dontSendNotification);
    }
}

void ToneSelectionDialog::refreshControlsFromAmp()
{
    enabledToggle.setToggleState (amp.isEnabled(), juce::dontSendNotification);
    bypassToggle.setToggleState (amp.isBypassed(), juce::dontSendNotification);
}

void ToneSelectionDialog::rescanLibrary()
{
    library.rescan();
    updateFilteredList();
    statusLabel.setText (juce::String (library.getTones().size()) + " tones found",
                         juce::dontSendNotification);
}

void ToneSelectionDialog::updateFilteredList()
{
    visibleTones = library.filter (searchEditor.getText());
    toneList.updateContent();
    toneList.repaint();

    // Highlight currently loaded model if present
    const auto current = amp.getEngine().getModelFile();
    if (current.existsAsFile())
    {
        for (int i = 0; i < visibleTones.size(); ++i)
        {
            if (visibleTones.getReference (i).file == current)
            {
                toneList.selectRow (i);
                break;
            }
        }
    }
}

const jamstudio::amp::AmpToneInfo* ToneSelectionDialog::getSelectedTone() const
{
    const int row = toneList.getSelectedRow();
    if (! juce::isPositiveAndBelow (row, visibleTones.size()))
        return nullptr;
    return &visibleTones.getReference (row);
}

void ToneSelectionDialog::applySelectedTone()
{
    const auto* tone = getSelectedTone();
    if (tone == nullptr)
    {
        statusLabel.setText ("Select a tone first.", juce::dontSendNotification);
        return;
    }

    loading = true;
    useToneButton.setEnabled (false);
    statusLabel.setText ("Loading " + tone->displayName + "...", juce::dontSendNotification);

    const auto toneCopy = *tone;
    amp.loadModelAsync (toneCopy.file, [this, toneCopy] (const bool ok, const juce::String& error)
    {
        loading = false;
        useToneButton.setEnabled (true);

        if (ok)
        {
            amp.setEnabled (true);
            amp.setBypass (false);
            refreshControlsFromAmp();
            statusLabel.setText ("Loaded: " + toneCopy.displayName + " - play your guitar to hear it",
                                 juce::dontSendNotification);
            if (loadedCallback)
                loadedCallback (toneCopy);
        }
        else
        {
            statusLabel.setText ("Load failed: " + error, juce::dontSendNotification);
        }
    });
}

void ToneSelectionDialog::importToneFile()
{
    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;

    fileChooser = std::make_unique<juce::FileChooser> (
        "Import Neural Amp Model (.nam)",
        jamstudio::amp::AmpModelLibrary::getUserAmpModelsDirectory(),
        "*.nam");

    fileChooser->launchAsync (flags, [this] (const juce::FileChooser& chooser)
    {
        const auto file = chooser.getResult();
        if (! file.existsAsFile())
            return;

        const auto imported = jamstudio::amp::AmpModelLibrary::importIntoUserLibrary (file);
        if (! imported.existsAsFile())
        {
            statusLabel.setText ("Could not import file.", juce::dontSendNotification);
            return;
        }

        rescanLibrary();

        // Select imported
        for (int i = 0; i < visibleTones.size(); ++i)
        {
            if (visibleTones.getReference (i).file == imported)
            {
                toneList.selectRow (i);
                break;
            }
        }

        statusLabel.setText ("Imported " + imported.getFileName() + " - press Use This Tone",
                             juce::dontSendNotification);
    });
}

void ToneSelectionDialog::openUserLibraryFolder()
{
    const auto dir = jamstudio::amp::AmpModelLibrary::getUserAmpModelsDirectory();
    dir.createDirectory();
    dir.revealToUser();
}

void ToneSelectionDialog::show (juce::Component* parent,
                                jamstudio::amp::AmpProcessor& ampProcessor,
                                ToneLoadedCallback onLoaded)
{
    auto dialog = std::make_unique<ToneSelectionDialog> (ampProcessor, std::move (onLoaded));

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "Tone Selection Mode";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog.release());
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.useBottomRightCornerResizer = true;

    options.launchAsync();
}

} // namespace jamstudio::ui
