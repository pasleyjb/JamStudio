cmake_minimum_required(VERSION 3.22)

project(JamStudio VERSION 0.4.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(third_party/JUCE EXCLUDE_FROM_ALL)

juce_add_gui_app(JamStudio
    PRODUCT_NAME "JamStudio"
    COMPANY_NAME "JamStudio"
    BUNDLE_ID "com.jamstudio.app"
    NEEDS_CURL FALSE
    NEEDS_WEB_BROWSER FALSE)

juce_generate_juce_header(JamStudio)

target_sources(JamStudio PRIVATE
    src/app/Main.cpp
    src/app/MainWindow.cpp
    src/app/MainComponent.cpp
    src/audio/StemTrack.cpp
    src/audio/StemMixer.cpp
    src/audio/Metronome.cpp
    src/audio/TransportController.cpp
    src/audio/AudioRecorder.cpp
    src/audio/RecordingExporter.cpp
    src/ai/DemucsSeparator.cpp
    src/notation/Score.cpp
    src/notation/MusicXmlLoader.cpp
    src/notation/MusicXmlParser.cpp
    src/notation/NotationView.cpp
    src/project/ProjectManager.cpp
    src/ui/StemStrip.cpp
    src/ui/TransportBar.cpp
    src/ui/WaveformDisplay.cpp
    src/ui/SeparationProgressBar.cpp)

target_include_directories(JamStudio PRIVATE src)

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
    JUCE_USE_CURL=0)

target_link_libraries(JamStudio PRIVATE
    juce::juce_audio_utils
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)