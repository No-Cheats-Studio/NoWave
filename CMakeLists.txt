cmake_minimum_required(VERSION 3.25)

project(NoWave VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

find_package(Vulkan REQUIRED)

if(DEFINED ENV{VK_SDK_PATH})
    set(VULKAN_SDK_ROOT "$ENV{VK_SDK_PATH}")
elseif(DEFINED ENV{VULKAN_SDK})
    set(VULKAN_SDK_ROOT "$ENV{VULKAN_SDK}")
endif()

find_program(GLSLC_EXECUTABLE
    NAMES glslc
    HINTS
        "${VULKAN_SDK_ROOT}/Bin"
        "${VULKAN_SDK_ROOT}/bin"
    REQUIRED
)

set(RAYTRACER_SHADER "${CMAKE_CURRENT_SOURCE_DIR}/src/raytracing/shaders/raytracer.comp")
set(RAYTRACER_SPV "${CMAKE_CURRENT_BINARY_DIR}/generated/raytracer.spv")
set(RAYTRACER_SHADER_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/raytracer_spv.h")

add_custom_command(
    OUTPUT "${RAYTRACER_SPV}" "${RAYTRACER_SHADER_HEADER}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${GLSLC_EXECUTABLE}" --target-env=vulkan1.1 -O
            "${RAYTRACER_SHADER}" -o "${RAYTRACER_SPV}"
    COMMAND "${CMAKE_COMMAND}"
            "-DSPV_FILE=${RAYTRACER_SPV}"
            "-DHEADER_FILE=${RAYTRACER_SHADER_HEADER}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv_to_header.cmake"
    DEPENDS
        "${RAYTRACER_SHADER}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/spv_to_header.cmake"
    VERBATIM
)

add_custom_target(NoWaveRaytracerShader
    DEPENDS "${RAYTRACER_SPV}" "${RAYTRACER_SHADER_HEADER}"
)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG v2.0.0rc13
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    GIT_SHALLOW TRUE
)

FetchContent_Declare(
    ufbx
    GIT_REPOSITORY https://github.com/ufbx/ufbx.git
    GIT_TAG v0.23.0
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE tinyobjloader glm ufbx)

set_target_properties(tinyobjloader PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)

add_library(ufbx STATIC "${ufbx_SOURCE_DIR}/ufbx.c")
target_include_directories(ufbx PUBLIC "${ufbx_SOURCE_DIR}")
set_target_properties(ufbx PROPERTIES
    C_STANDARD 99
    C_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

juce_add_plugin(NoWave
    COMPANY_NAME "NoWave"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Nwve
    PLUGIN_CODE Nwv1
    FORMATS VST3 Standalone
    PRODUCT_NAME "NoWave"
)

target_sources(NoWave
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginProcessor.h
        src/PluginEditor.cpp
        src/PluginEditor.h
        src/parameters/ParameterLayout.cpp
        src/parameters/ParameterLayout.h
        src/scene/AcousticMaterial.h
        src/scene/RoomMesh.cpp
        src/scene/RoomMesh.h
        src/scene/ObjSceneLoader.cpp
        src/scene/ObjSceneLoader.h
        src/scene/FbxSceneLoader.cpp
        src/scene/FbxSceneLoader.h
        src/scene/SceneFileLoader.cpp
        src/scene/SceneFileLoader.h
        src/scene/MaterialLibrary.cpp
        src/scene/MaterialLibrary.h
        src/scene/RoomPresets.cpp
        src/scene/RoomPresets.h
        src/ui/StartScreenComponent.cpp
        src/ui/StartScreenComponent.h
        src/ui/Viewport3DComponent.cpp
        src/ui/Viewport3DComponent.h
        src/ui/BottomControlBar.cpp
        src/ui/BottomControlBar.h
        src/audio/ChannelEQ.cpp
        src/audio/ChannelEQ.h
        src/audio/IRBuffer.h
        src/audio/IRUpdateQueue.h
        src/audio/CrossfadeConvolver.cpp
        src/audio/CrossfadeConvolver.h
        src/audio/ReverbEngine.cpp
        src/audio/ReverbEngine.h
        src/raytracing/BVHBuilder.cpp
        src/raytracing/BVHBuilder.h
        src/raytracing/BVHNode.h
        src/raytracing/AcousticSimulation.h
        src/raytracing/RaytracingWorker.cpp
        src/raytracing/RaytracingWorker.h
        src/raytracing/VulkanContext.cpp
        src/raytracing/VulkanContext.h
        src/raytracing/VulkanRaytracer.cpp
        src/raytracing/VulkanRaytracer.h
        "${RAYTRACER_SHADER_HEADER}"
)

add_dependencies(NoWave NoWaveRaytracerShader)

target_include_directories(NoWave
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_BINARY_DIR}/generated"
)

target_compile_definitions(NoWave
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(NoWave
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        tinyobjloader
        ufbx
        glm::glm
        Vulkan::Vulkan
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

