/**
 * @file engine.cpp
 * @brief Vulkan core initialization implementation using `vk::raii`.
 */
#include "engine.hpp"
#include "engine_init_helpers.hpp"

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
      spec.require_discrete_gpu = true;
      return spec;
    }
  }

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
            debug_messenger = vk::raii::DebugUtilsMessengerEXT(instance, debug_create_info);
          }

          const std::vector<std::string> &required_device_extensions = device_spec.required_extensions;
          (void)required_device_extensions;

          return std::unexpected(EngineInitError::NotImplemented);
        });
  }
}
