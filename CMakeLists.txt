cmake_minimum_required(VERSION 3.20)
project(pulsedrive C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(PD_BUILD_TESTS "Build pulsedrive test programs" ON)
option(PD_SPOTIFY "Enable Spotify integration (requires libcurl)" ON)

# ── Kiss FFT (public domain) ──
include(FetchContent)
set(KISSFFT_TEST OFF CACHE BOOL "" FORCE)
set(KISSFFT_TOOLS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    kissfft
    GIT_REPOSITORY https://github.com/mborgerding/kissfft.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(kissfft)

# ── nlohmann/json (for Spotify API responses) ──
if(PD_SPOTIFY)
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.11.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# ── Library sources ──
set(PD_SOURCES
    src/pd_core.cpp
    src/pd_beat_detect.cpp
    src/pd_intensity.cpp
    src/pd_timeline.cpp
)

# Platform-specific audio capture
if(APPLE)
    list(APPEND PD_SOURCES src/pd_audio_capture_coreaudio.cpp)
elseif(WIN32)
    list(APPEND PD_SOURCES src/pd_audio_capture_wasapi.cpp)
else()
    list(APPEND PD_SOURCES src/pd_audio_capture_stub.cpp)
endif()

# Spotify integration (optional)
if(PD_SPOTIFY)
    list(APPEND PD_SOURCES
        src/pd_spotify.cpp
        src/pd_http.cpp
        src/pd_oauth.cpp
    )
endif()

add_library(pulsedrive STATIC ${PD_SOURCES})

target_include_directories(pulsedrive PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pulsedrive PRIVATE kissfft)

if(PD_SPOTIFY)
    target_compile_definitions(pulsedrive PRIVATE PD_SPOTIFY_ENABLED)
    target_link_libraries(pulsedrive PRIVATE nlohmann_json::nlohmann_json)
    find_package(CURL REQUIRED)
    target_link_libraries(pulsedrive PRIVATE CURL::libcurl)
endif()

# Platform libraries for audio capture
if(APPLE)
    target_link_libraries(pulsedrive PRIVATE
        "-framework CoreAudio"
        "-framework AudioToolbox"
        "-framework CoreFoundation"
    )
elseif(WIN32)
    target_link_libraries(pulsedrive PRIVATE ole32 uuid)
endif()

# ── Test program ──
if(PD_BUILD_TESTS)
    add_executable(pd_test test/pd_test.cpp)
    target_link_libraries(pd_test PRIVATE pulsedrive)
    if(APPLE)
        target_link_libraries(pd_test PRIVATE "-framework CoreAudio" "-framework AudioToolbox" "-framework CoreFoundation")
    endif()
endif()
