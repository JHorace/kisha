//
// Created by jsumihiro on 6/13/26.
//

#include "errors.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

namespace kisha::engine {
  std::string MissingNamesError::describe() const {
    return fmt::format("Missing required names: {}", missing_names);
  }

  void DeviceRejection::describe(std::string &out) const {
    fmt::format_to(std::back_inserter(out), "  Device '{}' ({}) rejected:\n", device_name, device_type);
    if (!missing_features.empty()) {
      fmt::format_to(std::back_inserter(out), "    missing required features: {}\n", missing_features);
    }
    if (!missing_required_extensions.empty()) {
      fmt::format_to(std::back_inserter(out), "    missing required extensions: {}\n", missing_required_extensions);
    }
    if (no_suitable_queue_family) {
      out += "    no suitable queue family\n";
    }
    if (missing_async_compute) {
      out += "    no dedicated async-compute queue family\n";
    }
    if (missing_dedicated_transfer) {
      out += "    no dedicated transfer queue family\n";
    }
    if (discrete_gpu_required) {
      out += "    a discrete GPU is required\n";
    }
  }

  std::string NoSuitableDeviceError::describe() const {
    std::string out = fmt::format("Failed to select a suitable physical device; {} candidate(s) considered:\n",
                                  candidates.size());
    for (const DeviceRejection &candidate : candidates) {
      candidate.describe(out);
    }
    return out;
  }
}
