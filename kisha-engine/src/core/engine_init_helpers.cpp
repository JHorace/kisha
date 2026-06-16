//
// Created by jsumihiro on 6/12/26.
//

#include <algorithm>
#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>
#include <vulkan/vulkan_raii.hpp>

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

#include "errors.hpp"
#include "engine_init_helpers.hpp"

namespace kisha::engine::util {
  namespace {
    /**
     * @brief returns any features needed by the engine that are not supported by the given physical device
     *        Really this should take in vk::PhysicalDeviceFeatures{11,12,13} and validate against those but I can't be bothered.
     */
    std::vector<std::string> physical_device_missing_features(const vk::raii::PhysicalDevice &physical_device) {
      std::vector<std::string> missing_features = {};

      const auto feature_chain =
          physical_device.getFeatures2<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features, vk::PhysicalDeviceVulkan12Features,
            vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceShaderObjectFeaturesEXT,
            vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR, vk::PhysicalDeviceDescriptorBufferFeaturesEXT,
            vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR>();
      const auto &vulkan_11_features = feature_chain.get<vk::PhysicalDeviceVulkan11Features>();
      const auto &vulkan_12_features = feature_chain.get<vk::PhysicalDeviceVulkan12Features>();
      const auto &vulkan_13_features = feature_chain.get<vk::PhysicalDeviceVulkan13Features>();
      const auto &shader_object_features = feature_chain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>();
      const auto &unified_image_layouts_features =
          feature_chain.get<vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>();
      const auto &descriptor_buffer_features =
          feature_chain.get<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>();
      const auto &swapchain_maintenance_1_features =
          feature_chain.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR>();

      if (!vulkan_11_features.shaderDrawParameters)
        missing_features.emplace_back("shaderDrawParameters");
      if (!vulkan_12_features.timelineSemaphore)
        missing_features.emplace_back("timelineSemaphore");
      if (!vulkan_12_features.bufferDeviceAddress)
        missing_features.emplace_back("bufferDeviceAddress");

      if (!unified_image_layouts_features.unifiedImageLayouts)
        missing_features.emplace_back("unifiedImageLayouts");

      if (!vulkan_13_features.dynamicRendering)
        missing_features.emplace_back("dynamicRendering");

      if (!vulkan_13_features.synchronization2)
        missing_features.emplace_back("synchronization2");

      if (!shader_object_features.shaderObject)
        missing_features.emplace_back("shaderObject");

      if (!descriptor_buffer_features.descriptorBuffer)
        missing_features.emplace_back("descriptorBuffer");

      if (!swapchain_maintenance_1_features.swapchainMaintenance1)
        missing_features.emplace_back("swapchainMaintenance1");

      return missing_features;
    }
  }

  InstanceSpec reconcile(const InstanceSpec &engine, const InstanceSpec &app) {
    InstanceSpec result = engine;
    for (const std::string &name : app.required_extensions)
      append_unique(&result.required_extensions, name);
    for (const std::string &name : app.optional_extensions)
      append_unique(&result.optional_extensions, name);
    for (const std::string &name : app.required_layers)
      append_unique(&result.required_layers, name);
    for (const std::string &name : app.optional_layers)
      append_unique(&result.optional_layers, name);
    result.min_api_version = std::max(engine.min_api_version, app.min_api_version);
    return result;
  }

  DeviceSpec reconcile(const DeviceSpec &engine, const DeviceSpec &app) {
    DeviceSpec result = engine;
    for (const std::string &name : app.required_extensions)
      append_unique(&result.required_extensions, name);
    for (const std::string &name : app.optional_extensions)
      append_unique(&result.optional_extensions, name);
    result.require_discrete_gpu = engine.require_discrete_gpu || app.require_discrete_gpu;
    result.require_async_compute = engine.require_async_compute || app.require_async_compute;
    result.require_dedicated_transfer = engine.require_dedicated_transfer || app.require_dedicated_transfer;
    return result;
  }

  void append_unique(std::vector<std::string> *names, const std::string_view name) {
    const bool exists = std::ranges::any_of(*names, [&](const std::string &existing) { return existing == name; });
    if (!exists) {
      names->emplace_back(name);
    }
  }

  std::vector<const char *> to_c_string_ptrs(const std::vector<std::string> &names) {
    std::vector<const char *> out;
    out.reserve(names.size());
    for (const std::string &name : names) {
      out.push_back(name.c_str());
    }
    return out;
  }

  std::vector<std::string> enumerate_instance_extension_names(const vk::raii::Context &context) {
    const auto properties = context.enumerateInstanceExtensionProperties();
    if (!properties) {
      spdlog::error("Failed to enumerate instance extension properties: {}", vk::to_string(properties.error()));
      return {};
    }
    return *properties
        | std::views::transform([](const vk::ExtensionProperties &p) { return std::string(p.extensionName); })
        | std::ranges::to<std::vector>();
  }

  std::vector<std::string> enumerate_instance_layer_names(const vk::raii::Context &context) {
    const auto properties = context.enumerateInstanceLayerProperties();
    if (!properties) {
      spdlog::error("Failed to enumerate instance layer properties: {}", vk::to_string(properties.error()));
      return {};
    }
    return *properties
        | std::views::transform([](const vk::LayerProperties &p) { return std::string(p.layerName); })
        | std::ranges::to<std::vector>();
  }

  std::vector<std::string> enumerate_device_extension_names(const vk::raii::PhysicalDevice &physical_device) {
    const auto properties = physical_device.enumerateDeviceExtensionProperties();
    if (!properties) {
      spdlog::error("Failed to enumerate device extension properties: {}", vk::to_string(properties.error()));
      return {};
    }
    return *properties
        | std::views::transform([](const vk::ExtensionProperties &p) { return std::string(p.extensionName); })
        | std::ranges::to<std::vector>();
  }

  std::expected<void, MissingNamesError> validate_required_names(const std::vector<std::string> &available,
                                                                        const std::vector<std::string> &required) {
    const std::unordered_set<std::string> available_set(available.begin(), available.end());
    std::vector<std::string> missing = required
    | std::views::filter([&](const std::string &name) { return !available_set.contains(name); })
    | std::ranges::to<std::vector>();

    if (!missing.empty())
      return std::unexpected(MissingNamesError{std::move(missing)});
    return {};
  }

  std::expected<vk::raii::Instance, EngineInitError> create_instance(const vk::raii::Context &context,
                                                                     const vk::ApplicationInfo &application_info,
                                                                     const std::vector<std::string> &required_layers,
                                                                     const std::vector<std::string> &required_extensions) {
    if (auto r = validate_required_names(enumerate_instance_layer_names(context), required_layers); !r) {
      spdlog::error("Missing required Vulkan instance layers: {}", r.error().missing_names);
      return std::unexpected(EngineInitError::MissingRequiredLayers);
    }

    if (auto r = validate_required_names(enumerate_instance_extension_names(context), required_extensions); !r) {
      spdlog::error("Missing required Vulkan instance extensions: {}", r.error().missing_names);
      return std::unexpected(EngineInitError::MissingRequiredExtensions);
    }

    const std::vector<const char *> instance_layer_ptrs = to_c_string_ptrs(required_layers);
    const std::vector<const char *> instance_extension_ptrs = to_c_string_ptrs(required_extensions);

    const vk::InstanceCreateInfo instance_create_info = vk::InstanceCreateInfo{}
        .setPApplicationInfo(&application_info)
        .setEnabledLayerCount(std::uint32_t(instance_layer_ptrs.size()))
        .setPpEnabledLayerNames(instance_layer_ptrs.data())
        .setEnabledExtensionCount(std::uint32_t(instance_extension_ptrs.size()))
        .setPpEnabledExtensionNames(instance_extension_ptrs.data());

    return context.createInstance(instance_create_info)
        .transform_error([](const vk::Result result) {
          spdlog::error("Failed to create Vulkan instance: {}", vk::to_string(result));
          return EngineInitError::InstanceCreationFailed;
        });
  }

std::expected<QueueSelection, EngineInitError> select_queue_families(const vk::raii::PhysicalDevice &physical_device) {
  const std::vector<vk::QueueFamilyProperties> queue_properties = physical_device.getQueueFamilyProperties();

  std::optional<std::uint32_t> graphics_family;
  std::optional<std::uint32_t> dedicated_compute_family;
  std::optional<std::uint32_t> fallback_compute_family;
  std::optional<std::uint32_t> dedicated_transfer_family;
  std::optional<std::uint32_t> fallback_transfer_family;

  for (std::uint32_t family_index = 0; family_index < queue_properties.size(); ++family_index) {
    const vk::QueueFlags flags = queue_properties[family_index].queueFlags;

    if (!graphics_family.has_value() && flags & vk::QueueFlagBits::eGraphics) {
      graphics_family = family_index;
    }

    if (flags & vk::QueueFlagBits::eCompute) {
      if (!(flags & vk::QueueFlagBits::eGraphics)) {
        if (!dedicated_compute_family.has_value()) {
          dedicated_compute_family = family_index;
        }
      } else if (!fallback_compute_family.has_value()) {
        fallback_compute_family = family_index;
      }
    }

    if (flags & vk::QueueFlagBits::eTransfer) {
      const bool has_graphics = bool(flags & vk::QueueFlagBits::eGraphics);
      const bool has_compute = bool(flags & vk::QueueFlagBits::eCompute);
      if (!has_graphics && !has_compute) {
        if (!dedicated_transfer_family.has_value()) {
          dedicated_transfer_family = family_index;
        }
      } else if (!fallback_transfer_family.has_value()) {
        fallback_transfer_family = family_index;
      }
    }
  }

  if (!graphics_family.has_value()) {
    return std::unexpected(EngineInitError::NoSuitableQueueFamily);
  }

  const std::optional<std::uint32_t> async_compute_family =
    dedicated_compute_family
      .or_else([&] { return fallback_compute_family; })
      .or_else([&] { return graphics_family; });

  const std::optional<std::uint32_t> transfer_family =
    dedicated_transfer_family
      .or_else([&] { return fallback_transfer_family; })
      .or_else([&] { return async_compute_family; })
      .or_else([&] { return graphics_family; });

  return QueueSelection{
    .indices =
      QueueFamilyIndices{
        .graphics = *graphics_family,
        .present = *graphics_family,
        .async_compute = async_compute_family,
        .transfer = transfer_family,
      },
    .has_dedicated_async_compute = dedicated_compute_family.has_value(),
    .has_dedicated_transfer = dedicated_transfer_family.has_value(),
  };
}
  
std::expected<std::vector<DeviceSelection>, NoSuitableDeviceError> rank_physical_devices(const vk::raii::PhysicalDevices &physical_devices,
                                                                            const DeviceSpec &device_spec) {
  struct Candidate {
    std::size_t index = 0U;
    vk::PhysicalDeviceType type = vk::PhysicalDeviceType::eOther;
    QueueSelection queues{};
    std::vector<std::string> enabled_extensions;
    std::vector<std::string> missing_optional_extensions;
    std::size_t satisfied_optional = 0U;
  };

  std::vector<Candidate> discrete_candidates;
  std::vector<Candidate> integrated_candidates;
  // Diagnostics for every device that was considered but rejected.
  std::vector<DeviceRejection> rejected;

  for (std::size_t index = 0U; index < physical_devices.size(); ++index) {
    const vk::raii::PhysicalDevice &physical_device = physical_devices[index];
    const vk::PhysicalDeviceProperties properties = physical_device.getProperties();

    // Only discrete and integrated GPUs are considered as device candidates.
    if (properties.deviceType != vk::PhysicalDeviceType::eDiscreteGpu &&
        properties.deviceType != vk::PhysicalDeviceType::eIntegratedGpu) {
      continue;
    }

    DeviceRejection rejection{
      .device_name = std::string(properties.deviceName),
      .device_type = vk::to_string(properties.deviceType),
    };

    if (std::vector<std::string> missing_features = physical_device_missing_features(physical_device); !missing_features.empty()) {
      spdlog::debug("Skipping device '{}': missing required features: {}", rejection.device_name, missing_features);
      rejection.missing_features = std::move(missing_features);
      rejected.push_back(std::move(rejection));
      continue;
    }

    const std::vector<std::string> available_extensions = enumerate_device_extension_names(physical_device);
    if (auto r = validate_required_names(available_extensions, device_spec.required_extensions); !r) {
      spdlog::debug("Skipping device '{}': missing required extensions: {}", rejection.device_name, r.error().missing_names);
      rejection.missing_required_extensions = std::move(r.error().missing_names);
      rejected.push_back(std::move(rejection));
      continue;
    }

    const std::expected<QueueSelection, EngineInitError> queues = select_queue_families(physical_device);
    if (!queues) {
      spdlog::debug("Skipping device '{}': no suitable queue families", rejection.device_name);
      rejection.no_suitable_queue_family = true;
      rejected.push_back(std::move(rejection));
      continue;
    }
    if (device_spec.require_async_compute && !queues->has_dedicated_async_compute) {
      spdlog::debug("Skipping device '{}': no dedicated async-compute queue family", rejection.device_name);
      rejection.missing_async_compute = true;
      rejected.push_back(std::move(rejection));
      continue;
    }
    if (device_spec.require_dedicated_transfer && !queues->has_dedicated_transfer) {
      spdlog::debug("Skipping device '{}': no dedicated transfer queue family", rejection.device_name);
      rejection.missing_dedicated_transfer = true;
      rejected.push_back(std::move(rejection));
      continue;
    }

    const std::unordered_set<std::string> available_set(available_extensions.begin(), available_extensions.end());

    std::vector<std::string> enabled_extensions = device_spec.required_extensions;
    std::vector<std::string> missing_optional_extensions;
    std::size_t satisfied_optional = 0U;
    for (const std::string &name : device_spec.optional_extensions) {
      if (available_set.contains(name)) {
        append_unique(&enabled_extensions, name);
        ++satisfied_optional;
      } else {
        append_unique(&missing_optional_extensions, name);
      }
    }

    Candidate candidate{
      .index = index,
      .type = properties.deviceType,
      .queues = *queues,
      .enabled_extensions = std::move(enabled_extensions),
      .missing_optional_extensions = std::move(missing_optional_extensions),
      .satisfied_optional = satisfied_optional,
    };

    if (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu) {
      discrete_candidates.push_back(std::move(candidate));
    } else {
      integrated_candidates.push_back(std::move(candidate));
    }
  }

  // Order each tier by the number of satisfied optional extensions, best first;
  // ties keep enumeration order. Discrete GPUs always precede integrated ones.
  const auto by_optional_desc = [](const Candidate &a, const Candidate &b) {
    return a.satisfied_optional > b.satisfied_optional;
  };
  std::ranges::stable_sort(discrete_candidates, by_optional_desc);
  std::ranges::stable_sort(integrated_candidates, by_optional_desc);

  std::vector<Candidate> ordered = std::move(discrete_candidates);
  if (!device_spec.require_discrete_gpu) {
    // Integrated GPUs are only viable when a discrete GPU is not required.
    for (Candidate &candidate : integrated_candidates) {
      ordered.push_back(std::move(candidate));
    }
    integrated_candidates.clear();
  }

  if (!ordered.empty()) {
    std::vector<DeviceSelection> selections;
    selections.reserve(ordered.size());
    for (Candidate &candidate : ordered) {
      selections.push_back(DeviceSelection{
        .index = candidate.index,
        .queues = candidate.queues,
        .enabled_extensions = std::move(candidate.enabled_extensions),
        .missing_optional_extensions = std::move(candidate.missing_optional_extensions),
      });
    }
    return selections;
  }

  // No device was selected: any otherwise-suitable integrated GPU was rejected
  // solely because a discrete GPU is required.
  if (device_spec.require_discrete_gpu) {
    for (const Candidate &candidate : integrated_candidates) {
      const vk::raii::PhysicalDevice &physical_device = physical_devices[candidate.index];
      const vk::PhysicalDeviceProperties properties = physical_device.getProperties();
      rejected.push_back(DeviceRejection{
        .device_name = std::string(properties.deviceName),
        .device_type = vk::to_string(properties.deviceType),
        .discrete_gpu_required = true,
      });
    }
  }

  spdlog::error("No suitable physical device found among {} candidate(s)", rejected.size());
  return std::unexpected(NoSuitableDeviceError{.candidates = std::move(rejected)});
}

std::expected<DeviceSelection, NoSuitableDeviceError> select_physical_device(const vk::raii::PhysicalDevices &physical_devices,
                                                                            const DeviceSpec &device_spec) {
  return rank_physical_devices(physical_devices, device_spec)
      .transform([](std::vector<DeviceSelection> selections) { return std::move(selections.front()); });
}

  std::vector<vk::DeviceQueueCreateInfo> build_queue_create_infos(const QueueSelection &queues) {
    std::vector<std::uint32_t> unique_families;
    const auto add_family = [&](const std::uint32_t family) {
      if (std::ranges::find(unique_families, family) == unique_families.end()) {
        unique_families.push_back(family);
      }
    };
    add_family(queues.indices.graphics);
    add_family(queues.indices.present);
    if (queues.indices.async_compute.has_value()) {
      add_family(*queues.indices.async_compute);
    }
    if (queues.indices.transfer.has_value()) {
      add_family(*queues.indices.transfer);
    }

    static constexpr float queue_priority = 1.0F;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    queue_create_infos.reserve(unique_families.size());
    for (const std::uint32_t family : unique_families) {
      queue_create_infos.push_back(vk::DeviceQueueCreateInfo{}
          .setQueueFamilyIndex(family)
          .setQueueCount(1U)
          .setPQueuePriorities(&queue_priority));
    }
    return queue_create_infos;
  }

  std::expected<vk::raii::Device, EngineInitError> create_logical_device(const vk::raii::PhysicalDevice &physical_device,
                                                                         const QueueSelection &queues,
                                                                         const std::vector<std::string> &enabled_extensions) {
    const std::vector<vk::DeviceQueueCreateInfo> queue_create_infos = build_queue_create_infos(queues);

    const std::vector<const char *> extension_ptrs = to_c_string_ptrs(enabled_extensions);

    vk::StructureChain<vk::DeviceCreateInfo, vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan11Features,
                       vk::PhysicalDeviceVulkan12Features, vk::PhysicalDeviceVulkan13Features,
                       vk::PhysicalDeviceShaderObjectFeaturesEXT, vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR,
                       vk::PhysicalDeviceDescriptorBufferFeaturesEXT, vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR>
        chain;

    chain.get<vk::PhysicalDeviceVulkan11Features>().setShaderDrawParameters(true);
    chain.get<vk::PhysicalDeviceVulkan12Features>().setTimelineSemaphore(true).setBufferDeviceAddress(true);
    chain.get<vk::PhysicalDeviceVulkan13Features>().setDynamicRendering(true).setSynchronization2(true);
    chain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>().setShaderObject(true);
    chain.get<vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>().setUnifiedImageLayouts(true);
    chain.get<vk::PhysicalDeviceDescriptorBufferFeaturesEXT>().setDescriptorBuffer(true);
    chain.get<vk::PhysicalDeviceSwapchainMaintenance1FeaturesKHR>().setSwapchainMaintenance1(true);

    chain.get<vk::DeviceCreateInfo>()
        .setQueueCreateInfos(queue_create_infos)
        .setPEnabledExtensionNames(extension_ptrs);

    return physical_device.createDevice(chain.get<vk::DeviceCreateInfo>())
        .transform_error([](const vk::Result result) {
          spdlog::error("Failed to create logical device: {}", vk::to_string(result));
          return EngineInitError::DeviceCreationFailed;
        });
  }

  Queues acquire_queues(const vk::raii::Device &device, const QueueSelection &queues) {
    Queues result{};
    result.graphics = device.getQueue(queues.indices.graphics, 0U);
    if (queues.indices.async_compute.has_value()) {
      result.async_compute = device.getQueue(*queues.indices.async_compute, 0U);
    }
    if (queues.indices.transfer.has_value()) {
      result.transfer = device.getQueue(*queues.indices.transfer, 0U);
    }
    return result;
  }

  std::expected<vk::raii::SurfaceKHR, EngineInitError> create_surface(const vk::raii::Instance &instance,
                                                                      const NativeWindowHandle &window_handle) {
    const auto map_error = [](const vk::Result result) {
      spdlog::error("Failed to create surface: {}", vk::to_string(result));
      return EngineInitError::SurfaceCreationFailed;
    };

    return std::visit(
        [&](const auto &handle) -> std::expected<vk::raii::SurfaceKHR, EngineInitError> {
          using Handle = std::decay_t<decltype(handle)>;
          if constexpr (std::is_same_v<Handle, std::monostate>) {
            spdlog::error("Cannot create a surface from an empty (headless) native window handle");
            return std::unexpected(EngineInitError::SurfaceCreationFailed);
          }
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
          else if constexpr (std::is_same_v<Handle, WaylandWindowHandle>) {
            return instance
                .createWaylandSurfaceKHR(
                    vk::WaylandSurfaceCreateInfoKHR{}.setDisplay(handle.display).setSurface(handle.surface))
                .transform_error(map_error);
          }
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
          else if constexpr (std::is_same_v<Handle, XcbWindowHandle>) {
            return instance
                .createXcbSurfaceKHR(
                    vk::XcbSurfaceCreateInfoKHR{}.setConnection(handle.connection).setWindow(handle.window))
                .transform_error(map_error);
          }
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
          else if constexpr (std::is_same_v<Handle, XlibWindowHandle>) {
            return instance
                .createXlibSurfaceKHR(vk::XlibSurfaceCreateInfoKHR{}.setDpy(handle.display).setWindow(handle.window))
                .transform_error(map_error);
          }
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
          else if constexpr (std::is_same_v<Handle, Win32WindowHandle>) {
            return instance
                .createWin32SurfaceKHR(
                    vk::Win32SurfaceCreateInfoKHR{}.setHinstance(handle.hinstance).setHwnd(handle.hwnd))
                .transform_error(map_error);
          }
#endif
          else {
            return std::unexpected(EngineInitError::SurfaceCreationFailed);
          }
        },
        window_handle);
  }

  std::expected<SurfaceCapabilities, EngineInitError> query_surface_capabilities(const vk::raii::PhysicalDevice &physical_device,
                                                                                 const vk::raii::SurfaceKHR &surface) {
    const auto map_error = [](const vk::Result result, const std::string_view what) {
      spdlog::error("Failed to query surface {}: {}", what, vk::to_string(result));
      return EngineInitError::SurfaceCapabilityQueryFailed;
    };

    const vk::PhysicalDeviceSurfaceInfo2KHR surface_info = vk::PhysicalDeviceSurfaceInfo2KHR{}.setSurface(*surface);

    SurfaceCapabilities result{};

    {
      const std::expected<vk::SurfaceCapabilities2KHR, vk::Result> caps2 = physical_device.getSurfaceCapabilities2KHR(surface_info);
      if (!caps2) {
        return std::unexpected(map_error(caps2.error(), "capabilities"));
      }
      result.capabilities = caps2->surfaceCapabilities;
    }

    {
      const std::expected<std::vector<vk::SurfaceFormat2KHR>, vk::Result> formats2 = physical_device.getSurfaceFormats2KHR(surface_info);
      if (!formats2) {
        return std::unexpected(map_error(formats2.error(), "formats"));
      }
      result.formats.reserve(formats2->size());
      for (const vk::SurfaceFormat2KHR &format : *formats2) {
        result.formats.push_back(format.surfaceFormat);
      }
    }

    {
      std::expected<std::vector<vk::PresentModeKHR>, vk::Result> present_modes = physical_device.getSurfacePresentModesKHR(*surface);
      if (!present_modes) {
        return std::unexpected(map_error(present_modes.error(), "present modes"));
      }
      result.present_modes = std::move(*present_modes);
    }

    result.per_present_mode.reserve(result.present_modes.size());
    for (const vk::PresentModeKHR present_mode : result.present_modes) {
      const vk::SurfacePresentModeKHR present_mode_info = vk::SurfacePresentModeKHR{}.setPresentMode(present_mode);
      const vk::PhysicalDeviceSurfaceInfo2KHR mode_surface_info =
          vk::PhysicalDeviceSurfaceInfo2KHR{}.setSurface(*surface).setPNext(&present_mode_info);

      vk::SurfaceCapabilities2KHR caps2{};
      vk::SurfacePresentScalingCapabilitiesKHR scaling{};
      vk::SurfacePresentModeCompatibilityKHR compatibility{};
      caps2.pNext = &compatibility;
      compatibility.pNext = &scaling;

      const vk::Result count_result = physical_device.getSurfaceCapabilities2KHR(&mode_surface_info, &caps2);
      if (count_result != vk::Result::eSuccess) {
        return std::unexpected(map_error(count_result, "present-mode capabilities"));
      }

      std::vector<vk::PresentModeKHR> compatible_modes(compatibility.presentModeCount);
      compatibility.setPPresentModes(compatible_modes.data());

      const vk::Result data_result = physical_device.getSurfaceCapabilities2KHR(&mode_surface_info, &caps2);
      if (data_result != vk::Result::eSuccess) {
        return std::unexpected(map_error(data_result, "present-mode capabilities"));
      }
      compatible_modes.resize(compatibility.presentModeCount);

      result.per_present_mode.push_back(PresentModeCapabilities{
          .present_mode = present_mode,
          .min_image_count = caps2.surfaceCapabilities.minImageCount,
          .max_image_count = caps2.surfaceCapabilities.maxImageCount,
          .compatible_present_modes = std::move(compatible_modes),
          .supported_scaling = scaling.supportedPresentScaling,
          .supported_gravity_x = scaling.supportedPresentGravityX,
          .supported_gravity_y = scaling.supportedPresentGravityY,
          .min_scaled_image_extent = scaling.minScaledImageExtent,
          .max_scaled_image_extent = scaling.maxScaledImageExtent,
      });
    }

    return result;
  }

  VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT severity, const VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void * /*user_data*/
  ) {
    const char *message = (callback_data != nullptr && callback_data->pMessage != nullptr) ? callback_data->pMessage : "<no message>";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
      spdlog::error("Vulkan validation [{:#x}]: {}", std::uint32_t(message_type), message);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
      spdlog::warn("Vulkan validation [{:#x}]: {}", std::uint32_t(message_type), message);
    } else {
      spdlog::info("Vulkan validation [{:#x}]: {}", std::uint32_t(message_type), message);
    }
    return VK_FALSE;
  }
}
