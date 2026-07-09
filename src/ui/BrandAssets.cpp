#include "BrandAssets.h"

#include "JamStudioAssets.h"

namespace jamstudio::ui
{

namespace
{
juce::Image loadFromBinary (const void* data, const size_t numBytes)
{
    if (data == nullptr || numBytes == 0)
        return {};

    return juce::ImageFileFormat::loadFrom (data, numBytes);
}
} // namespace

bool BrandAssets::isAvailable()
{
    return loadSplashImage().isValid() || loadWindowIcon().isValid();
}

juce::Image BrandAssets::loadSplashImage()
{
    // Prefer full portrait JPG (original art).
    auto img = loadFromBinary (JamStudioAssets::JamStudioIcon_jpg,
                               static_cast<size_t> (JamStudioAssets::JamStudioIcon_jpgSize));

    if (! img.isValid())
        img = loadFromBinary (JamStudioAssets::JamStudioIcon256_png,
                              static_cast<size_t> (JamStudioAssets::JamStudioIcon256_pngSize));

    return img;
}

juce::Image BrandAssets::loadWindowIcon (const int preferredSize)
{
    auto img = loadFromBinary (JamStudioAssets::JamStudioIcon256_png,
                               static_cast<size_t> (JamStudioAssets::JamStudioIcon256_pngSize));

    if (! img.isValid())
    {
        // Fallback: square-crop the splash portrait around the mic.
        auto splash = loadSplashImage();

        if (splash.isValid())
        {
            const auto side = juce::jmin (splash.getWidth(), splash.getHeight());
            const auto x = (splash.getWidth() - side) / 2;
            const auto y = (splash.getHeight() - side) / 2;
            img = splash.getClippedImage ({ x, y, side, side });
        }
    }

    if (img.isValid() && preferredSize > 0
        && (img.getWidth() != preferredSize || img.getHeight() != preferredSize))
    {
        return img.rescaled (preferredSize, preferredSize, juce::Graphics::highResamplingQuality);
    }

    return img;
}

juce::Image BrandAssets::loadPracticeIcon()
{
    return loadFromBinary (JamStudioAssets::Practice_jpg,
                           static_cast<size_t> (JamStudioAssets::Practice_jpgSize));
}

juce::Image BrandAssets::loadWizardBackground()
{
    auto img = loadFromBinary (JamStudioAssets::wizardbkgrnd_jpg,
                               static_cast<size_t> (JamStudioAssets::wizardbkgrnd_jpgSize));

    // Fallback so older builds without the asset still look intentional.
    if (! img.isValid())
        img = loadSplashImage();

    return img;
}

} // namespace jamstudio::ui
