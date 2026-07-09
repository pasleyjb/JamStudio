#include "FullPageTabsWindow.h"

#include "JamStudioTheme.h"

namespace jamstudio::ui
{

class FullPageTabsWindow::Content : public juce::Component
{
public:
    explicit Content (jamstudio::audio::TransportController& transport)
        : notationView (transport)
    {
        titleLabel.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        titleLabel.setText ("Full Page Tabs", juce::dontSendNotification);
        addAndMakeVisible (titleLabel);

        hintLabel.setText ("Printable layout — use Print or Export PNG", juce::dontSendNotification);
        hintLabel.setColour (juce::Label::textColourId, JamStudioTheme::getColours().textSecondary);
        addAndMakeVisible (hintLabel);

        printButton.onClick = [this] { printTabs(); };
        exportButton.onClick = [this] { exportPng(); };
        addAndMakeVisible (printButton);
        addAndMakeVisible (exportButton);

        notationView.setLayoutMode (jamstudio::notation::NotationView::LayoutMode::fullPageRows);
        notationView.setPrintFriendly (true);
        notationView.setFollowPlayback (true);

        viewport.setViewedComponent (&notationView, false);
        viewport.setScrollBarsShown (true, false);
        addAndMakeVisible (viewport);
    }

    void setScore (const jamstudio::notation::Score& score)
    {
        currentScore = score;
        notationView.setScore (score);

        const auto title = score.getTitle().isNotEmpty() ? score.getTitle() : "Untitled";
        const auto part = score.isEmpty() ? juce::String() : score.getActivePart().name;
        titleLabel.setText (part.isNotEmpty() ? (title + "  —  " + part) : title,
                            juce::dontSendNotification);

        resized();
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
        printButton.setBounds (header.removeFromRight (90).reduced (2));
        header.removeFromRight (6);
        exportButton.setBounds (header.removeFromRight (110).reduced (2));
        header.removeFromRight (10);
        titleLabel.setBounds (header.removeFromLeft (juce::jmax (160, header.getWidth() / 2)));
        hintLabel.setBounds (header);

        bounds.removeFromTop (6);
        viewport.setBounds (bounds);

        // Size to viewport width first so measures-per-row reflows, then expand height.
        const auto pageWidth = juce::jmax (viewport.getWidth(), 720);
        notationView.setSize (pageWidth, 200);
        notationView.setSize (juce::jmax (pageWidth, notationView.getContentWidth()),
                              juce::jmax (viewport.getHeight(), notationView.getContentHeight()));
    }

private:
    void printTabs()
    {
        if (currentScore.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Print Tabs",
                                                    "Load or generate a score before printing.");
            return;
        }

        const auto image = notationView.renderToImage (2.0f);
        const auto tempDir = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                 .getChildFile ("JamStudio")
                                 .getChildFile ("print");
        tempDir.createDirectory();

        const auto safeTitle = currentScore.getTitle().retainCharacters (
            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ");
        const auto file = tempDir.getChildFile (
            (safeTitle.isNotEmpty() ? safeTitle.trim() : "JamStudio-Tabs") + "-print.png");

        if (! writePng (image, file))
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Print Tabs",
                                                    "Could not write print image.");
            return;
        }

        // Prefer system print; fall back to opening the image for the user to print.
        juce::StringArray lpCmd;
        lpCmd.add ("/usr/bin/lp");
        lpCmd.add (file.getFullPathName());

        juce::ChildProcess printer;

        if (printer.start (lpCmd))
        {
            juce::AlertWindow::showMessageBoxAsync (
                juce::MessageBoxIconType::InfoIcon,
                "Print Tabs",
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
            "Print Tabs",
            "Opened printable image:\n" + file.getFullPathName()
                + "\n\nUse your image viewer’s Print command (Ctrl+P).");
    }

    void exportPng()
    {
        if (currentScore.isEmpty())
        {
            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Export Tabs",
                                                    "Load or generate a score before exporting.");
            return;
        }

        const auto defaultName = (currentScore.getTitle().isNotEmpty()
                                      ? currentScore.getTitle()
                                      : "JamStudio-Tabs")
                                     .retainCharacters (
                                         "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_ ")
                                 + ".png";

        fileChooser = std::make_unique<juce::FileChooser> ("Export tabs as PNG",
                                                           juce::File::getSpecialLocation (
                                                               juce::File::userDocumentsDirectory)
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

            const auto image = notationView.renderToImage (2.0f);

            if (! writePng (image, file))
            {
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        "Export Tabs",
                                                        "Failed to write PNG file.");
                return;
            }

            juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Export Tabs",
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

    jamstudio::notation::Score currentScore;
    juce::Label titleLabel;
    juce::Label hintLabel;
    juce::TextButton printButton { "Print" };
    juce::TextButton exportButton { "Export PNG" };
    juce::Viewport viewport;
    jamstudio::notation::NotationView notationView;
    std::unique_ptr<juce::FileChooser> fileChooser;
};

FullPageTabsWindow::FullPageTabsWindow (jamstudio::audio::TransportController& transport)
    : DocumentWindow ("JamStudio — Full Page Tabs",
                      JamStudioTheme::getColours().windowBackground,
                      DocumentWindow::allButtons),
      transportController (transport)
{
    juce::ignoreUnused (transportController);
    setUsingNativeTitleBar (true);
    content = std::make_unique<Content> (transport);
    setContentNonOwned (content.get(), true);
    setResizable (true, true);
    setResizeLimits (800, 600, 4000, 3000);
    setFullScreen (false);
    centreWithSize (1100, 800);
    setVisible (false);
}

FullPageTabsWindow::~FullPageTabsWindow()
{
    setContentNonOwned (nullptr, false);
    content.reset();
}

void FullPageTabsWindow::setScore (const jamstudio::notation::Score& score)
{
    if (content != nullptr)
        content->setScore (score);
}

void FullPageTabsWindow::showWindow (const bool shouldShow)
{
    setVisible (shouldShow);

    if (shouldShow)
    {
        setFullScreen (true);
        toFront (true);
    }

    if (visibilityChanged != nullptr)
        visibilityChanged (shouldShow);
}

void FullPageTabsWindow::closeButtonPressed()
{
    setVisible (false);

    if (visibilityChanged != nullptr)
        visibilityChanged (false);
}

void FullPageTabsWindow::setVisibilityChangedCallback (std::function<void (bool visible)> callback)
{
    visibilityChanged = std::move (callback);
}

} // namespace jamstudio::ui
