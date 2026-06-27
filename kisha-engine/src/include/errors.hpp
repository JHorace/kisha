//
// Created by jsumihiro on 6/12/26.
//

#ifndef KISHA_ERRORS_HPP
#define KISHA_ERRORS_HPP

#include <concepts>
#include <cstdint>
#include <string>
#include <vector>

namespace kisha::engine {
  /**
   * @brief Concept satisfied by any error (or value) that can render a
   *        human-readable description of itself via a `describe()` member
   *        returning something convertible to `std::string`.
   *
   * This is the "describe" half of the describe/emit split: types own their
   * textual representation, while emission policy (logger, severity, sink)
   * stays at the call site.
   */
  template <typename E>
  concept Describable = requires(const E &error) {
    { error.describe() } -> std::convertible_to<std::string>;
  };

  enum class EngineError : uint8_t {
    Unknown = 0U,
    NotImplemented,
    ApiVersionTooLow,
    MissingRequiredLayers,
    MissingRequiredExtensions,
    NoSuitableQueueFamily,
    NoSuitableDevice,
    InstanceCreationFailed,
    DeviceCreationFailed,
    NoPresentCapableQueue,
    SurfaceCreationFailed,
    SurfaceCapabilityQueryFailed,
    SwapchainCreationFailed,
    NoSurfaceCapableDevice,
    PresentFailed,
    FrameSyncCreationFailed,
    ImageAcquisitionFailed,
  };

  struct MissingNamesError {
    std::vector<std::string> missing_names;
    [[nodiscard]] std::string describe() const;
  };

  /**
   * @brief Describes a single physical device that was considered but rejected,
   *        along with the reasons it failed selection.
   */
  struct DeviceRejection {
    std::string device_name;
    std::string device_type;
    std::vector<std::string> missing_features;
    std::vector<std::string> missing_required_extensions;
    bool no_suitable_queue_family = false;
    bool missing_async_compute = false;
    bool missing_dedicated_transfer = false;
    bool discrete_gpu_required = false;
    void describe(std::string &out) const;
  };

  /**
   * @brief Error returned when no physical device satisfies the device spec.
   *        Carries diagnostics about every candidate that was rejected.
   */
  struct NoSuitableDeviceError {
    std::vector<DeviceRejection> candidates;
    [[nodiscard]] std::string describe() const;
  };
}
#endif //KISHA_ERRORS_HPP
