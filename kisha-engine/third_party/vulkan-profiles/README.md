# Vulkan Profiles

The engine uses the Khronos
[Vulkan-Profiles](https://github.com/KhronosGroup/Vulkan-Profiles) client API
(`vulkan/vulkan_profiles.hpp`: `vpGetPhysicalDeviceProfileSupport`,
`vpCreateDevice`, ...).

## What is Vulkan Profiles?

[Vulkan Profiles](https://github.com/KhronosGroup/Vulkan-Profiles) is a Khronos
toolset that captures a named, versioned set of Vulkan **requirements** — an API
version, device/instance extensions, feature bits and limits — in a single JSON
definition. From that JSON it generates a header-only client API
(`vulkan_profiles.hpp`) that lets applications:

- **Query support** for a profile on a given instance/physical device
  (`vpGetInstanceProfileSupport`, `vpGetPhysicalDeviceProfileSupport`), and
- **Create** instances/devices that have exactly the profile's extensions and
  features enabled (`vpCreateInstance`, `vpCreateDevice`).

### What it is used for

The key benefit for us is that the relationship between an *extension* and the
*physical-device feature structs* it needs (the structs that must be chained into
`VkDeviceCreateInfo::pNext`) is encoded once in the profile JSON. The generated
code knows which `VkPhysicalDevice*Features` structs to populate and chain, so we
don't hand-maintain that brittle mapping or per-feature checks ourselves. A
profile is the single source of truth for "what this engine requires from a GPU".

## How the kisha engine uses it

- The engine's required capabilities are authored as a profile,
  **`VP_KISHA_baseline`**, in
  [`kisha-engine/profiles/VP_KISHA_baseline.json`](../../profiles/VP_KISHA_baseline.json).
  This is the source of truth for the engine's required API version, extensions
  (`VK_EXT_shader_object`, `VK_KHR_unified_image_layouts`,
  `VK_EXT_descriptor_buffer`, `VK_KHR_swapchain_maintenance1`) and feature bits
  (`shaderDrawParameters`, `timelineSemaphore`, `bufferDeviceAddress`,
  `dynamicRendering`, `synchronization2`, `shaderObject`, `unifiedImageLayouts`,
  `descriptorBuffer`, `swapchainMaintenance1`).
- That profile is baked into the `vulkan_profiles.hpp` we generate at configure
  time, which exposes the `VP_KISHA_BASELINE_NAME` /
  `VP_KISHA_BASELINE_SPEC_VERSION` macros.
- At runtime, device selection uses the profile instead of hand-written feature
  checks. In `kisha-engine/src/core/engine_init_helpers.cpp`:
    - `engine_profile()` builds the `VpProfileProperties` for `VP_KISHA_baseline`.
    - `device_supports_profile()` wraps `vpGetPhysicalDeviceProfileSupport()` to
      reject physical devices that do not satisfy the profile.
    - `select_physical_device()` skips any device that fails the profile-support
      check, recording a rejection diagnostic.

## What to change if engine or app requirements change

When the engine (or an app's) GPU requirements change — adding/removing a
required extension, feature bit or bumping the required API version — the change
flows through the profile rather than scattered code:

1. **Edit the profile JSON**
   ([`kisha-engine/profiles/VP_KISHA_baseline.json`](../../profiles/VP_KISHA_baseline.json)):
   add/remove the extension under `capabilities.*.extensions`, the feature bit
   under the appropriate `VkPhysicalDevice*Features` struct in
   `capabilities.*.features`, and/or update `api-version`. Bump the profile
   `version` when the requirements change meaningfully.
2. **Regenerate** the vendored `vulkan_profiles.hpp` so it embeds the updated
   profile (see *How to regenerate* below). Nothing in the C++ runtime needs to
   change for a pure requirement change — `engine_profile()` /
   `device_supports_profile()` pick up the new requirements automatically.
3. **If a newly required extension also needs the engine to opt into a feature
   struct on the logical device**, that chaining is handled by `vpCreateDevice`
   once device creation is wired up (the profile drives the `pNext` chain). No
   manual feature struct chaining is required.
4. **Rebuild and run the tests** to confirm a profile-supporting device is still
   selected on your hardware.

> Note: app-supplied extras that are *not* part of the engine profile (e.g. a
> game's optional extensions) continue to flow through `DeviceSpec` /
> `InstanceSpec` reconciliation, independent of the profile.

## How it is consumed (generated at configure time)

We **generate** `vulkan/vulkan_profiles.hpp` ourselves at CMake configure time so
the engine's own `VP_KISHA_baseline` profile is baked directly into the header.
This is wired up in [`cmake/ProjectDependencies.cmake`](../../../cmake/ProjectDependencies.cmake):

1. The Vulkan-Profiles repository is fetched **download-only** via CPM
   (`CPMAddPackage(... DOWNLOAD_ONLY YES)`), pinned to tag
   `vulkan-sdk-1.4.350.0`. We do **not** build the upstream CMake project — we
   only need its generator script `scripts/gen_profiles_solution.py`.
2. The script is run against the system Vulkan registry (`vk.xml`) and our
   profiles directory (`kisha-engine/profiles/`), emitting the header into
   `${CMAKE_BINARY_DIR}/vulkan-profiles-generated/vulkan/vulkan_profiles.hpp`.
3. That generated directory is exposed as include root through the
   `Vulkan::Profiles` INTERFACE target, so `#include <vulkan/vulkan_profiles.hpp>`
   resolves to our generated copy (which contains `VP_KISHA_baseline`).

### Why generate instead of using a system/SDK header?

The Vulkan SDK (or the distro `vulkan-profiles` package) does ship a prebuilt
`vulkan_profiles.hpp`, but it only bakes in the **upstream standard profiles** —
not our custom `VP_KISHA_baseline`. Since the engine references
`VP_KISHA_BASELINE_NAME` / `VP_KISHA_BASELINE_SPEC_VERSION` and queries the
profile via `vpGetPhysicalDeviceProfileSupport`, the profile must be compiled
into the header — which means we have to generate it ourselves.

We still avoid the *full* upstream CMake build (it pulls in `Vulkan-Headers`,
`Vulkan-Utility-Libraries`, `valijson`, `jsoncpp` and builds the profiles layer);
the header-only client API generated by the script is all we need.

## Prerequisites

Generation happens automatically during `cmake` configure and requires:

- **Python 3** (`find_package(Python3 ...)`), used to run the generator script.
- The **Vulkan registry** `vk.xml`, which ships with the Vulkan headers/SDK
  (e.g. `/usr/share/vulkan/registry/vk.xml`, or `$VULKAN_SDK/share/vulkan/registry`).
- Network access on the first configure so CPM can clone Vulkan-Profiles.

If `vk.xml` or the generation step fails, CMake aborts at configure time with a
clear message.

## Version

- The CPM `GIT_TAG` (`vulkan-sdk-1.4.350.0`) and the `vk.xml` version must match
  the system `VK_HEADER_VERSION` (currently `350`).

> When bumping the Vulkan SDK / headers, update the CPM `GIT_TAG` in
> `cmake/ProjectDependencies.cmake` to the matching `vulkan-sdk-*` tag so the
> generator stays in sync with the registry/headers `Vulkan::Vulkan` provides.

## Manual regeneration (for inspection/debugging)

The build does this automatically; to reproduce by hand:

```bash
git clone --depth 1 --branch vulkan-sdk-1.4.350.0 \
    https://github.com/KhronosGroup/Vulkan-Profiles.git vp

python3 vp/scripts/gen_profiles_solution.py \
    --registry /usr/share/vulkan/registry/vk.xml \
    --input <repo>/kisha-engine/profiles \
    --output-library-inc ./out/vulkan
# -> ./out/vulkan/vulkan_profiles.hpp (contains VP_KISHA_baseline)
```

Notes:
- `--registry` must point at a `vk.xml` whose version matches the Vulkan headers
  the engine compiles against.
- `--input` points at `kisha-engine/profiles/` (the source of truth); every
  profile JSON there is baked into the header. Keep that directory free of
  non-profile config files (no top-level `capabilities` key).
