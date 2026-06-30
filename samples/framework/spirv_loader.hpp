#ifndef KISHA_SAMPLES_SPIRV_LOADER_HPP
#define KISHA_SAMPLES_SPIRV_LOADER_HPP

#include <cstdint>
#include <expected>
#include <filesystem>
#include <vector>

#include "sample_error.hpp"

namespace kisha::samples {
  [[nodiscard]] std::expected<std::vector<std::uint32_t>, SampleError>
  load_spirv(const std::filesystem::path &path);
}

#endif //KISHA_SAMPLES_SPIRV_LOADER_HPP
