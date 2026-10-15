#include "AudacityRemote.h"

#if JUCE_WINDOWS

namespace jamstudio::audio
{

namespace
{
AudacityRemote::Result unavailable()
{
    AudacityRemote::Result result;
    result.error = "Audacity remote control is not available on Windows.";
    return result;
}
} // namespace

AudacityRemote::~AudacityRemote() = default;

AudacityRemote::Result AudacityRemote::connect()
{
    return unavailable();
}

AudacityRemote::Result AudacityRemote::prepareTake()
{
    return unavailable();
}

AudacityRemote::Result AudacityRemote::startRecording()
{
    return unavailable();
}

AudacityRemote::Result AudacityRemote::stop()
{
    AudacityRemote::Result idle;
    idle.ok = true;
    return idle;
}

} // namespace jamstudio::audio

#else

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

namespace jamstudio::audio
{

namespace
{
juce::String pipePath (const char* end)
{
    return "/tmp/audacity_script_pipe." + juce::String (end) + "." + juce::String (static_cast<int> (::getuid()));
}

bool writeAll (const int fd, const std::string& bytes)
{
    std::size_t sent = 0;

    while (sent < bytes.size())
    {
        const auto n = ::write (fd, bytes.data() + sent, bytes.size() - sent);

        if (n < 0)
        {
            if (errno == EINTR)
                continue;

            return false;
        }

        sent += static_cast<std::size_t> (n);
    }

    return true;
}

/** Audacity ends a reply with a blank line after "BatchCommand finished".
    Some replies also start with a blank line, so that first newline is not the end. */
bool replyFinished (const std::string& data)
{
    const auto marker = data.find ("BatchCommand finished:");

    if (marker == std::string::npos)
        return false;

    return data.find ("\n\n", marker) != std::string::npos;
}
} // namespace

AudacityRemote::~AudacityRemote()
{
    closePipes();
}

bool AudacityRemote::audacityIsRunning()
{
    juce::ChildProcess probe;

    if (! probe.start ("pgrep -x audacity"))
        return false;

    probe.waitForProcessToFinish (500);
    return probe.getExitCode() == 0;
}

bool AudacityRemote::openPipes()
{
    if (toFd >= 0 && fromFd >= 0)
        return true;

    closePipes();

    const auto toPath = pipePath ("to");
    const auto fromPath = pipePath ("from");
    toFd = ::open (toPath.toRawUTF8(), O_WRONLY | O_NONBLOCK);

    if (toFd < 0)
        return false;

    const auto flags = ::fcntl (toFd, F_GETFL, 0);

    if (flags >= 0)
        ::fcntl (toFd, F_SETFL, flags & ~O_NONBLOCK);

    // A non-blocking read open succeeds before Audacity opens its write end,
    // which lets a server blocked in fopen proceed.
    fromFd = ::open (fromPath.toRawUTF8(), O_RDONLY | O_NONBLOCK);

    if (fromFd < 0)
    {
        closePipes();
        return false;
    }

    return true;
}

void AudacityRemote::closePipes()
{
    if (toFd >= 0)
        ::close (toFd);

    if (fromFd >= 0)
        ::close (fromFd);

    toFd = -1;
    fromFd = -1;
}

AudacityRemote::Result AudacityRemote::command (const juce::String& line, const int timeoutMs)
{
    Result result;

    if (! openPipes())
    {
        result.error = "Audacity script pipe is not open.";
        return result;
    }

    const auto payload = line.toStdString() + "\n";

    if (! writeAll (toFd, payload))
    {
        closePipes();

        if (! openPipes() || ! writeAll (toFd, payload))
        {
            result.error = "Could not send a command to Audacity.";
            return result;
        }
    }

    std::string data;
    const auto started = juce::Time::getMillisecondCounterHiRes();

    while (juce::Time::getMillisecondCounterHiRes() - started < static_cast<double> (timeoutMs))
    {
        if (replyFinished (data))
            break;

        pollfd pollFd {};
        pollFd.fd = fromFd;
        pollFd.events = POLLIN;
        const auto elapsed = juce::Time::getMillisecondCounterHiRes() - started;
        const auto left = juce::jmax (1, timeoutMs - static_cast<int> (elapsed));
        const auto ready = ::poll (&pollFd, 1, left);

        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        if (ready == 0)
            continue;

        char buffer[2048];
        const auto n = ::read (fromFd, buffer, sizeof (buffer));

        if (n > 0)
            data.append (buffer, buffer + n);
        else if (n == 0)
            break;
        else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
            break;
    }

    result.response = juce::String::fromUTF8 (data.data(), static_cast<int> (data.size()));
    result.ok = result.response.contains ("BatchCommand finished: OK");

    if (! result.ok)
    {
        if (result.response.contains ("BatchCommand finished: Failed"))
            result.error = "Audacity rejected " + line + ".";
        else
            result.error = "Audacity did not answer. Close any dialog in Audacity and press Start again.";
    }

    return result;
}

AudacityRemote::Result AudacityRemote::connect()
{
    if (openPipes())
    {
        Result ready;
        ready.ok = true;
        return ready;
    }

    if (! audacityIsRunning())
    {
        const auto launched = std::system ("setsid /usr/bin/audacity >/tmp/audacity-from-jamstudio.log 2>&1 </dev/null &");
        juce::ignoreUnused (launched);
    }

    for (int i = 0; i < 40; ++i)
    {
        if (openPipes())
        {
            Result ready;
            ready.ok = true;
            return ready;
        }

        juce::Thread::sleep (200);
    }

    Result failed;
    failed.error = audacityIsRunning()
                       ? "Audacity is open but the script pipe isn't answering. Close any Audacity dialog and press Start again."
                       : "Audacity did not start. Open Audacity and press Start again.";
    return failed;
}

AudacityRemote::Result AudacityRemote::prepareTake()
{
    auto connected = connect();

    if (! connected.ok)
        return connected;

    command ("Stop:", 1500);

    const auto pref = command ("GetPreference: Name=\"/AudioIO/Duplex\"", 1500);
    const auto overdubOn = pref.response.contains ("\n1\n") || pref.response.startsWith ("1\n");

    if (overdubOn)
    {
        const auto toggled = command ("Overdub:", 1500);

        if (toggled.ok)
            connected.turnedOverdubOff = true;
    }

    command ("CursProjectStart:", 1500);
    command ("Select: Start=0 End=0 RelativeTo=ProjectStart", 1500);
    connected.ok = true;
    connected.error.clear();
    return connected;
}

AudacityRemote::Result AudacityRemote::startRecording()
{
    return command ("Record1stChoice:", 2500);
}

AudacityRemote::Result AudacityRemote::stop()
{
    if (toFd < 0)
    {
        Result idle;
        idle.ok = true;
        return idle;
    }

    return command ("Stop:", 2000);
}

} // namespace jamstudio::audio

#endif
