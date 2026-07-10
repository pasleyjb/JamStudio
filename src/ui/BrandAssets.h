#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

/** Embedded JamStudio brand artwork (mic logo). */
struct BrandAssets
{
    /** Full portrait brand image for wizard splash / about. */
    [[nodiscard]] static juce::Image loadSplashImage();

    /** Square brand mark for window / taskbar icons. */
    [[nodiscard]] static juce::Image loadWindowIcon (int preferredSize = 256);

    /** Practice wizard tile - Gibson open-book headstock art. */
    [[nodiscard]] static juce::Image loadPracticeIcon();

    /** Full-bleed art for startup splash + wizard background. */
    [[nodiscard]] static juce::Image loadWizardBackground();

    /** True when assets embedded successfully. */
    [[nodiscard]] static bool isAvailable();
};

} // namespace jamstudio::ui
