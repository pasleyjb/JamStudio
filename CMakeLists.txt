cmake_minimum_required(VERSION 3.22)

project(JamStudio VERSION 0.10.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# JUCE leaves Core Audio combiner output buffers untouched on underrun, which
# squeals on Mac when the mic and speakers are separate devices. The pinned
# submodule stays clean; CI applies this patch before configuring JUCE.
if(APPLE)
    set(JAMSTUDIO_COREAUDIO_CPP
        "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules/juce_audio_devices/native/juce_CoreAudio_mac.cpp")
    if(EXISTS "${JAMSTUDIO_COREAUDIO_CPP}")
        file(STRINGS "${JAMSTUDIO_COREAUDIO_CPP}" _jamstudio_coreaudio_silence REGEX "writeSilence")
        if(NOT _jamstudio_coreaudio_silence)
            find_package(Git REQUIRED)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" apply
                    "${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-coreaudio-underrun-silence.patch"
                WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE"
                RESULT_VARIABLE _jamstudio_juce_patch
                ERROR_VARIABLE _jamstudio_juce_patch_err)
            if(NOT _jamstudio_juce_patch EQUAL 0)
                message(FATAL_ERROR
                    "Failed to apply patches/juce-coreaudio-underrun-silence.patch:\n${_jamstudio_juce_patch_err}")
            endif()
        endif()
    endif()
endif()

# JUCE does not clear the scratch buffer it sums later callbacks from.
# setSize leaves the previous mix in place, so each block was added on top
# of the last one. The pinned submodule stays clean; configure applies this.
set(JAMSTUDIO_ADM_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules/juce_audio_devices/audio_io/juce_AudioDeviceManager.cpp")
if(EXISTS "${JAMSTUDIO_ADM_CPP}")
    file(STRINGS "${JAMSTUDIO_ADM_CPP}" _jamstudio_adm_clear REGEX "Callback 0 is expected to fill")
    if(NOT _jamstudio_adm_clear)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply
                "${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-audiodevicemanager-clear-scratch.patch"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE"
            RESULT_VARIABLE _jamstudio_adm_patch
            ERROR_VARIABLE _jamstudio_adm_patch_err)
        if(NOT _jamstudio_adm_patch EQUAL 0)
            message(FATAL_ERROR
                "Failed to apply patches/juce-audiodevicemanager-clear-scratch.patch:\n${_jamstudio_adm_patch_err}")
        endif()
    endif()
endif()

# PipeWire's ALSA plugin does not implement snd_pcm_link. Without this, capture
# blocks the mix thread and late periods stack into a buzz on the HP.
set(JAMSTUDIO_ALSA_CPP
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules/juce_audio_devices/native/juce_ALSA_linux.cpp")
if(EXISTS "${JAMSTUDIO_ALSA_CPP}")
    file(STRINGS "${JAMSTUDIO_ALSA_CPP}" _jamstudio_alsa_link REGEX "devicesLinked")
    if(NOT _jamstudio_alsa_link)
        find_package(Git REQUIRED)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" apply
                "${CMAKE_CURRENT_SOURCE_DIR}/patches/juce-alsa-pipewire-clock.patch"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE"
            RESULT_VARIABLE _jamstudio_alsa_patch
            ERROR_VARIABLE _jamstudio_alsa_patch_err)
        if(NOT _jamstudio_alsa_patch EQUAL 0)
            message(FATAL_ERROR
                "Failed to apply patches/juce-alsa-pipewire-clock.patch:\n${_jamstudio_alsa_patch_err}")
        endif()
    endif()
endif()

add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)

juce_add_gui_app(JamStudio
    PRODUCT_NAME "JamStudio"
    COMPANY_NAME "JamStudio"
    BUNDLE_ID "com.jamstudio.app"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/resources/JamStudioIcon256.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/resources/JamStudioIcon48.png"
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "JamStudio records your instrument and microphone while you practice."
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
    src/platform/HostEnvironment.cpp
    src/app/Main.cpp
    src/app/MainWindow.cpp
    src/app/MainComponent.cpp
    src/app/PracticeSetupPipeline.cpp
    src/audio/StemTrack.cpp
    src/audio/StemMixer.cpp
    src/audio/Metronome.cpp
    src/audio/MultiBusMaster.cpp
    src/audio/TransportController.cpp
    src/audio/StageMediaPlayer.cpp
    src/audio/FFmpegAudioFormat.cpp
    src/audio/TempoDetector.cpp
    src/audio/AudioRecorder.cpp
    src/audio/TakeRecorder.cpp
    src/audio/LiveInputs.cpp
    src/audio/AudioInterfaceManager.cpp
    src/audio/ExternalRecorder.cpp
    src/audio/AudacityRemote.cpp
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
    src/ui/AudioSettingsDialog.cpp
    src/ui/TransportBar.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SeparationProgressBar.cpp
    src/ui/StartupWizard.cpp
    src/ui/PerformanceBar.cpp
    src/ui/SetListEditorDialog.cpp
    src/ui/VideoOutputWindow.cpp
    src/ui/StageFxControllerWindow.cpp)

# FFmpeg / libav for MPEG codecs (stage FX video + compressed audio)
# Prefer pkg-config (Linux, Homebrew on macOS); fall back to find_path/find_library (Windows/vcpkg).
set(JAMSTUDIO_FFMPEG_LIBS "")
set(JAMSTUDIO_FFMPEG_INCLUDES "")

if(APPLE)
    set(_jamstudio_pkg_config_path "$ENV{PKG_CONFIG_PATH}")
    foreach(_prefix IN ITEMS /opt/homebrew /usr/local)
        if(EXISTS "${_prefix}/opt/ffmpeg/lib/pkgconfig")
            set(_jamstudio_pkg_config_path "${_prefix}/opt/ffmpeg/lib/pkgconfig:${_jamstudio_pkg_config_path}")
        endif()
        if(EXISTS "${_prefix}/lib/pkgconfig")
            set(_jamstudio_pkg_config_path "${_prefix}/lib/pkgconfig:${_jamstudio_pkg_config_path}")
        endif()
    endforeach()
    set(ENV{PKG_CONFIG_PATH} "${_jamstudio_pkg_config_path}")
endif()

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
            "FFmpeg not found. On macOS: brew install ffmpeg. "
            "On Linux: install libavformat-dev libavcodec-dev libavutil-dev libswresample-dev. "
            "On Windows: vcpkg install ffmpeg:x64-windows")
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
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=1
    JAMSTUDIO_USE_FFMPEG=1)

if(UNIX AND NOT APPLE)
    target_compile_definitions(JamStudio PRIVATE
        JUCE_ALSA=1
        JUCE_JACK=1)
elseif(WIN32)
    target_compile_definitions(JamStudio PRIVATE
        JUCE_WASAPI=1
        JUCE_DIRECTSOUND=1
        JUCE_ALSA=0
        JUCE_JACK=0)
elseif(APPLE)
    # Core Audio is JUCE's macOS backend. Keep ALSA/JACK off so the link
    # step does not look for Linux audio libraries.
    target_compile_definitions(JamStudio PRIVATE
        JUCE_ALSA=0
        JUCE_JACK=0)
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