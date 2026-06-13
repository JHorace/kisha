//
// Created by jsumihiro on 6/12/26.
//

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
    return {};   // success, no payload
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
        .setEnabledLayerCount(static_cast<std::uint32_t>(instance_layer_ptrs.size()))
        .setPpEnabledLayerNames(instance_layer_ptrs.data())
        .setEnabledExtensionCount(static_cast<std::uint32_t>(instance_extension_ptrs.size()))
        .setPpEnabledExtensionNames(instance_extension_ptrs.data());

    return vk::raii::Instance(context, instance_create_info);
  }

  VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(const VkDebugUtilsMessageSeverityFlagBitsEXT severity, const VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void * /*user_data*/
  ) {
    const char *message = (callback_data != nullptr && callback_data->pMessage != nullptr) ? callback_data->pMessage : "<no message>";
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0U) {
      spdlog::error("Vulkan validation [{:#x}]: {}", static_cast<std::uint32_t>(message_type), message);
    } else if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) != 0U) {
      spdlog::warn("Vulkan validation [{:#x}]: {}", static_cast<std::uint32_t>(message_type), message);
    } else {
      spdlog::info("Vulkan validation [{:#x}]: {}", static_cast<std::uint32_t>(message_type), message);
    }
    return VK_FALSE;
  }
}
