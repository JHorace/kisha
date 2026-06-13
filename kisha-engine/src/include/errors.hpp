//
// Created by jsumihiro on 6/12/26.
//

#ifndef KISHA_ERRORS_HPP
#define KISHA_ERRORS_HPP

namespace kisha::engine {
  enum class EngineInitError : uint8_t {
    Unknown = 0U,
    NotImplemented,
    ApiVersionTooLow,
    MissingRequiredLayers,
    MissingRequiredExtensions,
    NoSuitableQueueFamily,
    NoSuitableDevice,
  };

  struct MissingNamesError {
    std::vector<std::string> missing_names;
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
  };

  /**
   * @brief Error returned when no physical device satisfies the device spec.
   *        Carries diagnostics about every candidate that was rejected.
   */
  struct NoSuitableDeviceError {
    std::vector<DeviceRejection> candidates;
  };
}
#endif //KISHA_ERRORS_HPP
