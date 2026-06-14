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

# These prevent vulkan from throwing exceptions.
# Alongside our c++23 requirement this will cause vk::raii functions to return std::expected
target_compile_definitions(kisha_engine_dependencies INTERFACE
        VULKAN_HPP_NO_EXCEPTIONS
        VULKAN_HPP_USE_STD_EXPECTED
)

target_link_libraries(
        kisha_engine_dependencies
        INTERFACE
        Vulkan::Vulkan
        glm::glm
        GPUOpen::VulkanMemoryAllocator
        spdlog::spdlog
        fmt::fmt
)

add_dependencies(kisha_engine_dependencies
    kisha_shaders)

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
