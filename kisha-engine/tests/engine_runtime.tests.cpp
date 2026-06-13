#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <expected>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <catch2/catch_test_macros.hpp>

#include "engine.hpp"
#include "engine_init_helpers.hpp"

namespace {

}

TEST_CASE("Engine init enforces Vulkan 1.3+ baseline", "[engine][core]") {
  const kisha::engine::EngineCreateInfo create_info{
    .api_version = VK_API_VERSION_1_2
  };

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::ApiVersionTooLow);
}

TEST_CASE("Engine init reports missing required instance layers", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.required_instance_layers = {"VK_LAYER_KISHA_definitely_does_not_exist"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::MissingRequiredLayers);
}

TEST_CASE("Engine init reports missing required instance extensions", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.required_instance_extensions = {"VK_KISHA_definitely_does_not_exist_extension"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::MissingRequiredExtensions);
}

TEST_CASE("create_instance succeeds with default requirements", "[engine][core][gpu]") {
  const vk::raii::Context context;
  const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
      .setApiVersion(VK_API_VERSION_1_3);

  const std::expected<vk::raii::Instance, kisha::engine::EngineInitError> result =
    kisha::engine::util::create_instance(context, application_info, {}, {});

  REQUIRE(result.has_value());
  REQUIRE(static_cast<VkInstance>(**result) != VK_NULL_HANDLE);
}
