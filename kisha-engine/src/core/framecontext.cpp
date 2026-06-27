#include "framecontext.hpp"

#include <spdlog/spdlog.h>

namespace kisha::engine {
  std::expected<FrameContext, EngineInitError> FrameContext::create(const vk::raii::Device &device,
                                                                    const std::uint32_t image_count) {
    std::vector<vk::raii::Semaphore> image_available;
    image_available.reserve(MAX_FRAMES_IN_FLIGHT);
    std::vector<vk::raii::Fence> in_flight;
    in_flight.reserve(MAX_FRAMES_IN_FLIGHT);

    for (std::uint32_t frame = 0U; frame < MAX_FRAMES_IN_FLIGHT; ++frame) {
      std::expected<vk::raii::Semaphore, vk::Result> semaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
      if (!semaphore) {
        spdlog::error("Failed to create image-available semaphore: {}", vk::to_string(semaphore.error()));
        return std::unexpected(EngineInitError::FrameSyncCreationFailed);
      }
      image_available.push_back(std::move(*semaphore));

      // Created signaled so the very first wait on a frame slot does not block.
      const vk::FenceCreateInfo fence_info = vk::FenceCreateInfo{}.setFlags(vk::FenceCreateFlagBits::eSignaled);
      std::expected<vk::raii::Fence, vk::Result> fence = device.createFence(fence_info);
      if (!fence) {
        spdlog::error("Failed to create in-flight fence: {}", vk::to_string(fence.error()));
        return std::unexpected(EngineInitError::FrameSyncCreationFailed);
      }
      in_flight.push_back(std::move(*fence));
    }

    FrameContext result;
    result._image_available = std::move(image_available);
    result._in_flight = std::move(in_flight);
    result._image_count = image_count;
    return result;
  }
}
