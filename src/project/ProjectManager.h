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

    [[nodiscard]] static bool saveProject (const juce::File& projectFile, const ProjectData& data);
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
                                          juce::String& errorMessage);
};

} // namespace jamstudio::project