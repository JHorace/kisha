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
- That profile is baked into the generated `vulkan_profiles.hpp` (alongside the
  upstream standard profiles), which exposes the `VP_KISHA_BASELINE_NAME` /
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
 
## How to regenerate

```bash
git clone --depth 1 --branch <vulkan-sdk-tag> \
    https://github.com/KhronosGroup/Vulkan-Profiles.git vp
cd vp

# Assemble the input profile set: the upstream standard profiles plus the
# engine's own profile (the source of truth lives in kisha-engine/profiles/).
mkdir -p clean_profiles
cp profiles/*.json clean_profiles/
# Drop non-profile config files (no top-level `capabilities` key).
rm -f clean_profiles/VP_LUNARG_desktop_baseline_config.json
cp <repo>/kisha-engine/profiles/VP_KISHA_baseline.json clean_profiles/

# Generate using the system Vulkan registry.
PYTHONPATH=/usr/share/vulkan/registry \
python3 scripts/gen_profiles_solution.py \
    --registry /usr/share/vulkan/registry/vk.xml \
    --input ./clean_profiles \
    --output-library-inc ./out

cp ./out/vulkan_profiles.hpp \
   <repo>/kisha-engine/third_party/vulkan-profiles/include/vulkan/vulkan_profiles.hpp
```

Notes:
- `--registry` must point at a `vk.xml` whose version matches the Vulkan headers
  the engine compiles against.
- The engine's own profile (`VP_KISHA_baseline`, defining the required
  features/extensions/API version) is authored in
  `kisha-engine/profiles/VP_KISHA_baseline.json` and must be included in the
  generated header — that is what `engine_profile()` /
  `device_supports_profile()` look up at runtime.
- If the upstream `profiles/` directory contains non-profile config files (no
  `capabilities` key, e.g. `VP_LUNARG_desktop_baseline_config.json`), exclude
  them from the input directory before generating.
