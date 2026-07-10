#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** One searchable help topic (title, category, keywords, body). */
struct HelpTopic
{
    juce::String id;
    juce::String title;
    juce::String category;
    juce::String keywords;
    juce::String body;

    [[nodiscard]] bool matches (const juce::String& query) const;
};

/** Built-in instruction list for every major JamStudio function. */
class HelpCatalog
{
public:
    [[nodiscard]] static const juce::Array<HelpTopic>& getAllTopics();
    [[nodiscard]] static juce::StringArray getCategories();
    [[nodiscard]] static juce::Array<HelpTopic> search (const juce::String& query,
                                                        const juce::String& categoryFilter = {});
    [[nodiscard]] static const HelpTopic* findById (const juce::String& id);
};

} // namespace jamstudio::ui
