#ifndef KISHA_FRAMECONTEXT_HPP
#define KISHA_FRAMECONTEXT_HPP

#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <expected>
#include <vector>

#include "errors.hpp"

namespace kisha::engine {

  struct FrameResources {
    uint32_t frame_slot;
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
                                                                             const std::vector<std::uint32_t> &queue_families);

    [[nodiscard]] std::expected<FrameResources, EngineError> begin_frame(const vk::raii::Device& device);

  private:
    FrameRing(vk::raii::Semaphore&& frame_timeline,
              std::vector<std::vector<vk::raii::CommandPool>>&& command_pools,
              std::vector<std::uint32_t> queue_families);
    vk::raii::Semaphore _frame_timeline;
    std::vector<std::vector<vk::raii::CommandPool>> _command_pools = {};
    // The distinct queue family indices the command pools were created for.
    std::vector<std::uint32_t> _queue_families = {};
    std::vector<uint64_t> _frame_slot = {};
    std::vector<uint64_t> _submit_index = {};
    uint64_t _frame_counter = 0U;
    uint64_t _next_frame_index = 0U;
  };
}

#endif //KISHA_FRAMECONTEXT_HPP
