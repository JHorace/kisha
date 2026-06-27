#include "frame_ring.hpp"

#include <spdlog/spdlog.h>

#include "presenter.hpp"

namespace kisha::engine {
  std::expected<FrameRing, EngineError> FrameRing::create(const vk::raii::Device &device,
                                                                    const std::uint32_t frame_count,
                                                                    const std::vector<std::uint32_t> &queue_families) {
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

    // We need one command pool per (thread, frame-in-flight, queue family). Since the engine is
    // single-threaded for now, we have (frames-in-flight x queue families) pools.
    std::vector<std::vector<vk::raii::CommandPool>> command_pools;
    command_pools.reserve(FRAMES_IN_FLIGHT);
    for (std::uint32_t frame = 0U; frame < FRAMES_IN_FLIGHT; ++frame) {
      std::vector<vk::raii::CommandPool> per_family_pools;
      per_family_pools.reserve(queue_families.size());
      for (const std::uint32_t queue_family : queue_families) {
        vk::CommandPoolCreateInfo command_pool_create_info{
            vk::CommandPoolCreateFlagBits::eTransient, queue_family};
        std::expected<vk::raii::CommandPool, vk::Result> command_pool =
            device.createCommandPool(command_pool_create_info);
        if (!command_pool) {
          spdlog::error("Failed to create command pool for queue family {}: {}", queue_family,
                        vk::to_string(command_pool.error()));
          return std::unexpected(EngineError::Unknown);
        }
        per_family_pools.push_back(std::move(*command_pool));
      }
      command_pools.push_back(std::move(per_family_pools));
    }

    return FrameRing(std::move(*semaphore), std::move(command_pools), queue_families);
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

  FrameRing::FrameRing(vk::raii::Semaphore &&frame_timeline,
                       std::vector<std::vector<vk::raii::CommandPool>> &&command_pools,
                       std::vector<std::uint32_t> queue_families)
      : _frame_timeline(std::move(frame_timeline)),
        _command_pools(std::move(command_pools)),
        _queue_families(std::move(queue_families)) {
  }
}
