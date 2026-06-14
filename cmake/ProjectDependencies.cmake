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
# The Vulkan Profiles client API is a single header (`vulkan/vulkan_profiles.hpp`)
# that is *generated* from a set of profile JSON files against the Vulkan
# registry (`vk.xml`). We generate it ourselves so the engine's custom profile
# (`kisha-engine/profiles/VP_KISHA_baseline.json`) is baked directly into the
# header, which is what lets the engine reference `VP_KISHA_BASELINE_NAME` /
# `VP_KISHA_BASELINE_SPEC_VERSION` and call `vpGetPhysicalDeviceProfileSupport`.
#
# The Vulkan-Profiles repository is fetched (download only) via CPM purely to get
# the generator script `gen_profiles_solution.py`; we never build the upstream
# CMake project. The generator is run at configure time against the system
# `vk.xml` (version-matched to `Vulkan::Vulkan`) and our profiles directory.
CPMAddPackage(
        NAME VulkanProfiles
        GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Profiles.git
        GIT_TAG vulkan-sdk-1.4.350.0
        DOWNLOAD_ONLY YES
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# The Vulkan registry (vk.xml) the generator reads. It ships with the Vulkan
# headers and must match the version of `Vulkan::Vulkan` we link against.
find_file(VULKAN_REGISTRY_VK_XML
        NAMES vk.xml
        HINTS
        "${Vulkan_INCLUDE_DIRS}/../share/vulkan/registry"
        "$ENV{VULKAN_SDK}/share/vulkan/registry"
        "/usr/share/vulkan/registry"
)
if(NOT VULKAN_REGISTRY_VK_XML)
    message(FATAL_ERROR
            "Could not find the Vulkan registry (vk.xml). It ships with the Vulkan "
            "headers/SDK and is required to generate vulkan_profiles.hpp.")
endif()
message(STATUS "Using Vulkan registry: ${VULKAN_REGISTRY_VK_XML}")

set(KISHA_PROFILES_INPUT_DIR "${CMAKE_SOURCE_DIR}/kisha-engine/profiles")
set(VULKAN_PROFILES_GENERATED_DIR "${CMAKE_BINARY_DIR}/vulkan-profiles-generated")
set(VULKAN_PROFILES_GENERATED_HEADER "${VULKAN_PROFILES_GENERATED_DIR}/vulkan/vulkan_profiles.hpp")

file(MAKE_DIRECTORY "${VULKAN_PROFILES_GENERATED_DIR}/vulkan")

# Generate at configure time so the baked-in profile is always in sync with the
# JSON. Re-run whenever the script, registry or any profile JSON changes.
message(STATUS "Generating vulkan_profiles.hpp from ${KISHA_PROFILES_INPUT_DIR}")
execute_process(
        COMMAND "${Python3_EXECUTABLE}"
        "${VulkanProfiles_SOURCE_DIR}/scripts/gen_profiles_solution.py"
        --registry "${VULKAN_REGISTRY_VK_XML}"
        --input "${KISHA_PROFILES_INPUT_DIR}"
        --output-library-inc "${VULKAN_PROFILES_GENERATED_DIR}/vulkan"
        RESULT_VARIABLE VULKAN_PROFILES_GEN_RESULT
        OUTPUT_VARIABLE VULKAN_PROFILES_GEN_OUTPUT
        ERROR_VARIABLE VULKAN_PROFILES_GEN_OUTPUT
)
if(NOT VULKAN_PROFILES_GEN_RESULT EQUAL 0 OR NOT EXISTS "${VULKAN_PROFILES_GENERATED_HEADER}")
    message(FATAL_ERROR
            "Failed to generate vulkan_profiles.hpp:\n${VULKAN_PROFILES_GEN_OUTPUT}")
endif()
message(STATUS "Generated Vulkan Profiles header: ${VULKAN_PROFILES_GENERATED_HEADER}")

if(NOT TARGET VulkanProfiles_header)
    add_library(VulkanProfiles_header INTERFACE)
    # Expose the generated header's include root so `#include <vulkan/vulkan_profiles.hpp>`
    # resolves to our generated copy (which bakes in VP_KISHA_baseline) rather than
    # any system-installed one. Vulkan::Vulkan supplies the rest of the Vulkan headers.
    target_include_directories(VulkanProfiles_header INTERFACE "${VULKAN_PROFILES_GENERATED_DIR}")
    target_link_libraries(VulkanProfiles_header INTERFACE Vulkan::Vulkan)
endif()
add_library(Vulkan::Profiles ALIAS VulkanProfiles_header)
