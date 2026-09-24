cmake_minimum_required(VERSION 3.22)

project(SpatialAudio VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(SPATIAL_BUILD_APP "Build the Spatial Renderer app (downloads JUCE)" ON)

# ---------------------------------------------------------------------------
# Checks for the panning and pathway maths (no JUCE needed)
enable_testing()
add_executable(CoreTests Tests/CoreTests.cpp)
add_test(NAME CoreTests COMMAND CoreTests)

# ---------------------------------------------------------------------------
# The renderer app
if(SPATIAL_BUILD_APP)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

    juce_add_gui_app(SpatialRenderer
        PRODUCT_NAME "Spatial Renderer"
        COMPANY_NAME "Ruby Singh"
        BUNDLE_ID "ca.rubysingh.spatialrenderer"
        VERSION ${PROJECT_VERSION}
        MICROPHONE_PERMISSION_ENABLED TRUE
        MICROPHONE_PERMISSION_TEXT "Spatial Renderer receives audio from Ableton Live through a virtual audio device.")

    target_sources(SpatialRenderer PRIVATE
        Source/Renderer/Main.cpp
        Source/Renderer/AudioEngine.cpp
        Source/Renderer/ControlList.cpp
        Source/Renderer/MainComponent.cpp
        Source/Renderer/RoomLayout.cpp
        Source/Renderer/RoomPanel.cpp
        Source/Renderer/RoomView3D.cpp)

    target_compile_definitions(SpatialRenderer PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:SpatialRenderer,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:SpatialRenderer,JUCE_VERSION>")

    target_link_libraries(SpatialRenderer
        PRIVATE
            juce::juce_audio_utils
            juce::juce_audio_devices
            juce::juce_gui_extra
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()
