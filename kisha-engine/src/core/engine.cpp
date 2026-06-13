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
    
    const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
        .setPApplicationName(create_info.application_name.c_str())
        .setApplicationVersion(create_info.application_version)
        .setPEngineName("kisha-engine")
        .setEngineVersion(create_info.engine_version)
        .setApiVersion(create_info.api_version);

    auto instance = util::create_instance(context, application_info, required_layers, required_instance_extensions);
    if (!instance) {
      return std::unexpected(instance.error());
    }


    return std::unexpected(EngineInitError::API_VERSION_TOO_LOW);
  }
}
