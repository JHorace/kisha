include(FetchContent)



### This CMakeLists.txt just handles the engine dependencies.
### The actual engine target is defined in ./src/CMakeLists.txt.

###### Engine dependencies
# Resolve all the dependencies here then re-export them as a single target

if(NOT TARGET kisha_engine_dependencies)
    add_library(kisha_engine_dependencies INTERFACE
            tests/engine_runtime.tests.cpp)
endif()
add_library(kisha::engine_dependencies ALIAS kisha_engine_dependencies)

target_compile_features(kisha_engine_dependencies INTERFACE cxx_std_23)
target_link_libraries(
        kisha_engine_dependencies
        INTERFACE
        Vulkan::Vulkan
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        spdlog::spdlog
)

##### Platform compile definitions

if(UNIX AND NOT APPLE)
    # Enable Linux-specific surface support.
    #    # This allows use of Wayland, XCB, and X11 Vulkan types and extensions.
    target_compile_definitions(kisha_engine_dependencies INTERFACE
            VK_USE_PLATFORM_WAYLAND_KHR
            VK_USE_PLATFORM_XCB_KHR
            VK_USE_PLATFORM_XLIB_KHR
    )
endif()

if(WIN32)
    # Enable Win32 surface support on Windows.
    target_compile_definitions(kisha_engine_dependencies INTERFACE VK_USE_PLATFORM_WIN32_KHR)
endif()

##### Testing

if(BUILD_TESTING)
#    find_package(Catch2 3 CONFIG QUIET)
#    if(NOT Catch2_FOUND)
#        FetchContent_Declare(
#                Catch2
#                GIT_REPOSITORY https://github.com/catchorg/Catch2.git
#                GIT_TAG v3.6.0
#                GIT_SHALLOW TRUE
#        )
#        FetchContent_MakeAvailable(Catch2)
#    endif()
#    if(NOT TARGET Catch2::Catch2WithMain)
#        message(FATAL_ERROR "Unable to resolve Catch2::Catch2WithMain target.")
#    endif()

    if(NOT TARGET kisha_engine_test_dependencies)
        add_library(kisha_engine_test_dependencies INTERFACE)
    endif()
    add_library(kisha::engine_test_dependencies ALIAS kisha_engine_test_dependencies)

    target_compile_features(kisha_engine_test_dependencies INTERFACE cxx_std_23)
    target_link_libraries(
            kisha_engine_test_dependencies
            INTERFACE
            kisha_engine_dependencies
            Catch2::Catch2WithMain
    )
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
