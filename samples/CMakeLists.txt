# Samples subtree (opt-in via KISHA_BUILD_SAMPLES, default OFF).
include(CPM)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
CPMAddPackage("gh:glfw/glfw#3.4")

add_library(kisha_sample_framework STATIC)
add_library(kisha::sample_framework ALIAS kisha_sample_framework)

target_compile_features(kisha_sample_framework PUBLIC cxx_std_23)
target_link_libraries(
        kisha_sample_framework
        PUBLIC
        kisha::engine
        PRIVATE
        glfw
)
target_include_directories(
        kisha_sample_framework
        PUBLIC
        "${CMAKE_CURRENT_LIST_DIR}/framework"
)

target_compile_definitions(kisha_sample_framework PRIVATE GLFW_INCLUDE_NONE)

add_subdirectory(framework)
add_subdirectory(triangle)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
