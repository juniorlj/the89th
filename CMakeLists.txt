cmake_minimum_required(VERSION 3.22)

project(THE89TH VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "Build architectures" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS" FORCE)
endif()

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

option(THE89TH_BUILD_PLUGIN "Build the VST3 plugin"    ON)
option(THE89TH_BUILD_TESTS  "Build the unit tests"     ON)
option(THE89TH_BUILD_CLI    "Build the headless renderer" ON)

include(cmake/CPM.cmake)

add_subdirectory(libs/core)

if(THE89TH_BUILD_CLI)
    add_subdirectory(cli)
endif()

if(THE89TH_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(THE89TH_BUILD_PLUGIN)
    add_subdirectory(plugin)
endif()
