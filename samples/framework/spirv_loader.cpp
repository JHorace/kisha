#include "spirv_loader.hpp"

#include <cstring>
#include <fstream>
#include <system_error>

namespace kisha::samples {
  namespace {
    constexpr std::uint32_t kSpirvMagic = 0x07230203U;
  }

  std::expected<std::vector<std::uint32_t>, SampleError>
  load_spirv(const std::filesystem::path &path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || ec) {
      return std::unexpected(SampleError::FileNotFound);
    }

    const std::uintmax_t byte_count = std::filesystem::file_size(path, ec);
    if (ec) {
      return std::unexpected(SampleError::FileReadFailed);
    }

    if (byte_count == 0U || (byte_count % sizeof(std::uint32_t)) != 0U) {
      return std::unexpected(SampleError::InvalidSpirv);
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
      return std::unexpected(SampleError::FileReadFailed);
    }

    std::vector<std::uint32_t> words(static_cast<std::size_t>(byte_count) / sizeof(std::uint32_t));
    std::vector<char> bytes(static_cast<std::size_t>(byte_count));
    if (!file.read(bytes.data(), static_cast<std::streamsize>(byte_count))) {
      return std::unexpected(SampleError::FileReadFailed);
    }
    std::memcpy(words.data(), bytes.data(), bytes.size());

    if (words.front() != kSpirvMagic) {
      return std::unexpected(SampleError::InvalidSpirv);
    }

    return words;
  }
}
