/**
 * @file engine.hpp
 * @brief Core Vulkan engine initialization interfaces.
 */

#ifndef KISHA_ENGINE_HPP
#define KISHA_ENGINE_HPP

#include <vulkan/vulkan_raii.hpp>
#include <expected>

#include "errors.hpp"

namespace kisha::engine {
  /**
   * @brief Queue-family indices chosen for the device.
   */
  struct QueueFamilyIndices {
    std::uint32_t graphics = 0;
    std::optional<std::uint32_t> async_compute;
    std::optional<std::uint32_t> transfer;
  };

  /**
   * @brief Requirements either the app or the engine imposes on instance creation.
   * The engine will reconcile its internal requirements with the app's.
   */
  struct InstanceSpec {
    std::vector<std::string> required_extensions;
    std::vector<std::string> optional_extensions;
    std::vector<std::string> required_layers;
    std::vector<std::string> optional_layers;
    std::uint32_t min_api_version = VK_API_VERSION_1_0;
  };

  /**
   * @brief Requirements either the app or the engine imposes on device creation.
   * The engine will reconcile its internal requirements with the app's.
   */
  struct DeviceSpec {
    std::vector<std::string> required_extensions;
    std::vector<std::string> optional_extensions;
    bool require_discrete_gpu = true;
    bool require_async_compute = false;
    bool require_dedicated_transfer = false;
  };
  /**
   * @brief Actual configuration of the engine after initialization.
   */
  struct EngineProfile {
    std::string device_name;
    vk::PhysicalDeviceType device_type = vk::PhysicalDeviceType::eOther;
    std::uint32_t vendor_id = 0U;
    std::uint32_t device_id = 0U;
    std::uint32_t api_version = VK_API_VERSION_1_0;
    std::vector<std::string> enabled_extensions;
    std::vector<std::string> missing_optional_extensions;
  };


  struct EngineCreateInfo {
    std::string application_name;
    uint32_t application_version = VK_MAKE_API_VERSION(0, 0, 1, 0);
    uint32_t engine_version = VK_MAKE_API_VERSION(0, 0, 1, 0);
    bool enable_validation = false;
    InstanceSpec instance_spec;
    DeviceSpec device_spec;
  };

  /**
   * @brief Fully initialized Vulkan core context using `vk::raii` ownership.
   */
  class EngineCore {
  public:
    static std::expected<EngineCore, EngineInitError> create(const EngineCreateInfo& create_info = {});

    //RAII type, so can only be move assigned/constructed
    EngineCore(EngineCore &&other) noexcept = default;
    EngineCore &operator=(EngineCore &&other) noexcept = default;

    EngineCore(const EngineCore &) = delete;
    EngineCore &operator=(const EngineCore &) = delete;
  private:

    EngineCore(vk::raii::Context &&context, vk::raii::Instance &&instance, vk::raii::DebugUtilsMessengerEXT &&debug_messenger,
               vk::raii::PhysicalDevice &&physical_device, vk::raii::Device &&device);

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debug_messenger_{nullptr};
    vk::raii::PhysicalDevice physical_device_{nullptr};
    vk::raii::Device device_{nullptr};
  };
}

#endif //KISHA_ENGINE_HPP
