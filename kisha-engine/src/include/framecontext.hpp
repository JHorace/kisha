#ifndef KISHA_FRAMECONTEXT_HPP
#define KISHA_FRAMECONTEXT_HPP

#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <expected>
#include <vector>

#include "errors.hpp"

namespace kisha::engine {
  /**
   * Owns per-frame resources - basically encapsulates 'frames-in-flight'
   * One important distinction to remember is that 'frames-in-flight' are not necessarily the same as swapchain images.
   * 'Frames-in-flight' are an application notion - the number of frame command lists the application allows itself to record before it starts blocking on the present fence.
   * Swapchain images are a GPU resource notion - The number of images that are actually available to render to and present.
   * They're likely to both be around 2 or 3, but they are still different knobs that affect presentation timing differently.
   */

  class FrameContext {
  public:
    static constexpr std::uint32_t MAX_FRAMES_IN_FLIGHT = 2U;

    FrameContext(FrameContext &&other) noexcept = default;
    FrameContext &operator=(FrameContext &&other) noexcept = default;

    FrameContext(const FrameContext &) = delete;
    FrameContext &operator=(const FrameContext &) = delete;

    [[nodiscard]] static std::expected<FrameContext, EngineInitError> create(const vk::raii::Device &device,
                                                                             std::uint32_t image_count);

    [[nodiscard]] const vk::raii::Semaphore &image_available(std::uint32_t frame) const {
      return _image_available[frame];
    }
    [[nodiscard]] const vk::raii::Fence &in_flight(std::uint32_t frame) const { return _in_flight[frame]; }
    [[nodiscard]] std::uint32_t current_frame() const { return _current_frame; }
    [[nodiscard]] std::uint32_t current_image_index() const { return _current_image_index; }
    [[nodiscard]] std::uint32_t image_count() const {
      return _image_count;
    }

    void set_current_image_index(std::uint32_t image_index) { _current_image_index = image_index; }
    void advance_frame() { _current_frame = (_current_frame + 1U) % MAX_FRAMES_IN_FLIGHT; }

  private:
    FrameContext() = default;

    std::vector<vk::raii::Semaphore> _image_available;
    std::vector<vk::raii::Fence> _in_flight;
    std::uint32_t _image_count = 0U;
    std::uint32_t _current_frame = 0U;
    std::uint32_t _current_image_index = 0U;
  };
}

#endif //KISHA_FRAMECONTEXT_HPP
