#include "AiToolsSetupDialog.h"

namespace jamstudio::ui
{

class AiToolsSetupDialog::ToolRow : public juce::Component
{
public:
    explicit ToolRow (const jamstudio::ai::AiToolInfo& tool)
    {
        nameLabel.setText (tool.name, juce::dontSendNotification);
        nameLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        addAndMakeVisible (nameLabel);

        statusLabel.setText (tool.available ? "Ready" : "Not installed", juce::dontSendNotification);
        statusLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
        statusLabel.setColour (juce::Label::textColourId,
                               tool.available ? JamStudioTheme::getColours().indicatorOn
                                              : JamStudioTheme::getColours().indicatorMute);
        addAndMakeVisible (statusLabel);

        purposeLabel.setText (tool.purpose, juce::dontSendNotification);
        purposeLabel.setFont (juce::FontOptions (12.0f));
        addAndMakeVisible (purposeLabel);

        if (! tool.available)
        {
            installLabel.setText ("Install: " + tool.installCommand, juce::dontSendNotification);
            installLabel.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            addAndMakeVisible (installLabel);
        }

        if (tool.notes.isNotEmpty())
        {
            notesLabel.setText (tool.notes, juce::dontSendNotification);
            notesLabel.setFont (juce::FontOptions (11.0f));
            addAndMakeVisible (notesLabel);
        }
    }

    void paint (juce::Graphics& g) override
    {
        const auto colours = JamStudioTheme::getColours();
        g.setColour (colours.border);
        g.drawRect (getLocalBounds(), 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (8);
        auto header = bounds.removeFromTop (20);
        nameLabel.setBounds (header.removeFromLeft (120));
        statusLabel.setBounds (header);
        bounds.removeFromTop (4);
        purposeLabel.setBounds (bounds.removeFromTop (18));
        bounds.removeFromTop (2);

        if (installLabel.isVisible())
        {
            installLabel.setBounds (bounds.removeFromTop (18));
            bounds.removeFromTop (2);
        }

        if (notesLabel.isVisible())
            notesLabel.setBounds (bounds.removeFromTop (32));
    }

    [[nodiscard]] int getPreferredHeight() const
    {
        auto height = 46;

        if (installLabel.isVisible())
            height += 20;

        if (notesLabel.isVisible())
            height += 34;

        return height;
    }

private:
    juce::Label nameLabel;
    juce::Label statusLabel;
    juce::Label purposeLabel;
    juce::Label installLabel;
    juce::Label notesLabel;
};

AiToolsSetupDialog::AiToolsSetupDialog (const juce::Array<jamstudio::ai::AiToolInfo>& tools)
{
    titleLabel.setFont (juce::FontOptions (18.0f, juce::Font::bold));
    addAndMakeVisible (titleLabel);

    introLabel.setText ("Optional Python tools power stem separation, AI lyrics, and AI tab transcription.\n"
                        "Install with pipx, then restart JamStudio.",
                        juce::dontSendNotification);
    introLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (introLabel);

    for (const auto& tool : tools)
    {
        auto* row = new ToolRow (tool);
        toolRows.add (row);
        addAndMakeVisible (row);
    }

    closeButton.onClick = [this]
    {
        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (0);
    };
    addAndMakeVisible (closeButton);

    const auto rowHeight = 100;
    const auto totalRowHeight = tools.size() * rowHeight;
    setSize (520, 120 + totalRowHeight + 48);
}

void AiToolsSetupDialog::paint (juce::Graphics& g)
{
    g.fillAll (JamStudioTheme::getColours().panelBackground);
}

void AiToolsSetupDialog::resized()
{
    auto bounds = getLocalBounds().reduced (16);
    titleLabel.setBounds (bounds.removeFromTop (28));
    bounds.removeFromTop (8);
    introLabel.setBounds (bounds.removeFromTop (44));
    bounds.removeFromTop (12);

    for (auto* row : toolRows)
    {
        const auto height = row->getPreferredHeight();
        row->setBounds (bounds.removeFromTop (height).reduced (0, 4));
    }

    bounds.removeFromTop (8);
    closeButton.setBounds (bounds.removeFromTop (30).removeFromRight (90));
}

void AiToolsSetupDialog::show (juce::Component* parent,
                               const juce::Array<jamstudio::ai::AiToolInfo>& tools)
{
    auto dialog = std::make_unique<AiToolsSetupDialog> (tools);
    const auto area = dialog->getLocalBounds();

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = "AI Tools Setup";
    options.dialogBackgroundColour = JamStudioTheme::getColours().panelBackground;
    options.content.setOwned (dialog.release());
    options.componentToCentreAround = parent;
    options.useNativeTitleBar = true;
    options.resizable = false;

    options.launchAsync();
}

} // namespace jamstudio::ui