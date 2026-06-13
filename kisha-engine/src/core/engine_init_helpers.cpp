//
// Created by jsumihiro on 6/12/26.
//

#include <algorithm>
#include <expected>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_set>
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
            vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>();
      const vk::PhysicalDeviceVulkan11Features &vulkan_11_features = feature_chain.get<vk::PhysicalDeviceVulkan11Features>();
      const vk::PhysicalDeviceVulkan12Features &vulkan_12_features = feature_chain.get<vk::PhysicalDeviceVulkan12Features>();
      const vk::PhysicalDeviceVulkan13Features &vulkan_13_features = feature_chain.get<vk::PhysicalDeviceVulkan13Features>();
      const vk::PhysicalDeviceShaderObjectFeaturesEXT &shader_object_features = feature_chain.get<vk::PhysicalDeviceShaderObjectFeaturesEXT>();
      const vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR &unified_image_layouts_features =
          feature_chain.get<vk::PhysicalDeviceUnifiedImageLayoutsFeaturesKHR>();

      if (!vulkan_11_features.shaderDrawParameters)
        missing_features.push_back("shaderDrawParameters");
      if (!vulkan_12_features.timelineSemaphore)
        missing_features.push_back("timelineSemaphore");

      if (!unified_image_layouts_features.unifiedImageLayouts)
        missing_features.push_back("unifiedImageLayouts");

      if (!vulkan_13_features.dynamicRendering)
        missing_features.push_back("dynamicRendering");

      if (!vulkan_13_features.synchronization2)
        missing_features.push_back("synchronization2");

      if (!shader_object_features.shaderObject)
        missing_features.push_back("shaderObject");

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
    return context.enumerateInstanceExtensionProperties()
        | std::views::transform([](const vk::ExtensionProperties &p) { return std::string(p.extensionName); })
        | std::ranges::to<std::vector>();
  }

  std::vector<std::string> enumerate_instance_layer_names(const vk::raii::Context &context) {
    return context.enumerateInstanceLayerProperties()
        | std::views::transform([](const vk::LayerProperties &p) { return std::string(p.layerName); })
        | std::ranges::to<std::vector>();
  }

  std::vector<std::string> enumerate_device_extension_names(const vk::raii::PhysicalDevice &physical_device) {
    return physical_device.enumerateDeviceExtensionProperties()
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

    return vk::raii::Instance(context, instance_create_info);
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
        .async_compute = async_compute_family,
        .transfer = transfer_family,
      },
    .has_dedicated_async_compute = dedicated_compute_family.has_value(),
    .has_dedicated_transfer = dedicated_transfer_family.has_value(),
  };
}
  
std::expected<DeviceSelection, NoSuitableDeviceError> select_physical_device(const vk::raii::PhysicalDevices &physical_devices,
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

  // Prefer the discrete GPU satisfying the most optional extensions; ties keep the first found.
  const auto best_by_optional = [](const std::vector<Candidate> &candidates) -> const Candidate * {
    const Candidate *best = nullptr;
    for (const Candidate &candidate : candidates) {
      if (best == nullptr || candidate.satisfied_optional > best->satisfied_optional) {
        best = &candidate;
      }
    }
    return best;
  };

  const Candidate *selected = best_by_optional(discrete_candidates);

  if (selected == nullptr && !device_spec.require_discrete_gpu) {
    // Only fall back to an integrated GPU when a discrete one is not required and none is suitable.
    selected = best_by_optional(integrated_candidates);
  }

  if (selected != nullptr) {
    return DeviceSelection{
      .index = selected->index,
      .queues = selected->queues,
      .enabled_extensions = selected->enabled_extensions,
      .missing_optional_extensions = selected->missing_optional_extensions,
    };
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
