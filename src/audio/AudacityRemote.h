#pragma once

#include <JuceHeader.h>

namespace jamstudio::audio
{

/**
 * Drives a running Audacity through mod-script-pipe.
 * The pipes stay open: closing the write end makes Audacity drop the server.
 */
class AudacityRemote
{
public:
    struct Result
    {
        bool ok = false;
        bool turnedOverdubOff = false;
        juce::String response;
        juce::String error;
    };

    AudacityRemote() = default;
    ~AudacityRemote();

    AudacityRemote (const AudacityRemote&) = delete;
    AudacityRemote& operator= (const AudacityRemote&) = delete;

    /** Open the script pipes, launching Audacity if it is not running. */
    Result connect();

    /** Stop Audacity, turn overdub off, and put the cursor at the project start. */
    Result prepareTake();

    /** Start recording. Returns after Audacity acknowledges. */
    Result startRecording();

    /** Stop Audacity's transport. Safe when it is already stopped. */
    Result stop();

private:
    Result command (const juce::String& line, int timeoutMs);
    bool openPipes();
    void closePipes();
    [[nodiscard]] static bool audacityIsRunning();

    int toFd = -1;
    int fromFd = -1;
};

} // namespace jamstudio::audio
