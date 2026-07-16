#pragma once

#include <JuceHeader.h>

namespace jamstudio::ui
{

class MixerWindow;
class StageFxControllerWindow;

/** User-facing options for sticky Mixer + Stage FX Controller pairing. */
struct FloatingDockSettings
{
    enum class Side
    {
        left = 0,
        right,
        top,
        bottom
    };

    /** Currently stuck together (controller follows mixer). Default: on. */
    bool sticky = true;
    /** Which side of the mixer the Stage FX Controller docks to. Default: right. */
    Side dockSide = Side::right;
    /** Gap in pixels between docked windows. */
    int gapPx = 4;
    /** Edge distance for optional auto-stick when dragging. */
    int snapThresholdPx = 12;
    /** If true, releasing a drag near the other window re-attaches. Default: off. */
    bool autoStickOnTouch = false;
    /** If true, maximising the mixer detaches the pair. Default: on. */
    bool detachOnMaximise = true;

    void load();
    void save() const;

    [[nodiscard]] static juce::File getSettingsFile();
    [[nodiscard]] static juce::String sideToString (Side side);
    [[nodiscard]] static Side sideFromString (const juce::String& s);
};

/**
 * Coordinates sticky docking between Mixer (parent) and Stage FX Controller.
 * Moving the mixer repositions the controller; dragging the controller moves both.
 */
class FloatingWindowDock : private juce::ComponentListener
{
public:
    FloatingWindowDock();
    ~FloatingWindowDock() override;

    void setWindows (MixerWindow* mixer, StageFxControllerWindow* stageFx);
    void shutdown();

    [[nodiscard]] FloatingDockSettings& getSettings() noexcept { return settings; }
    [[nodiscard]] const FloatingDockSettings& getSettings() const noexcept { return settings; }
    void setSettings (const FloatingDockSettings& s);
    void saveSettings() const { settings.save(); }

    void attach();
    void detach();
    [[nodiscard]] bool isSticky() const noexcept { return settings.sticky; }

    /** Place Stage FX relative to mixer using current dock side/gap. */
    void applyDockLayout();

    /** Call after either window is shown so sticky layout can run. */
    void onWindowVisibilityChanged();

    /** Mixer maximised - honour detachOnMaximise. */
    void onMixerMaximised();

    void refreshWindowDockButtons();

private:
    void componentMovedOrResized (juce::Component& component,
                                  bool wasMoved,
                                  bool wasResized) override;
    void componentVisibilityChanged (juce::Component& component) override;

    [[nodiscard]] bool bothVisible() const;
    [[nodiscard]] bool edgesTouching() const;
    /** Pick left/right/top/bottom from current relative window positions. */
    [[nodiscard]] FloatingDockSettings::Side inferDockSideFromGeometry() const;
    [[nodiscard]] juce::Rectangle<int> stageBoundsForMixer (juce::Rectangle<int> mixerBounds) const;
    void movePairFromStageDrag (juce::Rectangle<int> newStageBounds);
    void withSuppressed (const std::function<void()>& fn);

    MixerWindow* mixerWindow = nullptr;
    StageFxControllerWindow* stageWindow = nullptr;
    FloatingDockSettings settings;
    bool suppressCallbacks = false;
    juce::Rectangle<int> lastStageBounds;
};

} // namespace jamstudio::ui
