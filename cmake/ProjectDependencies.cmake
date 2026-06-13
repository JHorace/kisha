include_guard(GLOBAL)

include(CPM)

CPMAddPackage("gh:gabime/spdlog#v1.17.0")
CPMAddPackage("gh:g-truc/glm#1.0.3")
CPMAddPackage("gh:catchorg/Catch2#v3.15.0")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#v3.2.1")

find_package(Vulkan REQUIRED)
get_target_property(VULKAN_LIB_LOCATION Vulkan::Vulkan IMPORTED_LOCATION)
message(STATUS "Found Vulkan Library: ${VULKAN_LIB_LOCATION}")

# ---------------------------------------------------------------------------
# Vulkan Profiles
# ---------------------------------------------------------------------------
# The Vulkan Profiles client API is a single header (`vulkan/vulkan_profiles.hpp`).
# It is shipped, version-matched to the Vulkan headers, by the platform's Vulkan
# Profiles package (e.g. the LunarG Vulkan SDK, or the distro `vulkan-profiles`
# package), installed alongside `vulkan.hpp` on the standard Vulkan include path.
# We therefore just verify its presence and expose it through an INTERFACE target
# `Vulkan::Profiles`; no fetching or code generation is required.
find_file(VULKAN_PROFILES_HEADER
        NAMES vulkan/vulkan_profiles.hpp
        HINTS
        "${Vulkan_INCLUDE_DIRS}"
        "${Vulkan_INCLUDE_DIR}"
        "$ENV{VULKAN_SDK}/include"
)
if(NOT VULKAN_PROFILES_HEADER)
    message(FATAL_ERROR
            "Could not find vulkan/vulkan_profiles.hpp. Install the Vulkan Profiles "
            "header that ships with the Vulkan SDK (e.g. the `vulkan-profiles` package).")
endif()
message(STATUS "Found Vulkan Profiles header: ${VULKAN_PROFILES_HEADER}")

if(NOT TARGET VulkanProfiles_header)
    add_library(VulkanProfiles_header INTERFACE)
    # The header lives on the standard Vulkan include path, so linking Vulkan::Vulkan
    # is sufficient to make `#include <vulkan/vulkan_profiles.hpp>` resolve.
    target_link_libraries(VulkanProfiles_header INTERFACE Vulkan::Vulkan)
endif()
add_library(Vulkan::Profiles ALIAS VulkanProfiles_header)
