# Vulkan Profiles

The engine uses the Khronos
[Vulkan-Profiles](https://github.com/KhronosGroup/Vulkan-Profiles) client API
(`vulkan/vulkan_profiles.hpp`: `vpGetPhysicalDeviceProfileSupport`,
`vpCreateDevice`, ...).

## How it is consumed

The Vulkan Profiles client header (`vulkan/vulkan_profiles.hpp`) is provided by
the platform's Vulkan Profiles package, installed alongside `vulkan.hpp` on the
standard Vulkan include path:

- The **LunarG Vulkan SDK** bundles it, or
- a distro package provides it (on Arch/EndeavourOS: `vulkan-profiles`, on
  Debian/Ubuntu: the LunarG SDK packages).

### Why not fetch/generate or build the upstream project?

- The header is **version-matched** to the system Vulkan headers when it comes
  from the same package set, so there is no risk of drift.
- The full Vulkan-Profiles CMake build pulls in `Vulkan-Headers`,
  `Vulkan-Utility-Libraries`, `valijson`, `jsoncpp` and Python tooling and also
  builds the profiles layer; we only need the header-only client API.

## Prerequisite

Install the Vulkan Profiles header before configuring:

- Arch / EndeavourOS: `sudo pacman -S vulkan-profiles`
- LunarG Vulkan SDK: install the SDK and set `VULKAN_SDK` (the header ships under
  `$VULKAN_SDK/include/vulkan/`).

If the header is missing, CMake fails at configure time with a clear message.

## Version

- Use a Vulkan Profiles package matching the system `VK_HEADER_VERSION`
  (currently `350`, i.e. the `1.4.350.0` package series).

> Keep the Vulkan Profiles package in sync with the Vulkan headers provided by
> `Vulkan::Vulkan`. When bumping the Vulkan SDK / headers, install the matching
> `vulkan-profiles` version.
