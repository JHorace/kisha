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

TEST_CASE("Engine baseline raises an app api version below 1.3", "[engine][core][gpu]") {
  // The engine baseline requires Vulkan 1.3, and reconciliation takes the max,
  // so an app requesting 1.2 is bumped to 1.3 rather than failing ApiVersionTooLow.
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_2;

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  // Initialization must never fail because of the API version: on a capable GPU it
  // succeeds outright, and otherwise it fails for some other (environment) reason.
  REQUIRE((result.has_value() || result.error() != kisha::engine::EngineInitError::ApiVersionTooLow));
}

TEST_CASE("Engine init creates a logical device with default requirements", "[engine][core][gpu]") {
  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create();

  REQUIRE(result.has_value());
}

TEST_CASE("EngineInstance init enumerates at least one ranked device candidate", "[engine][core][gpu]") {
  // Phase 1 builds the instance/debug messenger and ranks the physical devices
  // without creating a logical device. On a capable GPU this yields candidates.
  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineInitError> result =
    kisha::engine::EngineInstance::create();

  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->device_candidates().empty());
}

TEST_CASE("EngineInstance only ranks discrete GPUs under the default spec", "[engine][core][gpu]") {
  // The engine device baseline requires a discrete GPU, so every ranked
  // candidate must be a discrete GPU and indices must stay in range.
  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineInitError> result =
    kisha::engine::EngineInstance::create();

  REQUIRE(result.has_value());

  const vk::raii::PhysicalDevices &devices = result->physical_devices();
  for (const kisha::engine::DeviceSelection &candidate : result->device_candidates()) {
    REQUIRE(candidate.index < devices.size());
    const vk::PhysicalDeviceProperties properties = devices[candidate.index].getProperties();
    REQUIRE(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu);
  }
}

TEST_CASE("EngineInstance init reports missing required instance layers", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_layers = {"VK_LAYER_KISHA_definitely_does_not_exist"};

  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineInitError> result =
    kisha::engine::EngineInstance::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::MissingRequiredLayers);
}

TEST_CASE("Engine init reports missing required instance layers", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_layers = {"VK_LAYER_KISHA_definitely_does_not_exist"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::MissingRequiredLayers);
}

TEST_CASE("Engine init reports missing required instance extensions", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_extensions = {"VK_KISHA_definitely_does_not_exist_extension"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineInitError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineInitError::MissingRequiredExtensions);
}

TEST_CASE("reconcile(InstanceSpec) unions names and takes max api version", "[engine][core]") {
  kisha::engine::InstanceSpec engine_spec{};
  engine_spec.required_extensions = {"VK_EXT_a", "VK_EXT_shared"};
  engine_spec.optional_extensions = {"VK_EXT_opt_engine"};
  engine_spec.required_layers = {"VK_LAYER_engine"};
  engine_spec.optional_layers = {"VK_LAYER_opt_shared"};
  engine_spec.min_api_version = VK_API_VERSION_1_3;

  kisha::engine::InstanceSpec app_spec{};
  app_spec.required_extensions = {"VK_EXT_shared", "VK_EXT_b"};
  app_spec.optional_extensions = {"VK_EXT_opt_app"};
  app_spec.required_layers = {"VK_LAYER_app"};
  app_spec.optional_layers = {"VK_LAYER_opt_shared"};
  app_spec.min_api_version = VK_API_VERSION_1_2;

  const kisha::engine::InstanceSpec result = kisha::engine::util::reconcile(engine_spec, app_spec);

  REQUIRE(result.required_extensions == std::vector<std::string>{"VK_EXT_a", "VK_EXT_shared", "VK_EXT_b"});
  REQUIRE(result.optional_extensions == std::vector<std::string>{"VK_EXT_opt_engine", "VK_EXT_opt_app"});
  REQUIRE(result.required_layers == std::vector<std::string>{"VK_LAYER_engine", "VK_LAYER_app"});
  REQUIRE(result.optional_layers == std::vector<std::string>{"VK_LAYER_opt_shared"});
  REQUIRE(result.min_api_version == VK_API_VERSION_1_3);
}

TEST_CASE("reconcile(DeviceSpec) unions extensions and ORs capability flags", "[engine][core]") {
  kisha::engine::DeviceSpec engine_spec{};
  engine_spec.required_extensions = {"VK_KHR_a", "VK_KHR_shared"};
  engine_spec.optional_extensions = {"VK_KHR_opt_engine"};
  engine_spec.require_discrete_gpu = false;
  engine_spec.require_async_compute = true;
  engine_spec.require_dedicated_transfer = false;

  kisha::engine::DeviceSpec app_spec{};
  app_spec.required_extensions = {"VK_KHR_shared", "VK_KHR_b"};
  app_spec.optional_extensions = {"VK_KHR_opt_app"};
  app_spec.require_discrete_gpu = true;
  app_spec.require_async_compute = false;
  app_spec.require_dedicated_transfer = false;

  const kisha::engine::DeviceSpec result = kisha::engine::util::reconcile(engine_spec, app_spec);

  REQUIRE(result.required_extensions == std::vector<std::string>{"VK_KHR_a", "VK_KHR_shared", "VK_KHR_b"});
  REQUIRE(result.optional_extensions == std::vector<std::string>{"VK_KHR_opt_engine", "VK_KHR_opt_app"});
  REQUIRE(result.require_discrete_gpu);
  REQUIRE(result.require_async_compute);
  REQUIRE_FALSE(result.require_dedicated_transfer);
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
