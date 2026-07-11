cmake_minimum_required(VERSION 3.22)

project(JamStudio VERSION 0.9.6 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# NeuralAmpModelerCore (static lib — do not pull in Core tools/tests)
# ---------------------------------------------------------------------------
set(NAM_CORE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/NeuralAmpModelerCore")
file(GLOB NAM_CORE_SOURCES
    "${NAM_CORE_ROOT}/NAM/*.cpp"
    "${NAM_CORE_ROOT}/NAM/*/*.cpp")

add_library(nam_core STATIC ${NAM_CORE_SOURCES})
target_compile_features(nam_core PUBLIC cxx_std_20)
target_compile_definitions(nam_core PUBLIC
    NAM_SAMPLE_FLOAT
    NAM_ENABLE_A2_FAST)
target_include_directories(nam_core
    PUBLIC
        "${NAM_CORE_ROOT}"
    SYSTEM PUBLIC
        "${NAM_CORE_ROOT}/Dependencies/eigen"
        "${NAM_CORE_ROOT}/Dependencies/nlohmann")

# NAM sources include "json.hpp" and "Eigen/Dense"
if(UNIX AND NOT APPLE)
    target_link_libraries(nam_core PUBLIC stdc++fs)
endif()

if(MSVC)
    target_compile_definitions(nam_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_compile_options(nam_core PRIVATE /W3 /wd4244 /wd4267)
else()
    target_compile_options(nam_core PRIVATE
        -Wno-unused-parameter
        -Wno-sign-compare
        -Wno-deprecated-declarations)
endif()

# ---------------------------------------------------------------------------
# JamStudio application
# ---------------------------------------------------------------------------
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
    src/amp/NamEngine.cpp
    src/amp/AmpProcessor.cpp
    src/audio/StemTrack.cpp
    src/audio/StemMixer.cpp
    src/audio/Metronome.cpp
    src/audio/MultiBusMaster.cpp
    src/audio/TransportController.cpp
    src/audio/StageMediaPlayer.cpp
    src/audio/FFmpegAudioFormat.cpp
    src/audio/TempoDetector.cpp
    src/audio/AudioRecorder.cpp
    src/audio/ExternalRecorder.cpp
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
    src/performance/SetListData.cpp
    src/performance/SetListManager.cpp
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
    src/ui/FloatingWindowDock.cpp
    src/ui/HelpCatalog.cpp
    src/ui/HelpBrowserDialog.cpp
    src/ui/RecordingTakesPanel.cpp
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
    src/ui/StartupWizard.cpp
    src/ui/PerformanceBar.cpp
    src/ui/SetListEditorDialog.cpp
    src/ui/VideoOutputWindow.cpp
    src/ui/StageFxControllerWindow.cpp)

# FFmpeg / libav for MPEG codecs (stage FX video + compressed audio)
# Prefer pkg-config (Linux); fall back to find_path/find_library (Windows/vcpkg).
set(JAMSTUDIO_FFMPEG_LIBS "")
set(JAMSTUDIO_FFMPEG_INCLUDES "")

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(FFMPEG QUIET libavformat libavcodec libavutil libswresample)
endif()

if(FFMPEG_FOUND)
    list(APPEND JAMSTUDIO_FFMPEG_INCLUDES ${FFMPEG_INCLUDE_DIRS})
    list(APPEND JAMSTUDIO_FFMPEG_LIBS ${FFMPEG_LIBRARIES})
    if(FFMPEG_LIBRARY_DIRS)
        target_link_directories(JamStudio PRIVATE ${FFMPEG_LIBRARY_DIRS})
    endif()
    if(FFMPEG_CFLAGS_OTHER)
        target_compile_options(JamStudio PRIVATE ${FFMPEG_CFLAGS_OTHER})
    endif()
else()
    find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
    find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
    find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
    find_path(SWRESAMPLE_INCLUDE_DIR libswresample/swresample.h)
    find_library(AVFORMAT_LIBRARY NAMES avformat avformat-61 avformat-60 avformat-59)
    find_library(AVCODEC_LIBRARY NAMES avcodec avcodec-61 avcodec-60 avcodec-59)
    find_library(AVUTIL_LIBRARY NAMES avutil avutil-59 avutil-58 avutil-57)
    find_library(SWRESAMPLE_LIBRARY NAMES swresample swresample-5 swresample-4)
    if(NOT AVFORMAT_INCLUDE_DIR OR NOT AVFORMAT_LIBRARY OR NOT AVCODEC_LIBRARY
       OR NOT AVUTIL_LIBRARY OR NOT SWRESAMPLE_LIBRARY)
        message(FATAL_ERROR
            "FFmpeg not found. On Linux: install libavformat-dev libavcodec-dev "
            "libavutil-dev libswresample-dev. On Windows: vcpkg install ffmpeg:x64-windows")
    endif()
    list(APPEND JAMSTUDIO_FFMPEG_INCLUDES
        ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR}
        ${AVUTIL_INCLUDE_DIR} ${SWRESAMPLE_INCLUDE_DIR})
    list(APPEND JAMSTUDIO_FFMPEG_LIBS
        ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY}
        ${AVUTIL_LIBRARY} ${SWRESAMPLE_LIBRARY})
endif()

target_include_directories(JamStudio PRIVATE
    src
    ${CMAKE_CURRENT_BINARY_DIR}/juce_binarydata_JamStudioAssets/JuceLibraryCode
    ${JAMSTUDIO_FFMPEG_INCLUDES})

target_compile_definitions(JamStudio PRIVATE
    JUCE_USE_FLAC=1
    JUCE_USE_OGGVORBIS=1
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JAMSTUDIO_USE_FFMPEG=1
    JAMSTUDIO_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

if(UNIX AND NOT APPLE)
    target_compile_definitions(JamStudio PRIVATE
        JUCE_ALSA=1
        JUCE_JACK=1)
elseif(WIN32)
    target_compile_definitions(JamStudio PRIVATE
        JUCE_ALSA=0
        JUCE_JACK=0)
endif()

# whole-archive: NAM registers WaveNet/LSTM/etc. via static constructors in
# otherwise-unreferenced TUs; without this, get_dsp throws "No config parser".
if(MSVC)
    target_link_libraries(JamStudio PRIVATE nam_core)
    target_link_options(JamStudio PRIVATE "/WHOLEARCHIVE:$<TARGET_FILE:nam_core>")
elseif(APPLE)
    target_link_libraries(JamStudio PRIVATE
        "-Wl,-force_load,$<TARGET_FILE:nam_core>"
        nam_core)
else()
    target_link_libraries(JamStudio PRIVATE
        -Wl,--whole-archive
        nam_core
        -Wl,--no-whole-archive)
endif()

target_link_libraries(JamStudio PRIVATE
    JamStudioAssets
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    ${JAMSTUDIO_FFMPEG_LIBS})

# JUCE NEEDS_CURL requires libcurl at link time (pkg-config may be missing headers-only)
if(UNIX AND NOT APPLE)
    find_package(CURL)
    if(CURL_FOUND)
        target_link_libraries(JamStudio PRIVATE CURL::libcurl)
    else()
        target_link_libraries(JamStudio PRIVATE curl)
    endif()
endif()

# ---- Install / packaging layout ----
include(GNUInstallDirs)

install(TARGETS JamStudio
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .)

if(UNIX AND NOT APPLE)
    install(FILES resources/jamstudio.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
    install(FILES resources/JamStudioIcon256.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
        RENAME jamstudio.png)
    install(FILES resources/JamStudioIcon48.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps
        RENAME jamstudio.png)
endif()

# Qt shell prototype (future): apps/JamStudio/
