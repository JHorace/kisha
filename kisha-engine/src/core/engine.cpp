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

      util::append_unique(&spec.required_extensions, VK_KHR_SURFACE_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_KHR_GET_SURFACE_CAPABILITIES_2_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_KHR_SURFACE_MAINTENANCE_1_EXTENSION_NAME);

      // Per-platform surface extensions, needed to create surfaces from native handles.
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
      util::append_unique(&spec.required_extensions, VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
      util::append_unique(&spec.required_extensions, VK_KHR_XCB_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
      util::append_unique(&spec.required_extensions, VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
      util::append_unique(&spec.required_extensions, VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif

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
                         vk::raii::Device &&device, Queues &&queues, EngineProfile &&profile)
      : _context(std::move(context)),
        _instance(std::move(instance)),
        _debug_messenger(std::move(debug_messenger)),
        _physical_device(std::move(physical_device)),
        _device(std::move(device)),
        _queues(std::move(queues)),
        _profile(std::move(profile)) {}

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
              util::select_physical_device(physical_devices, device_spec);
          if (!device_selection) {
            log_error(device_selection.error());
            return std::unexpected(EngineInitError::NoSuitableDevice);
          }

          vk::raii::PhysicalDevice physical_device = std::move(physical_devices[device_selection->index]);

          const vk::PhysicalDeviceProperties device_properties = physical_device.getProperties();
          EngineProfile profile{
            .device_name = std::string(device_properties.deviceName),
            .device_type = device_properties.deviceType,
            .vendor_id = device_properties.vendorID,
            .device_id = device_properties.deviceID,
            .api_version = device_properties.apiVersion,
            .enabled_extensions = device_selection->enabled_extensions,
            .missing_optional_extensions = device_selection->missing_optional_extensions,
          };

          return util::create_logical_device(physical_device, device_selection->queues, device_selection->enabled_extensions)
              .transform([&](vk::raii::Device device) {
                Queues queues = util::acquire_queues(device, device_selection->queues);
                return EngineCore(std::move(context), std::move(instance), std::move(debug_messenger),
                                  std::move(physical_device), std::move(device), std::move(queues), std::move(profile));
              });
        });
  }
}
