//
// Created by jsumihiro on 6/12/26.
//

#ifndef KISHA_ERRORS_HPP
#define KISHA_ERRORS_HPP

namespace kisha::engine {
  enum class EngineInitError : std::uint8_t {
    Unknown = 0U,
    API_VERSION_TOO_LOW,
  };
}
#endif //KISHA_ERRORS_HPP
