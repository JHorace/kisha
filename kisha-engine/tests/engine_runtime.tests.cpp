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

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> result =
    kisha::engine::EngineCore::create(create_info);

  // Initialization must never fail because of the API version: on a capable GPU it
  // succeeds outright, and otherwise it fails for some other (environment) reason.
  REQUIRE((result.has_value() || result.error() != kisha::engine::EngineError::ApiVersionTooLow));
}

TEST_CASE("Engine init creates a logical device with default requirements", "[engine][core][gpu]") {
  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> result =
    kisha::engine::EngineCore::create();

  REQUIRE(result.has_value());
}

TEST_CASE("EngineInstance init enumerates at least one ranked device candidate", "[engine][core][gpu]") {
  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineError> result =
    kisha::engine::EngineInstance::create();

  REQUIRE(result.has_value());
  REQUIRE_FALSE(result->device_candidates().empty());
}

TEST_CASE("EngineInstance only ranks discrete GPUs under the default spec", "[engine][core][gpu]") {
  // The engine device baseline requires a discrete GPU, so every ranked
  // candidate must be a discrete GPU and indices must stay in range.
  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineError> result =
    kisha::engine::EngineInstance::create();

  REQUIRE(result.has_value());

  const vk::raii::PhysicalDevices &devices = result->physical_devices();
  for (const kisha::engine::DeviceSelection &candidate : result->device_candidates()) {
    REQUIRE(candidate.index < devices.size());
    const vk::PhysicalDeviceProperties properties = devices[candidate.index].getProperties();
    REQUIRE(properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu);
  }
}

TEST_CASE("EngineCore is produced from EngineInstance with an active candidate", "[engine][core][gpu]") {
  std::expected<kisha::engine::EngineInstance, kisha::engine::EngineError> instance =
    kisha::engine::EngineInstance::create();
  REQUIRE(instance.has_value());

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> core =
    std::move(*instance).create_engine_core();

  REQUIRE(core.has_value());
  REQUIRE(static_cast<VkDevice>(*core->device()) != VK_NULL_HANDLE);
  REQUIRE_FALSE(core->device_candidates().empty());
  REQUIRE(core->queue_family_indices().present == core->queue_family_indices().graphics);
}

TEST_CASE("EngineCore::create yields a present family mirroring graphics", "[engine][core][gpu]") {
  // The eager EngineCore::create path now delegates to the two-phase init, so it
  // must still produce a working device whose present family mirrors graphics.
  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> core =
    kisha::engine::EngineCore::create();

  REQUIRE(core.has_value());
  REQUIRE(core->queue_family_indices().present == core->queue_family_indices().graphics);
}

TEST_CASE("EngineCore exposes a profile matching the active device", "[engine][core][gpu]") {
  // The EngineProfile is populated at device creation from the active candidate's
  // physical-device properties and the negotiated extension lists, and must agree
  // with the live physical device exposed by the core.
  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> core =
    kisha::engine::EngineCore::create();

  REQUIRE(core.has_value());

  const kisha::engine::EngineProfile &profile = core->profile();
  const vk::PhysicalDeviceProperties properties = core->physical_device().getProperties();

  REQUIRE(profile.device_name == std::string(properties.deviceName));
  REQUIRE(profile.device_type == properties.deviceType);
  REQUIRE(profile.vendor_id == properties.vendorID);
  REQUIRE(profile.device_id == properties.deviceID);
  REQUIRE(profile.api_version == properties.apiVersion);
  // The profile mirrors the active candidate's negotiated extension lists.
  const kisha::engine::DeviceSelection &active = core->device_candidates().front();
  REQUIRE(profile.enabled_extensions == active.enabled_extensions);
  REQUIRE(profile.missing_optional_extensions == active.missing_optional_extensions);
}

TEST_CASE("EngineCore::create_presenter fails for an empty (headless) window handle", "[engine][core][gpu]") {
  std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> core =
    kisha::engine::EngineCore::create();
  REQUIRE(core.has_value());

  const std::expected<kisha::engine::Presenter *, kisha::engine::EngineError> presenter =
    core->create_presenter(kisha::engine::NativeWindowHandle{});

  REQUIRE_FALSE(presenter.has_value());
  REQUIRE(presenter.error() == kisha::engine::EngineError::SurfaceCreationFailed);
  // The failed bind must not leave a dangling Presenter on the core.
  REQUIRE(core->presenter() == nullptr);
}

TEST_CASE("SurfaceCapabilities exposes sane defaults for the Phase 4 capability layer", "[engine][core]") {
  const kisha::engine::PresentModeCapabilities mode_caps{};
  REQUIRE(mode_caps.present_mode == vk::PresentModeKHR::eFifo);
  REQUIRE(mode_caps.min_image_count == 0U);
  REQUIRE(mode_caps.max_image_count == 0U);
  REQUIRE(mode_caps.compatible_present_modes.empty());

  const kisha::engine::SurfaceCapabilities caps{};
  REQUIRE(caps.formats.empty());
  REQUIRE(caps.present_modes.empty());
  REQUIRE(caps.per_present_mode.empty());
}

TEST_CASE("SwapchainConfig exposes sane defaults for the Phase 5 swapchain layer", "[engine][core]") {
  const kisha::engine::SwapchainConfig config{};
  REQUIRE(config.extent.width == 0U);
  REQUIRE(config.extent.height == 0U);
  REQUIRE(config.surface_format.format == vk::Format::eB8G8R8A8Srgb);
  REQUIRE(config.surface_format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear);
  REQUIRE(config.present_mode == vk::PresentModeKHR::eFifo);
  REQUIRE(config.min_image_count == 0U);
  REQUIRE(config.image_usage == vk::ImageUsageFlagBits::eColorAttachment);
}

TEST_CASE("EngineInstance init reports missing required instance layers", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_layers = {"VK_LAYER_KISHA_definitely_does_not_exist"};

  const std::expected<kisha::engine::EngineInstance, kisha::engine::EngineError> result =
    kisha::engine::EngineInstance::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineError::MissingRequiredLayers);
}

TEST_CASE("Engine init reports missing required instance layers", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_layers = {"VK_LAYER_KISHA_definitely_does_not_exist"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineError::MissingRequiredLayers);
}

TEST_CASE("Engine init reports missing required instance extensions", "[engine][core]") {
  kisha::engine::EngineCreateInfo create_info{};
  create_info.instance_spec.min_api_version = VK_API_VERSION_1_3;
  create_info.instance_spec.required_extensions = {"VK_KISHA_definitely_does_not_exist_extension"};

  const std::expected<kisha::engine::EngineCore, kisha::engine::EngineError> result =
    kisha::engine::EngineCore::create(create_info);

  REQUIRE_FALSE(result.has_value());
  REQUIRE(result.error() == kisha::engine::EngineError::MissingRequiredExtensions);
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

  const std::expected<vk::raii::Instance, kisha::engine::EngineError> result =
    kisha::engine::util::create_instance(context, application_info, {}, {});

  REQUIRE(result.has_value());
  REQUIRE(static_cast<VkInstance>(**result) != VK_NULL_HANDLE);
}
