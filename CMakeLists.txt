cmake_minimum_required(VERSION 3.25)

project(kisha VERSION 0.1.0 LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(ProjectOptions)
include(CTest)

if(KISHA_BUILD_ENGINE)
    add_subdirectory(kisha-engine)
endif()
