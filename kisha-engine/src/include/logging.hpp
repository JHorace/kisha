//
// Created by jsumihiro on 6/13/26.
//

#ifndef KISHA_LOGGING_HPP
#define KISHA_LOGGING_HPP

#include <spdlog/spdlog.h>

#include "errors.hpp"

/**
 * @file logging.hpp
 * @brief Generic log-emission helpers for `Describable` error types.
 *
 * The error types own their textual representation (`describe()`); these
 * helpers own the emission policy (which logger / severity). This keeps a
 * single uniform entry point instead of a bespoke free function per error type.
 */

namespace kisha::engine {
  /**
   * @brief Emits the description of a `Describable` error at error severity.
   */
  void log_error(const Describable auto &error) {
    spdlog::error("{}", error.describe());
  }
}

#endif //KISHA_LOGGING_HPP
