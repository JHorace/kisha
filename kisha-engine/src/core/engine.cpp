/**
 * @file engine.cpp
 * @brief Vulkan core initialization implementation using `vk::raii`.
 */
#include "engine.hpp"
#include "engine_init_helpers.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

namespace kisha::engine {
  std::expected<EngineCore, EngineInitError> EngineCore::create(const EngineCreateInfo &create_info) {
    if (create_info.api_version < VK_API_VERSION_1_3) {
      return std::unexpected(EngineInitError::API_VERSION_TOO_LOW);
    }

    vk::raii::Context context;

    std::vector<std::string> required_layers = create_info.required_instance_layers;
    std::vector<std::string> required_instance_extensions = create_info.required_instance_extensions;
    if (create_info.enable_validation) {
      util::append_unique(&required_layers, "VK_LAYER_KHRONOS_validation");
      util::append_unique(&required_instance_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    
    if (auto r = util::validate_required_names(util::enumerate_instance_layer_names(context), required_layers); !r) {
      spdlog::error("Missing required Vulkan instance layers: {}", r.error().missing_names);
      return std::unexpected(EngineInitError::MissingRequiredLayers);
    }

    if (auto r = util::validate_required_names(util::enumerate_instance_extension_names(context), required_instance_extensions); !r) {
      spdlog::error("Missing required Vulkan instance extensions: {}", r.error().missing_names);
      return std::unexpected(EngineInitError::MissingRequiredExtensions);
    }

    const std::vector<const char *> instance_layer_ptrs = util::to_c_string_ptrs(required_layers);
    const std::vector<const char *> instance_extension_ptrs = util::to_c_string_ptrs(required_instance_extensions);

    const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
        .setPApplicationName(create_info.application_name.c_str())
        .setApplicationVersion(create_info.application_version)
        .setPEngineName("kisha-engine")
        .setEngineVersion(create_info.engine_version)
        .setApiVersion(create_info.api_version);

    const vk::InstanceCreateInfo instance_create_info = vk::InstanceCreateInfo{}
        .setPApplicationInfo(&application_info)
        .setEnabledLayerCount(static_cast<std::uint32_t>(instance_layer_ptrs.size()))
        .setPpEnabledLayerNames(instance_layer_ptrs.data())
        .setEnabledExtensionCount(static_cast<std::uint32_t>(instance_extension_ptrs.size()))
        .setPpEnabledExtensionNames(instance_extension_ptrs.data());

    vk::raii::Instance instance(context, instance_create_info);


    return std::unexpected(EngineInitError::API_VERSION_TOO_LOW);
  }
}
