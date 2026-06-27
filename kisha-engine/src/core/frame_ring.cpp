#include "frame_ring.hpp"

#include <spdlog/spdlog.h>

#include "presenter.hpp"

namespace kisha::engine {
  std::expected<FrameRing, EngineError> FrameRing::create(const vk::raii::Device &device,
                                                                    const std::uint32_t frame_count) {
    std::vector<vk::raii::Semaphore> image_available;
    image_available.reserve(FRAMES_IN_FLIGHT);
    std::vector<vk::raii::Fence> in_flight;
    in_flight.reserve(FRAMES_IN_FLIGHT);

    vk::SemaphoreTypeCreateInfo timeline_semaphore_info = vk::SemaphoreTypeCreateInfo{
        vk::SemaphoreType::eTimeline, 0};
    vk::SemaphoreCreateInfo semaphore_create_info = {};
    semaphore_create_info.setPNext(&timeline_semaphore_info);
    std::expected<vk::raii::Semaphore, vk::Result> semaphore = device.createSemaphore(semaphore_create_info);

    if (!semaphore) {
      //TODO:
      return std::unexpected(EngineError::Unknown);
    }

    return FrameRing(std::move(*semaphore));
  }

  std::expected<FrameResources, EngineError> FrameRing::begin_frame(const vk::raii::Device &device) {
    uint32_t slot = _frame_counter % FRAMES_IN_FLIGHT;

    // Timeline semaphores let us signal/wait on a particular value - this simplifies synchronization a lot as we can essentially wait on a specific frame to finish rendering.
    if (auto result = device.waitSemaphores(vk::SemaphoreWaitInfo{vk::SemaphoreWaitFlagBits::eAny, *_frame_timeline, _frame_slot[slot]},  std::numeric_limits<std::uint64_t>::max());
      result != vk::Result::eSuccess) {
      spdlog::error("Failed to wait on timeline semaphore: {}", vk::to_string(result));
      return std::unexpected(EngineError::Unknown);
    }

    return FrameResources{
      .frame_slot = slot
    };
  }

  FrameRing::FrameRing(vk::raii::Semaphore &&frame_timeline) : _frame_timeline(std::move(frame_timeline)) {

  }
}
