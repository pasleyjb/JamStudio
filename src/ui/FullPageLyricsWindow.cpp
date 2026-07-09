#include "FullPageLyricsWindow.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

namespace
{
juce::String formatTimestamp (const double seconds)
{
    if (seconds < 0.0)
        return {};

    const auto total = juce::jmax (0, juce::roundToInt (seconds));
    const auto m = total / 60;
    const auto s = total % 60;
    return juce::String::formatted ("%d:%02d", m, s);
}

juce::String safePrintName (const juce::String& title)
{
    auto t = title.retainCharacters (
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
    t = t.trim();
    return t.isNotEmpty() ? t : "JamStudio-Lyrics";
}
} // namespace

//==============================================================================
/** Printable multi-column / multi-page style lyrics sheet. */
class FullPageLyricsWindow::Content : public juce::Component,
                                      private juce::Timer
{
public:
    explicit Content (jamstudio::audio::TransportController& transport)
        : transportController (transport)
    {
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        titleLabel.setText ("Full Page Lyrics", juce::dontSendNotification);
        addAndMakeVisible (titleLabel);

        hintLabel.setText ("Printable lyric sheet — use Print or Export PNG",
                           juce::dontSendNotification);
        hintLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (hintLabel);

        printButton.onClick = [this] { printSheet(); };
        exportButton.onClick = [this] { exportPng(); };
        closeButton.onClick = [this]
        {
            if (onCloseRequested != nullptr)
                onCloseRequested();
        };
        showTimesButton.setClickingTogglesState (true);
        showTimesButton.setToggleState (false, juce::dontSendNotification);
        showTimesButton.onClick = [this]
        {
            sheet.showTimestamps = showTimesButton.getToggleState();
            sheet.resized();
            sheet.repaint();
            updateSheetSize();
        };

        addAndMakeVisible (printButton);
        addAndMakeVisible (exportButton);
        addAndMakeVisible (closeButton);
        addAndMakeVisible (showTimesButton);

        viewport.setViewedComponent (&sheet, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);

        startTimerHz (20);
    }

    void setCloseCallback (std::function<void()> callback)
    {
        onCloseRequested = std::move (callback);
    }

    void setLyrics (const jamstudio::notation::LyricsTrack& lyrics)
    {
        currentLyrics = lyrics;
        sheet.setLyrics (lyrics);
        titleLabel.setText (lyrics.getTitle().isNotEmpty() ? lyrics.getTitle() : "Untitled lyrics",
                            juce::dontSendNotification);
        updateSheetSize();
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (JamStudioTheme::getColours().windowBackground);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (10);

        auto header = bounds.removeFromTop (34);
        closeButton.setBounds (header.removeFromRight (80).reduced (2));
        header.removeFromRight (6);
        printButton.setBounds (header.removeFromRight (90).reduced (2));
        header.removeFromRight (6);
        exportButton.setBounds (header.removeFromRight (110).reduced (2));
        header.removeFromRight (6);
        showTimesButton.setBounds (header.removeFromRight (100).reduced (2));
        header.removeFromRight (10);
        titleLabel.setBounds (header.removeFromLeft (juce::jmax (160, header.getWidth() / 2)));
        hintLabel.setBounds (header);

        bounds.removeFromTop (6);
        viewport.setBounds (bounds);
        updateSheetSize();
    }

private:
    //==========================================================================
    class LyricsSheet : public juce::Component
    {
    public:
        void setLyrics (const jamstudio::notation::LyricsTrack& track)
        {
            lyrics = track;
            resized();
            repaint();
        }

        void setActiveLine (const int lineIndex)
        {
            if (activeLine == lineIndex)
                return;

            activeLine = lineIndex;
            repaint();
        }

        [[nodiscard]] int getContentHeight() const noexcept
        {
            if (lyrics.isEmpty())
                return 400;

            const auto lineH = lineHeight();
            const auto titleBlock = 72;
            const auto bottomPad = 40;
            return titleBlock + lyrics.getNumLines() * lineH + bottomPad;
        }

        [[nodiscard]] juce::Image renderToImage (const float scale) const
        {
            const auto w = juce::jmax (720, getWidth());
            const auto h = juce::jmax (400, getContentHeight());
            const auto imgW = juce::roundToInt (static_cast<float> (w) * scale);
            const auto imgH = juce::roundToInt (static_cast<float> (h) * scale);

            juce::Image image (juce::Image::RGB, imgW, imgH, true);
            juce::Graphics g (image);
            g.fillAll (juce::Colours::white);
            g.addTransform (juce::AffineTransform::scale (scale));
            paintSheet (g, w, h, true, -1);
            return image;
        }

        void paint (juce::Graphics& g) override
        {
            paintSheet (g, getWidth(), getHeight(), false, activeLine);
        }

        void resized() override
        {
            setSize (juce::jmax (getWidth(), 720), juce::jmax (getHeight(), getContentHeight()));
        }

        bool showTimestamps = false;

    private:
        [[nodiscard]] int lineHeight() const noexcept
        {
            return showTimestamps ? 34 : 30;
        }

        void paintSheet (juce::Graphics& g,
                         const int width,
                         const int /*height*/,
                         const bool forPrint,
                         const int highlightLine) const
        {
            if (forPrint)
                g.fillAll (juce::Colours::white);
            else
                g.fillAll (JamStudioTheme::getColours().notationBackground);

            const auto margin = 48;
            auto area = juce::Rectangle<int> (0, 0, width, getContentHeight()).reduced (margin, 36);

            // Title
            g.setColour (forPrint ? juce::Colours::black : JamStudioTheme::getColours().text);
            g.setFont (juce::FontOptions (26.0f, juce::Font::bold));
            const auto songTitle = lyrics.getTitle().isNotEmpty() ? lyrics.getTitle() : "Lyrics";
            g.drawText (songTitle, area.removeFromTop (36), juce::Justification::centred);

            g.setFont (juce::FontOptions (12.0f));
            g.setColour (forPrint ? juce::Colours::darkgrey
                                  : JamStudioTheme::getColours().textSecondary);
            g.drawText ("JamStudio lyric sheet",
                        area.removeFromTop (22), juce::Justification::centred);
            area.removeFromTop (14);

            if (lyrics.isEmpty())
            {
                g.setColour (forPrint ? juce::Colours::black
                                      : JamStudioTheme::getColours().textSecondary);
                g.setFont (juce::FontOptions (16.0f));
                g.drawText ("No lyrics loaded — import LRC, find online lyrics, or run AI Lyrics.",
                            area, juce::Justification::centred);
                return;
            }

            const auto lineH = lineHeight();
            const auto textColour = forPrint ? juce::Colours::black
                                             : JamStudioTheme::getColours().text;
            const auto mutedColour = forPrint ? juce::Colours::darkgrey
                                              : JamStudioTheme::getColours().textSecondary;

            for (int i = 0; i < lyrics.getNumLines(); ++i)
            {
                if (const auto* line = lyrics.getLine (i))
                {
                    auto row = juce::Rectangle<int> (area.getX(), area.getY() + i * lineH,
                                                     area.getWidth(), lineH);
                    const auto isActive = ! forPrint && i == highlightLine;

                    if (isActive)
                    {
                        g.setColour (JamStudioTheme::getColours().lyricsHighlight.withAlpha (0.18f));
                        g.fillRoundedRectangle (row.toFloat().expanded (4.0f, 1.0f), 6.0f);
                    }

                    if (showTimestamps)
                    {
                        auto timeCol = row.removeFromLeft (56);
                        g.setColour (mutedColour);
                        g.setFont (juce::FontOptions (12.0f));
                        g.drawText (formatTimestamp (line->startSeconds), timeCol,
                                    juce::Justification::centredRight);
                        row.removeFromLeft (12);
                    }

                    g.setColour (isActive ? JamStudioTheme::getColours().lyricsHighlight
                                          : textColour);
                    g.setFont (juce::FontOptions (isActive ? 18.0f : 16.0f,
                                                  isActive ? juce::Font::bold
                                                           : juce::Font::plain));
                    g.drawText (line->text, row, juce::Justification::centredLeft, true);
                }
            }
        }

        jamstudio::notation::LyricsTrack lyrics;
        int activeLine = -1;
    };

    void updateSheetSize()
    {
        const auto pageWidth = juce::jmax (viewport.getWidth(), 720);
        sheet.setSize (pageWidth, juce::jmax (viewport.getHeight(), sheet.getContentHeight()));
    }

    void timerCallback() override
    {
        if (currentLyrics.isEmpty())
            return;

        const auto pos = transportController.getPosition();
        sheet.setActiveLine (currentLyrics.getActiveLineIndex (pos));

        // Keep active line roughly in view while playing
        if (transportController.isPlaying())
        {
            const auto line = currentLyrics.getActiveLineIndex (pos);

            if (line >= 0)
            {
                const auto y = 72 + line * (sheet.showTimestamps ? 34 : 30) - viewport.getHeight() / 3;
                viewport.setViewPosition (0, juce::jmax (0, y));
            }
        }
    }

    void printSheet()
    {
        if (currentLyrics.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Print Lyrics",
                                                    "Load or generate lyrics before printing.");
            return;
        }

        const auto image = sheet.renderToImage (2.0f);
        const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("JamStudio")
                                 .getChildFile ("print");
        tempDir.createDirectory();

        const auto file = tempDir.getChildFile (safePrintName (currentLyrics.getTitle()) + "-lyrics-print.png");

        if (! writePng (image, file))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Print Lyrics",
                                                    "Could not write print image.");
            return;
        }

        juce::StringArray lpCmd;
        lpCmd.add ("/usr/bin/lp");
        lpCmd.add (file.getFullPathName());

        juce::ChildProcess printer;

        if (printer.start (lpCmd))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "Print Lyrics",
                "Sent to the system printer:\n" + file.getFullPathName()
                    + "\n\nIf nothing prints, open the PNG and use your system print dialog.");
            return;
        }

        juce::StringArray openCmd;
        openCmd.add ("/usr/bin/xdg-open");
        openCmd.add (file.getFullPathName());
        juce::ChildProcess opener;
        opener.start (openCmd);

        juce::AlertWindow::showMessageBoxAsync (
            juce::MessageBoxIconType::InfoIcon,
            "Print Lyrics",
            "Opened printable image:\n" + file.getFullPathName()
                + "\n\nUse your image viewer’s Print command (Ctrl+P).");
    }

    void exportPng()
    {
        if (currentLyrics.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Export Lyrics",
                                                    "Load or generate lyrics before exporting.");
            return;
        }

        const auto defaultName = safePrintName (currentLyrics.getTitle()) + "-lyrics.png";

        fileChooser = std::make_unique<juce::FileChooser> (
            "Export lyrics as PNG",
            juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                .getChildFile (defaultName),
            "*.png");

        constexpr auto chooserFlags = juce::FileBrowserComponent::saveMode
                                      | juce::FileBrowserComponent::canSelectFiles
                                      | juce::FileBrowserComponent::warnAboutOverwriting;

        fileChooser->launchAsync (chooserFlags, [this] (const juce::FileChooser& chooser)
        {
            auto file = chooser.getResult();

            if (file == juce::File())
                return;

            if (! file.hasFileExtension (".png"))
                file = file.withFileExtension (".png");

            const auto image = sheet.renderToImage (2.0f);

            if (! writePng (image, file))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Export Lyrics",
                                                        "Failed to write PNG file.");
                return;
            }

            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Export Lyrics",
                                                    "Saved printable PNG:\n" + file.getFullPathName());
        });
    }

    static bool writePng (const juce::Image& image, const juce::File& file)
    {
        file.deleteFile();

        if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
        {
            juce::PNGImageFormat png;
            return png.writeImageToStream (image, *stream);
        }

        return false;
    }

    jamstudio::audio::TransportController& transportController;
    jamstudio::notation::LyricsTrack currentLyrics;
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::TextButton printButton { "Print" };
    juce::TextButton exportButton { "Export PNG" };
    juce::TextButton closeButton { "Close" };
    juce::ToggleButton showTimesButton { "Show times" };
    juce::Viewport viewport;
    LyricsSheet sheet;
    std::unique_ptr<juce::FileChooser> fileChooser;
    std::function<void()> onCloseRequested;
};

//==============================================================================
FullPageLyricsWindow::FullPageLyricsWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("JamStudio — Full Page Lyrics",
                      JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::closeButton),
      transportController (transport)
{
    juce::ignoreUnused (transportController);

    windowOpen = false;
    setVisible (false);
    setWantsKeyboardFocus (false);

    setUsingNativeTitleBar (false);
    content = std::make_unique<Content> (transport);
    content->setCloseCallback ([this] { hideWindow(); });
    setContentNonOwned (content.get(), false);
    setResizable (true, true);
    setResizeLimits (700, 500, 4000, 3000);
    setSize (900, 1000);

    setVisible (false);
    if (isOnDesktop())
        removeFromDesktop();
}

FullPageLyricsWindow::~FullPageLyricsWindow()
{
    hideWindow();
    setContentNonOwned (nullptr, false);
    content.reset();
}

void FullPageLyricsWindow::setLyrics (const jamstudio::notation::LyricsTrack& lyrics)
{
    if (content != nullptr)
        content->setLyrics (lyrics);
}

void FullPageLyricsWindow::showWindow (const bool shouldShow)
{
    if (shouldShow)
    {
        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userBounds.reduced (60.0f, 30.0f).getSmallestIntegerContainer();
            // Prefer a portrait sheet proportion when the display allows it.
            auto sheet = area;
            const auto idealW = juce::jmin (area.getWidth(), juce::roundToInt (area.getHeight() * 0.72f));
            sheet = sheet.withSizeKeepingCentre (idealW, area.getHeight());
            setBounds (sheet);
        }
        else
        {
            centreWithSize (900, 1000);
        }

        setFullScreen (false);

        if (! isOnDesktop())
            addToDesktop (getDesktopWindowStyleFlags());

        setVisible (true);
        toFront (true);
        windowOpen = true;

        if (visibilityChanged != nullptr)
            visibilityChanged (true);
    }
    else
    {
        hideWindow();
    }
}

void FullPageLyricsWindow::hideWindow()
{
    const auto wasOpen = windowOpen || isVisible() || isOnDesktop();

    windowOpen = false;
    setFullScreen (false);
    setVisible (false);

    if (isOnDesktop())
        removeFromDesktop();

    if (wasOpen && visibilityChanged != nullptr)
        visibilityChanged (false);
}

void FullPageLyricsWindow::closeButtonPressed()
{
    hideWindow();
}

void FullPageLyricsWindow::userTriedToCloseWindow()
{
    hideWindow();
}

void FullPageLyricsWindow::setVisibilityChangedCallback (std::function<void (bool visible)> callback)
{
    visibilityChanged = std::move (callback);
}

} // namespace jamstudio::ui
