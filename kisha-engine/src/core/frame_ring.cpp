#include "frame_ring.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>

#include "presenter.hpp"

namespace kisha::engine {
  namespace {
    std::expected<vk::raii::CommandBuffer, EngineError> allocate_primary_command_buffer(
        const vk::raii::Device &device, const vk::raii::CommandPool &pool) {
      const vk::CommandBufferAllocateInfo allocate_info{*pool, vk::CommandBufferLevel::ePrimary, 1U};
      std::expected<std::vector<vk::raii::CommandBuffer>, vk::Result> command_buffers =
          device.allocateCommandBuffers(allocate_info);
      if (!command_buffers) {
        spdlog::error("Failed to allocate command buffer: {}", vk::to_string(command_buffers.error()));
        return std::unexpected(EngineError::Unknown);
      }
      return std::move(command_buffers->front());
    }
  }

  std::expected<FrameRing, EngineError> FrameRing::create(const vk::raii::Device &device,
                                                                    const std::uint32_t frame_count,
                                                                    const std::uint32_t graphics_family,
                                                                    const std::optional<std::uint32_t> async_compute_family,
                                                                    const std::optional<std::uint32_t> transfer_family) {
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
    // single-threaded for now, we have (frames-in-flight x distinct queue families) pools. Roles
    // (graphics / async-compute / transfer) may share a queue family, so we deduplicate the families
    // and remember which pool slot each role maps to.
    std::vector<std::uint32_t> distinct_families;
    const auto pool_index_for = [&distinct_families](const std::uint32_t family) -> std::size_t {
      const auto it = std::ranges::find(distinct_families, family);
      if (it != distinct_families.end()) {
        return static_cast<std::size_t>(it - distinct_families.begin());
      }
      distinct_families.push_back(family);
      return distinct_families.size() - 1U;
    };

    const std::size_t graphics_pool_index = pool_index_for(graphics_family);
    const std::optional<std::size_t> async_compute_pool_index =
        async_compute_family ? std::optional{pool_index_for(*async_compute_family)} : std::nullopt;
    const std::optional<std::size_t> transfer_pool_index =
        transfer_family ? std::optional{pool_index_for(*transfer_family)} : std::nullopt;

    std::vector<std::vector<vk::raii::CommandPool>> command_pools;
    command_pools.reserve(FRAMES_IN_FLIGHT);
    std::vector<FrameRecorder> frames;
    frames.reserve(FRAMES_IN_FLIGHT);

    for (std::uint32_t frame = 0U; frame < FRAMES_IN_FLIGHT; ++frame) {
      std::vector<vk::raii::CommandPool> per_family_pools;
      per_family_pools.reserve(distinct_families.size());
      for (const std::uint32_t queue_family : distinct_families) {
        // eResetCommandBuffer lets each frame's command buffer be implicitly reset by begin() when
        // its slot is reused (begin_frame has already waited on the timeline, so the previous
        // submission has completed and the buffer is no longer pending).
        vk::CommandPoolCreateInfo command_pool_create_info{
            vk::CommandPoolCreateFlagBits::eTransient | vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
            queue_family};
        std::expected<vk::raii::CommandPool, vk::Result> command_pool =
            device.createCommandPool(command_pool_create_info);
        if (!command_pool) {
          spdlog::error("Failed to create command pool for queue family {}: {}", queue_family,
                        vk::to_string(command_pool.error()));
          return std::unexpected(EngineError::Unknown);
        }
        per_family_pools.push_back(std::move(*command_pool));
      }

      // Allocate this frame's command buffers from the pools we just created. Graphics is mandatory;
      // async-compute and transfer are only present when the engine has those queue families.
      std::expected<vk::raii::CommandBuffer, EngineError> graphics_command_buffer =
          allocate_primary_command_buffer(device, per_family_pools[graphics_pool_index]);
      if (!graphics_command_buffer) {
        return std::unexpected(graphics_command_buffer.error());
      }

      std::optional<vk::raii::CommandBuffer> async_compute_command_buffer;
      if (async_compute_pool_index) {
        std::expected<vk::raii::CommandBuffer, EngineError> buffer =
            allocate_primary_command_buffer(device, per_family_pools[*async_compute_pool_index]);
        if (!buffer) {
          return std::unexpected(buffer.error());
        }
        async_compute_command_buffer = std::move(*buffer);
      }

      std::optional<vk::raii::CommandBuffer> transfer_command_buffer;
      if (transfer_pool_index) {
        std::expected<vk::raii::CommandBuffer, EngineError> buffer =
            allocate_primary_command_buffer(device, per_family_pools[*transfer_pool_index]);
        if (!buffer) {
          return std::unexpected(buffer.error());
        }
        transfer_command_buffer = std::move(*buffer);
      }

      frames.push_back(FrameRecorder{
          .frame_slot = frame,
          .graphics_command_buffer = std::move(*graphics_command_buffer),
          .async_compute_command_buffer = std::move(async_compute_command_buffer),
          .transfer_command_buffer = std::move(transfer_command_buffer),
      });
      command_pools.push_back(std::move(per_family_pools));
    }

    return FrameRing(std::move(*semaphore), std::move(command_pools), std::move(frames));
  }

  std::expected<FrameRecorder *, EngineError> FrameRing::begin_frame(const vk::raii::Device &device) {
    uint32_t slot = _frame_counter % FRAMES_IN_FLIGHT;

    // Timeline semaphores let us signal/wait on a particular value - this simplifies synchronization a lot as we can essentially wait on a specific frame to finish rendering.
    if (auto result = device.waitSemaphores(vk::SemaphoreWaitInfo{vk::SemaphoreWaitFlagBits::eAny, *_frame_timeline, _frame_slot[slot]},  std::numeric_limits<std::uint64_t>::max());
      result != vk::Result::eSuccess) {
      spdlog::error("Failed to wait on timeline semaphore: {}", vk::to_string(result));
      return std::unexpected(EngineError::Unknown);
    }

    return &_frames[slot];
  }

  std::expected<void, EngineError> FrameRing::submit_frame(const vk::raii::Queue &graphics_queue,
                                                           FrameRecorder &recorder,
                                                           const vk::Semaphore wait_image_available,
                                                           const vk::Semaphore signal_render_finished) {
    const std::uint64_t signal_value = ++_submit_index;

    // The acquired image is ready once the image-available semaphore signals; the first stage that
    // touches it is color-attachment output, so that's all we need to wait for.
    const vk::SemaphoreSubmitInfo wait_info = vk::SemaphoreSubmitInfo{}
        .setSemaphore(wait_image_available)
        .setStageMask(vk::PipelineStageFlagBits2::eColorAttachmentOutput);

    const std::array<vk::SemaphoreSubmitInfo, 2U> signal_infos{
        // Binary, consumed by Presenter::present. Signal at eAllCommands so the implicit
        // present-layout transition recorded at the end of the frame has completed first.
        vk::SemaphoreSubmitInfo{}
            .setSemaphore(signal_render_finished)
            .setStageMask(vk::PipelineStageFlagBits2::eAllCommands),
        // Timeline, waited on by a future begin_frame before this slot is reused.
        vk::SemaphoreSubmitInfo{}
            .setSemaphore(*_frame_timeline)
            .setValue(signal_value)
            .setStageMask(vk::PipelineStageFlagBits2::eAllCommands),
    };

    const vk::CommandBufferSubmitInfo command_buffer_info =
        vk::CommandBufferSubmitInfo{}.setCommandBuffer(*recorder.graphics_command_buffer);

    const vk::SubmitInfo2 submit_info = vk::SubmitInfo2{}
        .setWaitSemaphoreInfos(wait_info)
        .setCommandBufferInfos(command_buffer_info)
        .setSignalSemaphoreInfos(signal_infos);

    if (std::expected<void, vk::Result> submitted = graphics_queue.submit2(submit_info); !submitted) {
      spdlog::error("Failed to submit graphics command buffer: {}", vk::to_string(submitted.error()));
      return std::unexpected(EngineError::SubmitFailed);
    }

    // This slot's next reuse must wait until this submission's timeline value is signaled, and the
    // next frame advances to the following slot.
    _frame_slot[recorder.frame_slot] = signal_value;
    ++_frame_counter;
    return {};
  }

  FrameRing::FrameRing(vk::raii::Semaphore &&frame_timeline,
                       std::vector<std::vector<vk::raii::CommandPool>> &&command_pools,
                       std::vector<FrameRecorder> &&frames)
      : _frame_timeline(std::move(frame_timeline)),
        _command_pools(std::move(command_pools)),
        _frames(std::move(frames)) {
    // One stored timeline value per slot, all starting at 0 to match the timeline's initial value
    // (so the first reuse of each slot waits on a value that is already signaled).
    _frame_slot.assign(_frames.size(), 0U);
  }
}
