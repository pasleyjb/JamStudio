cmake_minimum_required(VERSION 3.22)

project(JamStudio VERSION 0.9.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)

juce_add_gui_app(JamStudio
    PRODUCT_NAME "JamStudio"
    COMPANY_NAME "JamStudio"
    BUNDLE_ID "com.jamstudio.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/JamStudioIcon256.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/JamStudioIcon48.png"
    NEEDS_CURL TRUE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(JamStudio)

# Brand artwork (window icon + wizard splash + mode tiles)
juce_add_binary_data(JamStudioAssets
    HEADER_NAME "JamStudioAssets.h"
    NAMESPACE JamStudioAssets
    SOURCES
        resources/JamStudioIcon.jpg
        resources/JamStudioIcon256.png
        resources/Practice.jpg
        resources/wizardbkgrnd.jpg)

target_sources(JamStudio PRIVATE
    src/app/AppMenuBar.cpp
    src/app/Main.cpp
    src/app/MainWindow.cpp
    src/app/MainComponent.cpp
    src/app/PracticeSetupPipeline.cpp
    src/audio/StemTrack.cpp
    src/audio/StemMixer.cpp
    src/audio/Metronome.cpp
    src/audio/TransportController.cpp
    src/audio/TempoDetector.cpp
    src/audio/AudioRecorder.cpp
    src/audio/RecordingExporter.cpp
    src/audio/RecordingTakeManager.cpp
    src/midi/MidiMappingProfile.cpp
    src/midi/MidiControlSurface.cpp
    src/ai/CommandUtils.cpp
    src/ai/DemucsSeparator.cpp
    src/ai/WhisperTranscriber.cpp
    src/ai/BasicPitchTranscriber.cpp
    src/ai/AiToolsCatalog.cpp
    src/notation/Score.cpp
    src/notation/MusicXmlLoader.cpp
    src/notation/MusicXmlParser.cpp
    src/notation/LyricsTrack.cpp
    src/notation/LrcParser.cpp
    src/notation/LyricsView.cpp
    src/notation/MidiScoreConverter.cpp
    src/notation/NotationView.cpp
    src/notation/TranscriptionEditor.cpp
    src/notation/ScoreLyricsExtractor.cpp
    src/notation/TabLibraryClient.cpp
    src/notation/SongMetadata.cpp
    src/notation/OnlineLyricsClient.cpp
    src/project/ProjectManager.cpp
    src/project/RecentProjects.cpp
    src/ui/JamStudioTheme.cpp
    src/ui/JamStudioLookAndFeel.cpp
    src/ui/BrandAssets.cpp
    src/ui/IndicatorButton.cpp
    src/ui/TapeDeckButton.cpp
    src/ui/ToolbarTabs.cpp
    src/ui/AiToolsSetupDialog.cpp
    src/ui/StemMiniWaveform.cpp
    src/ui/StemStrip.cpp
    src/ui/StemLane.cpp
    src/ui/MixerChannelStrip.cpp
    src/ui/MixerWindow.cpp
    src/ui/NotationHeaderBar.cpp
    src/ui/FullPageTabsWindow.cpp
    src/ui/FullPageLyricsWindow.cpp
    src/ui/TabLibraryBrowserDialog.cpp
    src/ui/TranscriptionCorrectionDialog.cpp
    src/ui/OnlineLyricsDialog.cpp
    src/ui/MidiControlDialog.cpp
    src/ui/TransportBar.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SeparationProgressBar.cpp
    src/ui/StartupWizard.cpp)

target_include_directories(JamStudio PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_JamStudioAssets/JuceLibraryCode)

target_compile_definitions(JamStudio PRIVATE
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_ALSA=1
    JUCE_JACK=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1)

target_link_libraries(JamStudio PRIVATE
    JamStudioAssets
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# Qt shell prototype (future): apps/JamStudio/