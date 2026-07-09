#pragma once

#include "../audio/TransportController.h"
#include "../notation/LyricsTrack.h"
#include "../notation/Score.h"
#include "../ui/TransportBar.h"
#include "ProjectData.h"

namespace jamstudio::project
{

/** Serializes and restores JamStudio session state. */
class ProjectManager
{
public:
    [[nodiscard]] static ProjectData captureState (const juce::File& songFile,
                                                   const juce::File& scoreFile,
                                                   const juce::File& lyricsFile,
                                                   const jamstudio::notation::Score& score,
                                                   const jamstudio::notation::LyricsTrack& lyrics,
                                                   jamstudio::audio::TransportController& transport,
                                                   const jamstudio::ui::TransportBar& transportBar);

    /**
     * Saves project JSON and copies stem audio into a permanent media folder next to
     * the project file:  "{Name}.media/stems/" (wav stems)
     * Updates data.stems paths to those permanent copies (data is modified).
     */
    [[nodiscard]] static bool saveProject (const juce::File& projectFile, ProjectData& data);

    [[nodiscard]] static bool loadProject (const juce::File& projectFile,
                                           ProjectData& data,
                                           juce::String& errorMessage);

    [[nodiscard]] static bool applyState (const ProjectData& data,
                                          jamstudio::audio::TransportController& transport,
                                          jamstudio::ui::TransportBar& transportBar,
                                          jamstudio::notation::Score& score,
                                          jamstudio::notation::LyricsTrack& lyrics,
                                          juce::File& songFile,
                                          juce::File& scoreFile,
                                          juce::File& lyricsFile,
                                          juce::String& errorMessage,
                                          const juce::File& projectFile = {});

    /** Permanent folder for demucs / project stem media under Documents. */
    [[nodiscard]] static juce::File getMediaStemsDirectory (const juce::File& projectFile);

    /** Copy stem files into the project's .media/stems folder; rewrite paths in data. */
    [[nodiscard]] static bool relocateStemMedia (const juce::File& projectFile,
                                                 ProjectData& data,
                                                 juce::String& errorMessage);

    /** Resolve a stored stem path (absolute, relative, or same-name under .media/stems). */
    [[nodiscard]] static juce::File resolveStemFile (const juce::String& storedPath,
                                                     const juce::File& projectFile);

    /** How many of the project's listed stems can still be found on disk. */
    [[nodiscard]] static int countResolvedStems (const ProjectData& data,
                                                 const juce::File& projectFile);

    /**
     * True when the project expected separated stems (2+) but none/few remain —
     * typical after /tmp cleanup of demucs output.
     */
    [[nodiscard]] static bool needsStemRecovery (const ProjectData& data,
                                                 const juce::File& projectFile);
};

} // namespace jamstudio::project