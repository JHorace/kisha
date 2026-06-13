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
  
DeviceSelection select_physical_device(const vk::raii::PhysicalDevices &physical_devices,
                                       const DeviceSpec &device_spec) {
    return {};
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
