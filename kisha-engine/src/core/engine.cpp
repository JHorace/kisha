/**
 * @file engine.cpp
 * @brief Vulkan core initialization implementation using `vk::raii`.
 */
#include "engine.hpp"
#include "engine_init_helpers.hpp"
#include "logging.hpp"

#include <spdlog/spdlog.h>
#include <spdlog/fmt/ranges.h>

namespace kisha::engine {
  namespace {
    /**
     * @brief The engine's own instance requirements, reconciled with the app's.
     * Validation layer / debug-utils are conditional on `enable_validation`.
     */
    InstanceSpec engine_instance_baseline(const EngineCreateInfo &create_info) {
      InstanceSpec spec{};
      spec.min_api_version = VK_API_VERSION_1_3;
      if (create_info.enable_validation) {
        util::append_unique(&spec.required_layers, "VK_LAYER_KHRONOS_validation");
        util::append_unique(&spec.required_extensions, VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
      }
      return spec;
    }

    /**
     * @brief The engine's own device requirements, reconciled with the app's.
     */
    DeviceSpec engine_device_baseline() {
      DeviceSpec spec{};
      util::append_unique(&spec.required_extensions, VK_EXT_SHADER_OBJECT_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_KHR_UNIFIED_IMAGE_LAYOUTS_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
      spec.require_discrete_gpu = true;
      return spec;
    }
  }

  EngineCore::EngineCore(vk::raii::Context &&context, vk::raii::Instance &&instance,
                         vk::raii::DebugUtilsMessengerEXT &&debug_messenger, vk::raii::PhysicalDevice &&physical_device,
                         vk::raii::Device &&device)
      : context_(std::move(context)),
        instance_(std::move(instance)),
        debug_messenger_(std::move(debug_messenger)),
        physical_device_(std::move(physical_device)),
        device_(std::move(device)) {}

  std::expected<EngineCore, EngineInitError> EngineCore::create(const EngineCreateInfo &create_info) {
    const InstanceSpec instance_spec = util::reconcile(engine_instance_baseline(create_info), create_info.instance_spec);
    const DeviceSpec device_spec = util::reconcile(engine_device_baseline(), create_info.device_spec);

    if (instance_spec.min_api_version < VK_API_VERSION_1_3) {
      return std::unexpected(EngineInitError::ApiVersionTooLow);
    }

    vk::raii::Context context;

    const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
        .setPApplicationName(create_info.application_name.c_str())
        .setApplicationVersion(create_info.application_version)
        .setPEngineName("kisha-engine")
        .setEngineVersion(create_info.engine_version)
        .setApiVersion(instance_spec.min_api_version);

    return util::create_instance(context, application_info, instance_spec.required_layers, instance_spec.required_extensions)
        .and_then([&](vk::raii::Instance instance) -> std::expected<EngineCore, EngineInitError> {
          vk::raii::DebugUtilsMessengerEXT debug_messenger{nullptr};
          if (create_info.enable_validation) {
            const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info =
                vk::DebugUtilsMessengerCreateInfoEXT{}
                .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
                .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
                .setPfnUserCallback(vk::PFN_DebugUtilsMessengerCallbackEXT(&util::vulkan_debug_callback));
            std::expected<vk::raii::DebugUtilsMessengerEXT, vk::Result> messenger =
                instance.createDebugUtilsMessengerEXT(debug_create_info);
            if (!messenger) {
              spdlog::error("Failed to create debug utils messenger: {}", vk::to_string(messenger.error()));
              return std::unexpected(EngineInitError::InstanceCreationFailed);
            }
            debug_messenger = std::move(*messenger);
          }

          std::expected<vk::raii::PhysicalDevices, vk::Result> physical_devices_result = instance.enumeratePhysicalDevices();
          if (!physical_devices_result) {
            spdlog::error("Failed to enumerate physical devices: {}", vk::to_string(physical_devices_result.error()));
            return std::unexpected(EngineInitError::NoSuitableDevice);
          }
          vk::raii::PhysicalDevices physical_devices = std::move(*physical_devices_result);
          const std::expected<util::DeviceSelection, NoSuitableDeviceError> device_selection =
              util::select_physical_device(instance, physical_devices, device_spec);
          if (!device_selection) {
            log_error(device_selection.error());
            return std::unexpected(EngineInitError::NoSuitableDevice);
          }

          vk::raii::PhysicalDevice physical_device = std::move(physical_devices[device_selection->index]);
          return util::create_logical_device(physical_device, device_selection->queues, device_selection->enabled_extensions)
              .transform([&](vk::raii::Device device) {
                return EngineCore(std::move(context), std::move(instance), std::move(debug_messenger),
                                  std::move(physical_device), std::move(device));
              });
        });
  }
}
