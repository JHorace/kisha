include_guard(GLOBAL)

include(CPM)

CPMAddPackage("gh:gabime/spdlog#v1.17.0")
CPMAddPackage("gh:g-truc/glm#1.0.3")
CPMAddPackage("gh:catchorg/Catch2#v3.15.0")
CPMAddPackage("gh:GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator#v3.2.1")
CPMAddPackage("gh:fmtlib/fmt#11.0.2")

find_package(Vulkan REQUIRED)
get_target_property(VULKAN_LIB_LOCATION Vulkan::Vulkan IMPORTED_LOCATION)
message(STATUS "Found Vulkan Library: ${VULKAN_LIB_LOCATION}")

# We are requiring slangc be already installed because the build take a long time
find_program(SLANG_COMPILER_BIN slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)
