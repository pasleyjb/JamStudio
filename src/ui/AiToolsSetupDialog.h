#pragma once

#include "../ai/AiToolsCatalog.h"
#include "JamStudioTheme.h"

namespace jamstudio::ui
{

/** Shows AI dependency status and install commands. */
class AiToolsSetupDialog : public juce::Component
{
public:
    explicit AiToolsSetupDialog (const juce::Array<jamstudio::ai::AiToolInfo>& tools);

    void paint (juce::Graphics& g) override;
    void resized() override;

    static void show (juce::Component* parent,
                                    const juce::Array<jamstudio::ai::AiToolInfo>& tools);

private:
    class ToolRow;

    juce::Label titleLabel { {}, "AI Tools Setup" };
    juce::Label introLabel;
    juce::OwnedArray<ToolRow> toolRows;
    juce::TextButton closeButton { "Close" };
};

} // namespace jamstudio::ui