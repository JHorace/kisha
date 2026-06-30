//
// Created by jsumihiro on 6/12/26.
//

#ifndef KISHA_ENGINE_INIT_HELPERS_HPP
#define KISHA_ENGINE_INIT_HELPERS_HPP

#include "engine.hpp"

namespace kisha::engine::util {

  [[nodiscard]] InstanceSpec reconcile(const InstanceSpec &engine, const InstanceSpec &app);
  [[nodiscard]] DeviceSpec reconcile(const DeviceSpec &engine, const DeviceSpec &app);

  void append_unique(std::vector<std::string> *names, std::string_view name);
  std::vector<const char *> to_c_string_ptrs(const std::vector<std::string> &names);
  std::vector<std::string> enumerate_instance_extension_names(const vk::raii::Context &context);
  std::vector<std::string> enumerate_instance_layer_names(const vk::raii::Context &context);
  std::vector<std::string> enumerate_device_extension_names(const vk::raii::PhysicalDevice &physical_device);
  [[nodiscard]] std::expected<void, MissingNamesError> validate_required_names(const std::vector<std::string> &available, const std::vector<std::string> &required);
  [[nodiscard]] std::expected<vk::raii::Instance, EngineError> create_instance(const vk::raii::Context &context,
                                                                                   const vk::ApplicationInfo &application_info,
                                                                                   const std::vector<std::string> &required_layers,
                                                                                   const std::vector<std::string> &required_extensions);
  [[nodiscard]] std::expected<QueueSelection, EngineError> select_queue_families(const vk::raii::PhysicalDevice &physical_device);
  [[nodiscard]] std::expected<DeviceSelection, NoSuitableDeviceError> select_physical_device(const vk::raii::PhysicalDevices &physical_devices,
                                                                                            const DeviceSpec &device_spec);
  [[nodiscard]] std::expected<std::vector<DeviceSelection>, NoSuitableDeviceError> rank_physical_devices(
      const vk::raii::PhysicalDevices &physical_devices, const DeviceSpec &device_spec);
  [[nodiscard]] std::vector<vk::DeviceQueueCreateInfo> build_queue_create_infos(const QueueSelection &queues);
  [[nodiscard]] std::expected<vk::raii::Device, EngineError> create_logical_device(const vk::raii::PhysicalDevice &physical_device,
                                                                                       const QueueSelection &queues,
                                                                                       const std::vector<std::string> &enabled_extensions);
  [[nodiscard]] Queues acquire_queues(const vk::raii::Device &device, const QueueSelection &queues);
  [[nodiscard]] std::expected<vk::raii::SurfaceKHR, EngineError> create_surface(const vk::raii::Instance &instance,
                                                                                   const NativeWindowHandle &window_handle);
  [[nodiscard]] std::expected<SurfaceCapabilities, EngineError> query_surface_capabilities(const vk::raii::PhysicalDevice &physical_device,
                                                                                               const vk::raii::SurfaceKHR &surface);
  VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT message_type,
                                                       const VkDebugUtilsMessengerCallbackDataEXT *callback_data, void *user_data);
}

#endif //KISHA_ENGINE_INIT_HELPERS_HPP
