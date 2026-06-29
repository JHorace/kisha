#ifndef KISHA_FRAMECONTEXT_HPP
#define KISHA_FRAMECONTEXT_HPP

#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <expected>
#include <optional>
#include <vector>

#include "errors.hpp"

namespace kisha::engine {

  struct FrameRecorder {
    uint32_t frame_slot;
    vk::raii::CommandBuffer graphics_command_buffer;
    std::optional<vk::raii::CommandBuffer> async_compute_command_buffer;
    std::optional<vk::raii::CommandBuffer> transfer_command_buffer;
  };

  /**
   * Owns per-frame resources - basically encapsulates 'frames-in-flight'
   * One important distinction to remember is that 'frames-in-flight' are not necessarily the same as swapchain images.
   * 'Frames-in-flight' are an application notion - the number of frame command lists the application allows itself to record before it starts blocking on the present fence.
   * Swapchain images are a GPU resource notion - The number of images that are actually available to render to and present.
   * They're likely to both be around 2 or 3, but they are still different knobs that affect presentation timing differently.
   */
  class FrameRing {
  public:
    static constexpr std::uint32_t FRAMES_IN_FLIGHT = 2U;

    FrameRing(FrameRing &&other) noexcept = default;
    FrameRing &operator=(FrameRing &&other) noexcept = default;

    FrameRing(const FrameRing &) = delete;
    FrameRing &operator=(const FrameRing &) = delete;

    [[nodiscard]] static std::expected<FrameRing, EngineError> create(const vk::raii::Device &device,
                                                                             std::uint32_t frame_count,
                                                                             std::uint32_t graphics_family,
                                                                             std::optional<std::uint32_t> async_compute_family,
                                                                             std::optional<std::uint32_t> transfer_family);

    [[nodiscard]] std::expected<FrameRecorder *, EngineError> begin_frame(const vk::raii::Device& device);

    [[nodiscard]] std::expected<void, EngineError> submit_frame(const vk::raii::Queue &graphics_queue,
                                                                FrameRecorder &recorder,
                                                                vk::Semaphore wait_image_available,
                                                                vk::Semaphore signal_render_finished);

  private:
    FrameRing(vk::raii::Semaphore&& frame_timeline,
              std::vector<std::vector<vk::raii::CommandPool>>&& command_pools,
              std::vector<FrameRecorder>&& frames);
    vk::raii::Semaphore _frame_timeline;
    // Per-frame command pools, dimensioned (threads) x (frames-in-flight) x (distinct queue families).
    // Single-threaded for now, so the outer vector is indexed by frame slot, the inner by distinct
    // queue family. The pools must outlive the command buffers in _frames that were allocated from them.
    std::vector<std::vector<vk::raii::CommandPool>> _command_pools = {};
    // Per-frame-slot resources (command buffers) allocated up front from _command_pools.
    std::vector<FrameRecorder> _frames = {};
    // Per-slot timeline value that the slot's last submission signals; begin_frame waits on it
    // before reusing the slot. Sized to the number of frames-in-flight.
    std::vector<uint64_t> _frame_slot = {};
    // counter of submitted frames; also the next timeline value to signal.
    uint64_t _submit_index = 0U;
    // counter of frames begun; selects the active slot (mod frames-in-flight).
    uint64_t _frame_counter = 0U;
  };
}

#endif //KISHA_FRAMECONTEXT_HPP
