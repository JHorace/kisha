cmake_minimum_required(VERSION 3.25)

# Prevent in-source builds. Configuring/building in the source tree scatters
# generated artifacts (CMakeFiles/, _deps/, cmake_install.cmake, Makefile,
# CTestTestfile.cmake, ...) across the project. Require an out-of-source build
# directory instead (e.g. cmake-build-debug).
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR
            "In-source builds are not allowed.\n"
            "Run CMake from a separate build directory, for example:\n"
            "  cmake -S . -B cmake-build-debug\n"
            "  cmake --build cmake-build-debug\n"
            "Remove the stray CMakeCache.txt and CMakeFiles/ that this attempt created in the source tree.")
endif()

project(kisha VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(ProjectDependencies)
include(CommonCompileOptions)
include(CTest)

add_subdirectory(assets)

if(KISHA_BUILD_ENGINE)
    add_subdirectory(kisha-engine)
endif()

if(KISHA_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()
