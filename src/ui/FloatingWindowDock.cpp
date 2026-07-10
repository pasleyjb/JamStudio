#include "FloatingWindowDock.h"

#include "MixerWindow.h"
#include "StageFxControllerWindow.h"

namespace jamstudio::ui
{

namespace
{
juce::File jamStudioConfigDir()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("JamStudio");
    dir.createDirectory();
    return dir;
}
} // namespace

juce::File FloatingDockSettings::getSettingsFile()
{
    return jamStudioConfigDir().getChildFile ("floating-dock.json");
}

juce::String FloatingDockSettings::sideToString (const Side side)
{
    switch (side)
    {
        case Side::left: return "left";
        case Side::right: return "right";
        case Side::top: return "top";
        case Side::bottom: return "bottom";
    }
    return "right";
}

FloatingDockSettings::Side FloatingDockSettings::sideFromString (const juce::String& s)
{
    if (s.equalsIgnoreCase ("left"))
        return Side::left;
    if (s.equalsIgnoreCase ("top"))
        return Side::top;
    if (s.equalsIgnoreCase ("bottom"))
        return Side::bottom;
    return Side::right;
}

void FloatingDockSettings::load()
{
    // Defaults already set on the struct.
    const auto file = getSettingsFile();
    if (! file.existsAsFile())
        return;

    juce::var parsed;
    if (juce::JSON::parse (file.loadFileAsString(), parsed).failed())
        return;

    if (auto* o = parsed.getDynamicObject())
    {
        if (o->hasProperty ("sticky"))
            sticky = static_cast<bool> (o->getProperty ("sticky"));
        if (o->hasProperty ("dockSide"))
            dockSide = sideFromString (o->getProperty ("dockSide").toString());
        if (o->hasProperty ("gapPx"))
            gapPx = juce::jlimit (0, 64, static_cast<int> (o->getProperty ("gapPx")));
        if (o->hasProperty ("snapThresholdPx"))
            snapThresholdPx = juce::jlimit (2, 64, static_cast<int> (o->getProperty ("snapThresholdPx")));
        if (o->hasProperty ("autoStickOnTouch"))
            autoStickOnTouch = static_cast<bool> (o->getProperty ("autoStickOnTouch"));
        if (o->hasProperty ("detachOnMaximise"))
            detachOnMaximise = static_cast<bool> (o->getProperty ("detachOnMaximise"));
    }
}

void FloatingDockSettings::save() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("sticky", sticky);
    o->setProperty ("dockSide", sideToString (dockSide));
    o->setProperty ("gapPx", gapPx);
    o->setProperty ("snapThresholdPx", snapThresholdPx);
    o->setProperty ("autoStickOnTouch", autoStickOnTouch);
    o->setProperty ("detachOnMaximise", detachOnMaximise);
    getSettingsFile().replaceWithText (juce::JSON::toString (juce::var (o), true));
}

//==============================================================================
FloatingWindowDock::FloatingWindowDock()
{
    settings.load();
}

FloatingWindowDock::~FloatingWindowDock()
{
    shutdown();
}

void FloatingWindowDock::shutdown()
{
    if (mixerWindow != nullptr)
        mixerWindow->removeComponentListener (this);
    if (stageWindow != nullptr)
        stageWindow->removeComponentListener (this);
    mixerWindow = nullptr;
    stageWindow = nullptr;
}

void FloatingWindowDock::setWindows (MixerWindow* mixer, StageFxControllerWindow* stageFx)
{
    shutdown();
    mixerWindow = mixer;
    stageWindow = stageFx;

    if (mixerWindow != nullptr)
        mixerWindow->addComponentListener (this);
    if (stageWindow != nullptr)
        stageWindow->addComponentListener (this);

    refreshWindowDockButtons();
}

void FloatingWindowDock::setSettings (const FloatingDockSettings& s)
{
    settings = s;
    settings.save();
    refreshWindowDockButtons();

    if (settings.sticky && bothVisible())
        applyDockLayout();
}

void FloatingWindowDock::attach()
{
    settings.sticky = true;
    settings.save();
    applyDockLayout();
    refreshWindowDockButtons();
}

void FloatingWindowDock::detach()
{
    settings.sticky = false;
    settings.save();
    refreshWindowDockButtons();
}

void FloatingWindowDock::refreshWindowDockButtons()
{
    if (mixerWindow != nullptr)
        mixerWindow->setDockStickyState (settings.sticky);
    if (stageWindow != nullptr)
        stageWindow->setDockStickyState (settings.sticky);
}

bool FloatingWindowDock::bothVisible() const
{
    return mixerWindow != nullptr && stageWindow != nullptr
           && mixerWindow->isMixerVisible() && stageWindow->isControllerVisible()
           && mixerWindow->isShowing() && stageWindow->isShowing();
}

juce::Rectangle<int> FloatingWindowDock::stageBoundsForMixer (const juce::Rectangle<int> mixerBounds) const
{
    if (stageWindow == nullptr)
        return {};

    const auto w = stageWindow->getWidth() > 0 ? stageWindow->getWidth() : 500;
    const auto h = stageWindow->getHeight() > 0 ? stageWindow->getHeight() : 440;
    const auto g = settings.gapPx;

    switch (settings.dockSide)
    {
        case FloatingDockSettings::Side::left:
            return { mixerBounds.getX() - w - g, mixerBounds.getY(), w, h };
        case FloatingDockSettings::Side::right:
            return { mixerBounds.getRight() + g, mixerBounds.getY(), w, h };
        case FloatingDockSettings::Side::top:
            return { mixerBounds.getX(), mixerBounds.getY() - h - g, w, h };
        case FloatingDockSettings::Side::bottom:
            return { mixerBounds.getX(), mixerBounds.getBottom() + g, w, h };
    }

    return { mixerBounds.getRight() + g, mixerBounds.getY(), w, h };
}

void FloatingWindowDock::applyDockLayout()
{
    if (! bothVisible() || ! settings.sticky)
        return;

    withSuppressed ([this]
    {
        const auto stageBounds = stageBoundsForMixer (mixerWindow->getBounds());
        stageWindow->setBounds (stageBounds);
        lastStageBounds = stageBounds;
        stageWindow->toFront (false);
    });
}

void FloatingWindowDock::onWindowVisibilityChanged()
{
    refreshWindowDockButtons();
    if (settings.sticky && bothVisible())
        applyDockLayout();
}

void FloatingWindowDock::onMixerMaximised()
{
    if (settings.detachOnMaximise && settings.sticky)
        detach();
}

bool FloatingWindowDock::edgesTouching() const
{
    if (! bothVisible())
        return false;

    const auto a = mixerWindow->getBounds().expanded (settings.snapThresholdPx);
    return a.intersects (stageWindow->getBounds());
}

void FloatingWindowDock::withSuppressed (const std::function<void()>& fn)
{
    const auto prev = suppressCallbacks;
    suppressCallbacks = true;
    fn();
    suppressCallbacks = prev;
}

void FloatingWindowDock::movePairFromStageDrag (const juce::Rectangle<int> newStageBounds)
{
    if (mixerWindow == nullptr || stageWindow == nullptr)
        return;

    // Stage moved while sticky: apply same delta to mixer, then re-dock stage.
    const auto oldStage = lastStageBounds.isEmpty() ? stageWindow->getBounds() : lastStageBounds;
    const auto dx = newStageBounds.getX() - oldStage.getX();
    const auto dy = newStageBounds.getY() - oldStage.getY();

    withSuppressed ([this, dx, dy]
    {
        mixerWindow->setBounds (mixerWindow->getBounds().translated (dx, dy));
        applyDockLayout();
    });
}

void FloatingWindowDock::componentMovedOrResized (juce::Component& component,
                                                  const bool wasMoved,
                                                  const bool /*wasResized*/)
{
    if (suppressCallbacks || ! wasMoved)
        return;

    if (! bothVisible())
        return;

    if (&component == mixerWindow)
    {
        if (settings.sticky)
            applyDockLayout();
        return;
    }

    if (&component == stageWindow)
    {
        if (settings.sticky)
        {
            movePairFromStageDrag (stageWindow->getBounds());
            return;
        }

        // Detached: optional auto-stick when edges touch after drag.
        if (settings.autoStickOnTouch && edgesTouching())
            attach();
        else
            lastStageBounds = stageWindow->getBounds();
    }
}

void FloatingWindowDock::componentVisibilityChanged (juce::Component&)
{
    onWindowVisibilityChanged();
}

} // namespace jamstudio::ui
