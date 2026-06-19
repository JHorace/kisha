#include <cstring>
#include <string>
#include <vector>
#include <catch2/catch_test_macros.hpp>

#include "engine.hpp"
#include "engine_init_helpers.hpp"
#include "errors.hpp"

// These tests focus on the pure, CPU-only logic that backs EngineCore::create:
// name de-duplication, required-name validation, queue-family de-duplication and
// the human-readable error `describe()` helpers. None of them touch a real
// Vulkan device, so they are safe to run in headless/driverless environments.

namespace util = kisha::engine::util;

TEST_CASE("append_unique appends new names and ignores duplicates", "[core]") {
  std::vector<std::string> names;

  util::append_unique(&names, "a");
  util::append_unique(&names, "b");
  // "a" already present: must not be appended again.
  util::append_unique(&names, "a");

  REQUIRE(names == std::vector<std::string>{"a", "b"});
}

TEST_CASE("append_unique preserves first-seen insertion order", "[core]") {
  std::vector<std::string> names;

  util::append_unique(&names, "z");
  util::append_unique(&names, "y");
  util::append_unique(&names, "x");
  util::append_unique(&names, "y"); // duplicate, ignored

  REQUIRE(names == std::vector<std::string>{"z", "y", "x"});
}

TEST_CASE("to_c_string_ptrs mirrors the input strings", "[core]") {
  const std::vector<std::string> names{"alpha", "beta", "gamma"};

  const std::vector<const char *> ptrs = util::to_c_string_ptrs(names);

  REQUIRE(ptrs.size() == names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    REQUIRE(ptrs[i] != nullptr);
    // The returned pointers must reference the same characters as the source.
    REQUIRE(std::strcmp(ptrs[i], names[i].c_str()) == 0);
  }
}

TEST_CASE("to_c_string_ptrs on an empty vector yields an empty vector", "[core]") {
  const std::vector<std::string> names;

  const std::vector<const char *> ptrs = util::to_c_string_ptrs(names);

  REQUIRE(ptrs.empty());
}

TEST_CASE("validate_required_names succeeds when all required are available", "[core]") {
  const std::vector<std::string> available{"a", "b", "c"};
  const std::vector<std::string> required{"a", "c"};

  const std::expected<void, kisha::engine::MissingNamesError> result =
    util::validate_required_names(available, required);

  REQUIRE(result.has_value());
}

TEST_CASE("validate_required_names with no requirements always succeeds", "[core]") {
  const std::vector<std::string> available{"a"};
  const std::vector<std::string> required;

  REQUIRE(util::validate_required_names(available, required).has_value());
  // Even an empty availability list is fine when nothing is required.
  REQUIRE(util::validate_required_names({}, {}).has_value());
}

TEST_CASE("validate_required_names reports the missing names", "[core]") {
  const std::vector<std::string> available{"a", "b"};
  const std::vector<std::string> required{"a", "x", "y"};

  const std::expected<void, kisha::engine::MissingNamesError> result =
    util::validate_required_names(available, required);

  REQUIRE_FALSE(result.has_value());
  // Only the genuinely-absent names should be reported, in requested order.
  REQUIRE(result.error().missing_names == std::vector<std::string>{"x", "y"});
}

TEST_CASE("build_queue_create_infos collapses shared queue families", "[core]") {
  // All three roles resolve to the same family (the common single-queue GPU case):
  // exactly one DeviceQueueCreateInfo should be produced.
  kisha::engine::QueueSelection queues{};
  queues.indices.graphics = 0U;
  queues.indices.async_compute = 0U;
  queues.indices.transfer = 0U;

  const std::vector<vk::DeviceQueueCreateInfo> infos = util::build_queue_create_infos(queues);

  REQUIRE(infos.size() == 1U);
  REQUIRE(infos[0].queueFamilyIndex == 0U);
  REQUIRE(infos[0].queueCount == 1U);
  REQUIRE(infos[0].pQueuePriorities != nullptr);
  REQUIRE(infos[0].pQueuePriorities[0] == 1.0F);
}

TEST_CASE("build_queue_create_infos produces one entry per distinct family", "[core]") {
  // Distinct graphics/compute/transfer families: three unique infos, order preserved.
  kisha::engine::QueueSelection queues{};
  queues.indices.graphics = 0U;
  queues.indices.async_compute = 1U;
  queues.indices.transfer = 2U;

  const std::vector<vk::DeviceQueueCreateInfo> infos = util::build_queue_create_infos(queues);

  REQUIRE(infos.size() == 3U);
  REQUIRE(infos[0].queueFamilyIndex == 0U);
  REQUIRE(infos[1].queueFamilyIndex == 1U);
  REQUIRE(infos[2].queueFamilyIndex == 2U);
}

TEST_CASE("build_queue_create_infos de-duplicates partially shared families", "[core]") {
  // Compute shares the graphics family; transfer is dedicated.
  kisha::engine::QueueSelection queues{};
  queues.indices.graphics = 0U;
  queues.indices.async_compute = 0U;
  queues.indices.transfer = 1U;

  const std::vector<vk::DeviceQueueCreateInfo> infos = util::build_queue_create_infos(queues);

  REQUIRE(infos.size() == 2U);
  REQUIRE(infos[0].queueFamilyIndex == 0U);
  REQUIRE(infos[1].queueFamilyIndex == 1U);
}

TEST_CASE("build_queue_create_infos handles absent optional families", "[core]") {
  // Only a graphics family is known; async_compute/transfer are nullopt.
  kisha::engine::QueueSelection queues{};
  queues.indices.graphics = 3U;

  const std::vector<vk::DeviceQueueCreateInfo> infos = util::build_queue_create_infos(queues);

  REQUIRE(infos.size() == 2U);
  REQUIRE(infos[0].queueFamilyIndex == 3U);
}

TEST_CASE("MissingNamesError::describe lists the missing names", "[core]") {
  const kisha::engine::MissingNamesError error{{"VK_one", "VK_two"}};

  const std::string description = error.describe();

  REQUIRE(description.find("VK_one") != std::string::npos);
  REQUIRE(description.find("VK_two") != std::string::npos);
}

TEST_CASE("DeviceRejection::describe includes name, type and active reasons", "[core]") {
  kisha::engine::DeviceRejection rejection{};
  rejection.device_name = "Test GPU";
  rejection.device_type = "DiscreteGpu";
  rejection.missing_features = {"shaderObject"};
  rejection.missing_required_extensions = {"VK_EXT_thing"};
  rejection.no_suitable_queue_family = true;
  rejection.missing_async_compute = true;
  rejection.missing_dedicated_transfer = true;
  rejection.discrete_gpu_required = true;

  std::string out;
  rejection.describe(out);

  REQUIRE(out.find("Test GPU") != std::string::npos);
  REQUIRE(out.find("DiscreteGpu") != std::string::npos);
  REQUIRE(out.find("shaderObject") != std::string::npos);
  REQUIRE(out.find("VK_EXT_thing") != std::string::npos);
  REQUIRE(out.find("no suitable queue family") != std::string::npos);
  REQUIRE(out.find("async-compute") != std::string::npos);
  REQUIRE(out.find("transfer") != std::string::npos);
  REQUIRE(out.find("discrete GPU is required") != std::string::npos);
}

TEST_CASE("DeviceRejection::describe omits inactive reasons", "[core]") {
  // A rejection with no flags set should mention the device but none of the reasons.
  kisha::engine::DeviceRejection rejection{};
  rejection.device_name = "Quiet GPU";
  rejection.device_type = "IntegratedGpu";

  std::string out;
  rejection.describe(out);

  REQUIRE(out.find("Quiet GPU") != std::string::npos);
  REQUIRE(out.find("missing required features") == std::string::npos);
  REQUIRE(out.find("no suitable queue family") == std::string::npos);
  REQUIRE(out.find("discrete GPU is required") == std::string::npos);
}

TEST_CASE("NoSuitableDeviceError::describe summarizes every candidate", "[core]") {
  kisha::engine::DeviceRejection first{};
  first.device_name = "GPU-A";
  first.device_type = "DiscreteGpu";
  first.missing_required_extensions = {"VK_EXT_a"};

  kisha::engine::DeviceRejection second{};
  second.device_name = "GPU-B";
  second.device_type = "IntegratedGpu";
  second.discrete_gpu_required = true;

  const kisha::engine::NoSuitableDeviceError error{{first, second}};

  const std::string description = error.describe();

  // The summary reports the candidate count and details for each device.
  REQUIRE(description.find("2 candidate(s)") != std::string::npos);
  REQUIRE(description.find("GPU-A") != std::string::npos);
  REQUIRE(description.find("GPU-B") != std::string::npos);
  REQUIRE(description.find("VK_EXT_a") != std::string::npos);
  REQUIRE(description.find("discrete GPU is required") != std::string::npos);
}
