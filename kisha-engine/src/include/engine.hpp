/**
 * @file engine.hpp
 * @brief Core Vulkan engine initialization interfaces.
 */

#ifndef KISHA_ENGINE_HPP
#define KISHA_ENGINE_HPP

#include <vulkan/vulkan_raii.hpp>
#include <expected>
#include <variant>

#include "errors.hpp"
#include "presenter.hpp"

namespace kisha::engine {
  /**
   * @brief Queue-family indices chosen for the device.
   */
  struct QueueFamilyIndices {
    std::uint32_t graphics = 0;
    std::uint32_t present = 0;
    std::optional<std::uint32_t> async_compute;
    std::optional<std::uint32_t> transfer;
  };

  /**
   * @brief device queues acquired after device creation.
   */
  struct Queues {
    vk::raii::Queue graphics{nullptr};
    std::optional<vk::raii::Queue> async_compute;
    std::optional<vk::raii::Queue> transfer;
  };

  /**
   * @brief Information about selected queue families and queue topology
   */
  struct QueueSelection {
    QueueFamilyIndices indices;
    bool has_dedicated_async_compute = false;
    bool has_dedicated_transfer = false;
  };

  /**
   * @brief Selected physical-device including negotiated profile.
   */
  struct DeviceSelection {
    std::size_t index = 0U;
    QueueSelection queues{};
    std::vector<std::string> enabled_extensions;
    std::vector<std::string> missing_optional_extensions;
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

    [[nodiscard]] const vk::raii::Device &device() const { return _device; }
    [[nodiscard]] const vk::raii::PhysicalDevice &physical_device() const {
      return _physical_devices[_device_candidates[_active_candidate_index].index];
    }
    [[nodiscard]] const QueueFamilyIndices &queue_family_indices() const {
      return _device_candidates[_active_candidate_index].queues.indices;
    }
    [[nodiscard]] const std::vector<DeviceSelection> &device_candidates() const { return _device_candidates; }
    [[nodiscard]] const EngineProfile &profile() const { return _profile; }

    [[nodiscard]] std::expected<Presenter, EngineInitError> create_presenter(const NativeWindowHandle &window_handle);
  private:
    friend class EngineInstance;

    std::expected<void, EngineInitError> reselect_device_for_surface(const vk::raii::SurfaceKHR &surface);

    EngineCore(vk::raii::Context &&context, vk::raii::Instance &&instance, vk::raii::DebugUtilsMessengerEXT &&debug_messenger,
               vk::raii::PhysicalDevices &&physical_devices, std::vector<DeviceSelection> &&device_candidates,
               std::size_t active_candidate_index, vk::raii::Device &&device, Queues &&queues, EngineProfile &&profile);

    vk::raii::Context _context;
    vk::raii::Instance _instance{nullptr};
    vk::raii::DebugUtilsMessengerEXT _debug_messenger{nullptr};
    vk::raii::PhysicalDevices _physical_devices{nullptr};
    std::vector<DeviceSelection> _device_candidates;
    std::size_t _active_candidate_index = 0U;
    vk::raii::Device _device{nullptr};
    Queues _queues{};
    EngineProfile _profile;
  };

  /**
   * @brief EngineInstance is an EngineCore without a device. It enumerates and exposes all the details needed to
   *        select and create a device. On
   */
  class EngineInstance {
  public:
    static std::expected<EngineInstance, EngineInitError> create(const EngineCreateInfo &create_info = {});

    //RAII type, so can only be move assigned/constructed
    EngineInstance(EngineInstance &&other) noexcept = default;
    EngineInstance &operator=(EngineInstance &&other) noexcept = default;

    EngineInstance(const EngineInstance &) = delete;
    EngineInstance &operator=(const EngineInstance &) = delete;

    [[nodiscard]] const vk::raii::Instance &instance() const { return instance_; }
    [[nodiscard]] const vk::raii::PhysicalDevices &physical_devices() const { return physical_devices_; }
    [[nodiscard]] const std::vector<DeviceSelection> &device_candidates() const { return device_candidates_; }

    [[nodiscard]] std::expected<EngineCore, EngineInitError> create_engine_core() &&;
  private:
    EngineInstance(vk::raii::Context &&context, vk::raii::Instance &&instance,
                   vk::raii::DebugUtilsMessengerEXT &&debug_messenger, vk::raii::PhysicalDevices &&physical_devices,
                   std::vector<DeviceSelection> &&device_candidates);

    vk::raii::Context context_;
    vk::raii::Instance instance_{nullptr};
    vk::raii::DebugUtilsMessengerEXT debug_messenger_{nullptr};
    vk::raii::PhysicalDevices physical_devices_{nullptr};
    std::vector<DeviceSelection> device_candidates_;
  };
}

#endif //KISHA_ENGINE_HPP
