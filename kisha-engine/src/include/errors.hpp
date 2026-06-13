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
}
#endif //KISHA_ERRORS_HPP
