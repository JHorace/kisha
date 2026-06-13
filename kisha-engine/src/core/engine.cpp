/**
 * @file engine.cpp
 * @brief Vulkan core initialization implementation using `vk::raii`.
 */
#include "engine.hpp"
#include "engine_init_helpers.hpp"

namespace kisha::engine {
  std::expected<EngineCore, EngineInitError> EngineCore::create(const EngineCreateInfo &create_info) {
    if (create_info.api_version < VK_API_VERSION_1_3) {
      return std::unexpected(EngineInitError::API_VERSION_TOO_LOW);
    }

    vk::raii::Context context;

    return std::unexpected(EngineInitError::API_VERSION_TOO_LOW);
  }
}
