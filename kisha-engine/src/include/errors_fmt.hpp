//
// Created by jsumihiro on 6/13/26.
//

#ifndef KISHA_ERRORS_FMT_HPP
#define KISHA_ERRORS_FMT_HPP

#include <fmt/format.h>

#include "errors.hpp"

template <>
struct fmt::formatter<kisha::engine::MissingNamesError> : fmt::formatter<std::string> {
  auto format(const kisha::engine::MissingNamesError &error, format_context &ctx) const {
    return fmt::formatter<std::string>::format(error.describe(), ctx);
  }
};

template <>
struct fmt::formatter<kisha::engine::DeviceRejection> : fmt::formatter<std::string> {
  auto format(const kisha::engine::DeviceRejection &rejection, format_context &ctx) const {
    std::string out;
    rejection.describe(out);
    return fmt::formatter<std::string>::format(out, ctx);
  }
};

template <>
struct fmt::formatter<kisha::engine::NoSuitableDeviceError> : fmt::formatter<std::string> {
  auto format(const kisha::engine::NoSuitableDeviceError &error, format_context &ctx) const {
    return fmt::formatter<std::string>::format(error.describe(), ctx);
  }
};

#endif //KISHA_ERRORS_FMT_HPP
