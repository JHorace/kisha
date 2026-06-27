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
      util::append_unique(&spec.required_extensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
      util::append_unique(&spec.required_extensions, VK_KHR_SWAPCHAIN_MAINTENANCE_1_EXTENSION_NAME);
      spec.require_discrete_gpu = true;
      return spec;
    }

    std::expected<vk::raii::DebugUtilsMessengerEXT, EngineError> create_debug_messenger(const vk::raii::Instance &instance,
                                                                                            const bool enable_validation) {
      if (!enable_validation) {
        return vk::raii::DebugUtilsMessengerEXT{nullptr};
      }
      const vk::DebugUtilsMessengerCreateInfoEXT debug_create_info =
          vk::DebugUtilsMessengerCreateInfoEXT{}
          .setMessageSeverity(vk::DebugUtilsMessageSeverityFlagBitsEXT::eError | vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
          .setMessageType(vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                          vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance)
          .setPfnUserCallback(vk::PFN_DebugUtilsMessengerCallbackEXT(&util::vulkan_debug_callback));
      return instance.createDebugUtilsMessengerEXT(debug_create_info)
          .transform_error([](const vk::Result result) {
            spdlog::error("Failed to create debug utils messenger: {}", vk::to_string(result));
            return EngineError::InstanceCreationFailed;
          });
    }

    struct DeviceBundle {
      vk::raii::Device device{nullptr};
      Queues queues{};
      EngineProfile profile{};
    };

    EngineProfile build_profile(const vk::PhysicalDeviceProperties &properties, const DeviceSelection &selection) {
      return EngineProfile{
        .device_name = std::string(properties.deviceName),
        .device_type = properties.deviceType,
        .vendor_id = properties.vendorID,
        .device_id = properties.deviceID,
        .api_version = properties.apiVersion,
        .enabled_extensions = selection.enabled_extensions,
        .missing_optional_extensions = selection.missing_optional_extensions,
      };
    }

    std::expected<DeviceBundle, EngineError> build_device_bundle(const vk::raii::PhysicalDevice &physical_device,
                                                                     const DeviceSelection &selection) {
      EngineProfile profile = build_profile(physical_device.getProperties(), selection);
      return util::create_logical_device(physical_device, selection.queues, selection.enabled_extensions)
          .transform([&](vk::raii::Device device) {
            Queues queues = util::acquire_queues(device, selection.queues);
            return DeviceBundle{std::move(device), std::move(queues), std::move(profile)};
          });
    }
  }

  EngineCore::EngineCore(vk::raii::Context &&context, vk::raii::Instance &&instance,
                         vk::raii::DebugUtilsMessengerEXT &&debug_messenger, vk::raii::PhysicalDevices &&physical_devices,
                         std::vector<DeviceSelection> &&device_candidates, const size_t active_candidate_index,
                         vk::raii::Device &&device, FrameRing&& frame_ring, Queues &&queues, EngineProfile &&profile)
      : _context(std::move(context)),
        _instance(std::move(instance)),
        _debug_messenger(std::move(debug_messenger)),
        _physical_devices(std::move(physical_devices)),
        _device_candidates(std::move(device_candidates)),
        _active_candidate_index(active_candidate_index),
        _device(std::move(device)),
        _frame_ring(std::move(frame_ring)),
        _queues(std::move(queues)),
        _profile(std::move(profile)) {}

  std::expected<EngineCore, EngineError> EngineCore::create(const EngineCreateInfo &create_info) {
    return EngineInstance::create(create_info)
        .and_then([](EngineInstance engine_instance) { return std::move(engine_instance).create_engine_core(); });
  }

  EngineInstance::EngineInstance(vk::raii::Context &&context, vk::raii::Instance &&instance,
                                 vk::raii::DebugUtilsMessengerEXT &&debug_messenger,
                                 vk::raii::PhysicalDevices &&physical_devices,
                                 std::vector<DeviceSelection> &&device_candidates)
      : context_(std::move(context)),
        instance_(std::move(instance)),
        debug_messenger_(std::move(debug_messenger)),
        physical_devices_(std::move(physical_devices)),
        device_candidates_(std::move(device_candidates)) {}

  std::expected<EngineInstance, EngineError> EngineInstance::create(const EngineCreateInfo &create_info) {
    const InstanceSpec instance_spec = util::reconcile(engine_instance_baseline(create_info), create_info.instance_spec);
    const DeviceSpec device_spec = util::reconcile(engine_device_baseline(), create_info.device_spec);

    if (instance_spec.min_api_version < VK_API_VERSION_1_3) {
      return std::unexpected(EngineError::ApiVersionTooLow);
    }

    vk::raii::Context context;

    const vk::ApplicationInfo application_info = vk::ApplicationInfo{}
        .setPApplicationName(create_info.application_name.c_str())
        .setApplicationVersion(create_info.application_version)
        .setPEngineName("kisha-engine")
        .setEngineVersion(create_info.engine_version)
        .setApiVersion(instance_spec.min_api_version);

    return util::create_instance(context, application_info, instance_spec.required_layers, instance_spec.required_extensions)
        .and_then([&](vk::raii::Instance instance) -> std::expected<EngineInstance, EngineError> {
          return create_debug_messenger(instance, create_info.enable_validation)
              .and_then([&](vk::raii::DebugUtilsMessengerEXT debug_messenger) -> std::expected<EngineInstance, EngineError> {
                std::expected<vk::raii::PhysicalDevices, vk::Result> physical_devices_result = instance.enumeratePhysicalDevices();
                if (!physical_devices_result) {
                  spdlog::error("Failed to enumerate physical devices: {}", vk::to_string(physical_devices_result.error()));
                  return std::unexpected(EngineError::NoSuitableDevice);
                }
                vk::raii::PhysicalDevices physical_devices = std::move(*physical_devices_result);

                std::expected<std::vector<DeviceSelection>, NoSuitableDeviceError> candidates =
                    util::rank_physical_devices(physical_devices, device_spec);
                if (!candidates) {
                  log_error(candidates.error());
                  return std::unexpected(EngineError::NoSuitableDevice);
                }

                const vk::PhysicalDeviceProperties preferred_properties =
                    physical_devices[candidates->front().index].getProperties();
                spdlog::info("Selected {} candidate device(s); preferred: '{}'", candidates->size(),
                             std::string(preferred_properties.deviceName));

                return EngineInstance(std::move(context), std::move(instance), std::move(debug_messenger),
                                      std::move(physical_devices), std::move(*candidates));
              });
        });
  }

  std::expected<EngineCore, EngineError> EngineInstance::create_engine_core() && {
    if (device_candidates_.empty()) {
      spdlog::error("Cannot create a logical device: no suitable physical-device candidates");
      return std::unexpected(EngineError::NoSuitableDevice);
    }

    constexpr std::size_t active_candidate_index = 0U;
    const DeviceSelection &selection = device_candidates_[active_candidate_index];
    const vk::raii::PhysicalDevice &physical_device = physical_devices_[selection.index];

    const vk::PhysicalDeviceProperties properties = physical_device.getProperties();
    spdlog::info("Creating logical device on '{}' (graphics family {}, present family {})",
                 std::string(properties.deviceName), selection.queues.indices.graphics, selection.queues.indices.present);

    return build_device_bundle(physical_device, selection)
        .and_then([&](DeviceBundle bundle) -> std::expected<EngineCore, EngineError>{
          return FrameRing::create(bundle.device, FrameRing::FRAMES_IN_FLIGHT)
              .transform([&, bundle = std::move(bundle)](FrameRing ring) mutable {
                return EngineCore(std::move(context_), std::move(instance_), std::move(debug_messenger_),
                                  std::move(physical_devices_), std::move(device_candidates_), active_candidate_index,
                                  std::move(bundle.device), std::move(ring), std::move(bundle.queues), std::move(bundle.profile));
              });
        });
  }

  void EngineCore::begin_frame() {
    auto maybe_frame = _frame_ring.begin_frame(_device);

    if (!maybe_frame) {

    }

    (void)_presenter.value().acquire_next_image(_device, maybe_frame.value().frame_slot);
  }

  std::expected<void, EngineError> EngineCore::reselect_device_for_surface(const vk::raii::SurfaceKHR &surface) {
    for (std::size_t index = 0U; index < _device_candidates.size(); ++index) {
      if (index == _active_candidate_index) {
        continue;
      }
      const DeviceSelection &candidate = _device_candidates[index];
      const std::expected<vk::Bool32, vk::Result> supported =
          _physical_devices[candidate.index].getSurfaceSupportKHR(candidate.queues.indices.present, *surface);
      if (!supported.has_value() || *supported != VK_TRUE) {
        continue;
      }

      std::expected<DeviceBundle, EngineError> bundle =
          build_device_bundle(_physical_devices[candidate.index], candidate);
      if (!bundle) {
        return std::unexpected(bundle.error());
      }

      spdlog::warn("Active device cannot present to the surface; switching to '{}'", bundle->profile.device_name);
      _queues = std::move(bundle->queues);
      _profile = std::move(bundle->profile);
      _device = std::move(bundle->device);
      _active_candidate_index = index;
      return {};
    }

    spdlog::error("No physical-device candidate can present to the requested surface");
    return std::unexpected(EngineError::NoSurfaceCapableDevice);
  }

  std::expected<Presenter *, EngineError> EngineCore::create_presenter(const NativeWindowHandle &window_handle) {
    std::expected<vk::raii::SurfaceKHR, EngineError> surface_result = util::create_surface(_instance, window_handle);
    if (!surface_result) {
      return std::unexpected(surface_result.error());
    }
    vk::raii::SurfaceKHR surface = std::move(*surface_result);

    // confirm the active device's present family can actually present to this concrete surface.
    const DeviceSelection &active = _device_candidates[_active_candidate_index];
    const std::expected<vk::Bool32, vk::Result> supported =
        _physical_devices[active.index].getSurfaceSupportKHR(active.queues.indices.present, *surface);
    const bool active_supports = supported.has_value() && (*supported == VK_TRUE);

    if (!active_supports) {
      // Rare: the preferred device cannot present to this surface. Switch to a ranked candidate that can before binding it.
      std::expected<void, EngineError> reselected = reselect_device_for_surface(surface);
      if (!reselected) {
        return std::unexpected(reselected.error());
      }
    }

    spdlog::info("Created presentation surface (presenting device '{}')", _profile.device_name);
    _presenter.emplace(Presenter(std::move(surface), physical_device(),
                                 _device_candidates[_active_candidate_index].queues.indices.present));
    return &*_presenter;
  }
}
