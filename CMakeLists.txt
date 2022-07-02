cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_EXTENSIONS OFF)

set(ENABLE_DEVELOPER_MODE
        FALSE
        CACHE BOOL "Enable 'developer mode'")

set(WARNINGS_AS_ERRORS_DEVELOPER_DEFAULT FALSE)

include(FetchContent)
FetchContent_Declare(_project_options
        URL https://github.com/cpp-best-practices/project_options/archive/refs/tags/v0.17.0.zip)
FetchContent_MakeAvailable(_project_options)
include(${_project_options_SOURCE_DIR}/Index.cmake)

project(
        FAM
        VERSION 0.0.1
        DESCRIPTION ""
        HOMEPAGE_URL "https://github.com/danieldzahka/FAM"
        LANGUAGES CXX C)

set(GIT_SHA
        "Unknown"
        CACHE STRING "SHA this build was generated from")
string(
        SUBSTRING "${GIT_SHA}"
        0
        8
        GIT_SHORT_SHA)

get_property(BUILDING_MULTI_CONFIG GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if (BUILDING_MULTI_CONFIG)
    if (NOT CMAKE_BUILD_TYPE)
        message(TRACE "Setting up multi-config build types")
        set(CMAKE_CONFIGURATION_TYPES
                Debug
                Release
                RelWithDebInfo
                MinSizeRel
                CACHE STRING "Enabled build types" FORCE)
    else ()
        message(TRACE "User chose a specific build type, so we are using that")
        set(CMAKE_CONFIGURATION_TYPES
                ${CMAKE_BUILD_TYPE}
                CACHE STRING "Enabled build types" FORCE)
    endif ()
endif ()

include(${_project_options_SOURCE_DIR}/src/DynamicProjectOptions.cmake)

dynamic_project_options()
target_compile_features(project_options INTERFACE cxx_std_${CMAKE_CXX_STANDARD})

add_subdirectory(src)

option(ENABLE_TESTING "Enable the tests" ON)
if (ENABLE_TESTING)
    enable_testing()
    message("Building Tests")
    add_subdirectory(test)
endif ()