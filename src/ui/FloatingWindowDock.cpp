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

/** Keep a window rectangle mostly visible on the multi-display desktop. */
juce::Rectangle<int> clampToDesktop (juce::Rectangle<int> r)
{
    auto total = juce::Desktop::getInstance().getDisplays().getTotalBounds (true);
    if (total.isEmpty())
        return r;

    // Prefer keeping the title bar reachable.
    const int minVisible = 48;
    if (r.getRight() < total.getX() + minVisible)
        r.setX (total.getX() + minVisible - r.getWidth());
    if (r.getX() > total.getRight() - minVisible)
        r.setX (total.getRight() - minVisible);
    if (r.getBottom() < total.getY() + minVisible)
        r.setY (total.getY() + minVisible - r.getHeight());
    if (r.getY() > total.getBottom() - minVisible)
        r.setY (total.getBottom() - minVisible);
    return r;
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
    // If both windows are already open, pick the side that matches their relative pose.
    if (bothVisible())
        settings.dockSide = inferDockSideFromGeometry();

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

FloatingDockSettings::Side FloatingWindowDock::inferDockSideFromGeometry() const
{
    if (! bothVisible())
        return settings.dockSide;

    const auto m = mixerWindow->getBounds();
    const auto s = stageWindow->getBounds();
    const auto thr = juce::jmax (settings.snapThresholdPx, settings.gapPx + 8);

    // Distance from stage edge to the corresponding mixer edge (lower = better match).
    const int distRight = std::abs (s.getX() - (m.getRight() + settings.gapPx));
    const int distLeft = std::abs (s.getRight() - (m.getX() - settings.gapPx));
    const int distBottom = std::abs (s.getY() - (m.getBottom() + settings.gapPx));
    const int distTop = std::abs (s.getBottom() - (m.getY() - settings.gapPx));

    // Prefer sides where the windows roughly share the other axis (overlap).
    const bool overlapY = s.getY() < m.getBottom() + thr && s.getBottom() > m.getY() - thr;
    const bool overlapX = s.getX() < m.getRight() + thr && s.getRight() > m.getX() - thr;

    struct Candidate
    {
        FloatingDockSettings::Side side;
        int score;
    };

    // Lower score wins. Bonus when the pair already overlaps on the shared axis.
    Candidate cands[] = {
        { FloatingDockSettings::Side::right, distRight + (overlapY ? 0 : 200) },
        { FloatingDockSettings::Side::left, distLeft + (overlapY ? 0 : 200) },
        { FloatingDockSettings::Side::bottom, distBottom + (overlapX ? 0 : 200) },
        { FloatingDockSettings::Side::top, distTop + (overlapX ? 0 : 200) },
    };

    auto best = cands[0];
    for (const auto& c : cands)
        if (c.score < best.score)
            best = c;

    return best.side;
}

juce::Rectangle<int> FloatingWindowDock::stageBoundsForMixer (const juce::Rectangle<int> mixerBounds) const
{
    if (stageWindow == nullptr)
        return {};

    const auto w = stageWindow->getWidth() > 0 ? stageWindow->getWidth() : 560;
    const auto h = stageWindow->getHeight() > 0 ? stageWindow->getHeight() : 640;
    const auto g = settings.gapPx;

    juce::Rectangle<int> r;

    switch (settings.dockSide)
    {
        case FloatingDockSettings::Side::left:
            // Align tops; sit to the left of mixer
            r = { mixerBounds.getX() - w - g, mixerBounds.getY(), w, h };
            break;
        case FloatingDockSettings::Side::right:
            r = { mixerBounds.getRight() + g, mixerBounds.getY(), w, h };
            break;
        case FloatingDockSettings::Side::top:
            // Center horizontally on mixer when docking above/below
            r = { mixerBounds.getCentreX() - w / 2, mixerBounds.getY() - h - g, w, h };
            break;
        case FloatingDockSettings::Side::bottom:
            r = { mixerBounds.getCentreX() - w / 2, mixerBounds.getBottom() + g, w, h };
            break;
    }

    return clampToDesktop (r);
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

    // Expand both by snap threshold so near-miss top/bottom edges also count.
    const auto thr = settings.snapThresholdPx;
    const auto a = mixerWindow->getBounds().expanded (thr);
    return a.intersects (stageWindow->getBounds().expanded (thr / 2));
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

    // Stage moved while sticky: apply same delta to mixer, then re-dock stage
    // so top/bottom/left/right gap + alignment stay exact.
    const auto oldStage = lastStageBounds.isEmpty() ? stageWindow->getBounds() : lastStageBounds;
    const auto dx = newStageBounds.getX() - oldStage.getX();
    const auto dy = newStageBounds.getY() - oldStage.getY();

    withSuppressed ([this, dx, dy]
    {
        auto mixerBounds = mixerWindow->getBounds().translated (dx, dy);
        mixerWindow->setBounds (clampToDesktop (mixerBounds));
        applyDockLayout();
    });
}

void FloatingWindowDock::componentMovedOrResized (juce::Component& component,
                                                  const bool wasMoved,
                                                  const bool wasResized)
{
    if (suppressCallbacks)
        return;

    // Re-dock on move *and* resize (width/height changes affect top/bottom/left/right placement).
    if (! wasMoved && ! wasResized)
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
            // Only follow pure moves; resizing the stage alone should not shove the mixer.
            if (wasMoved)
                movePairFromStageDrag (stageWindow->getBounds());
            else if (wasResized)
            {
                // Keep attached: re-place stage relative to mixer after stage resize.
                applyDockLayout();
            }
            return;
        }

        // Detached: optional auto-stick when edges touch after drag (all four sides).
        if (settings.autoStickOnTouch && edgesTouching())
        {
            settings.dockSide = inferDockSideFromGeometry();
            attach();
        }
        else
            lastStageBounds = stageWindow->getBounds();
    }
}

void FloatingWindowDock::componentVisibilityChanged (juce::Component&)
{
    onWindowVisibilityChanged();
}

} // namespace jamstudio::ui
