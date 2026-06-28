#ifndef KISHA_RENDERING_HPP
#define KISHA_RENDERING_HPP

#include <vulkan/vulkan_raii.hpp>
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "errors.hpp"
#include "frame_ring.hpp"

namespace kisha::engine {
  struct PipelineHandle {
    std::uint32_t id = 0U; // 0 reserved as 'invalid'
  };

  struct ImageHandle {
    std::uint32_t id = 0U; // 0 reserved as 'invalid'
  };

  struct GraphicsPipelineDescription {
    std::span<const std::uint32_t> vertex_spirv;
    std::span<const std::uint32_t> fragment_spirv;
    vk::Format color_format = vk::Format::eUndefined; // swapchain format
    vk::PrimitiveTopology topology = vk::PrimitiveTopology::eTriangleList;
  };

  /**
   * @brief One declared access of a tracked image within a pass.
   */
  struct ImageAccess {
    ImageHandle image;
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eNone;
    vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;
  };

  struct ColorAttachmentDescription {
    ImageHandle target; // tracked image to render into
    std::array<float, 4> clear_color{0.F, 0.F, 0.F, 1.F};
  };

  /**
   * @brief A single draw referencing a pipeline and a vertex count.
   */
  struct DrawItem {
    PipelineHandle pipeline;
    std::uint32_t vertex_count = 3U;
  };

  /**
   * @brief An ordered unit of work carrying its resource access declarations.
   */
  struct PassDescription {
    std::vector<ImageAccess> image_accesses; // declared before recording the pass
    ColorAttachmentDescription color;
    std::vector<DrawItem> draws;
  };

  /**
   * @brief A frame's work as an ordered list of passes.
   */
  struct FrameDescription {
    std::vector<PassDescription> passes;
  };

  struct ImageState {
    vk::ImageLayout layout = vk::ImageLayout::eUndefined;
    vk::PipelineStageFlags2 stage = vk::PipelineStageFlagBits2::eTopOfPipe;
    vk::AccessFlags2 access = vk::AccessFlagBits2::eNone;
  };

  /**
   * @brief Tracks the last-known state of each registered image and emits the
   *        minimal synchronization2 barrier only when a declared access differs.
   */
  class ResourceStateTracker {
  public:
    void register_image(ImageHandle handle, vk::Image image, const ImageState &initial);
    void reset_image(ImageHandle handle, vk::ImageLayout layout); // e.g. swapchain image -> eUndefined per frame

    // Returns a barrier only when the requested access differs from last-known; updates stored state.
    std::optional<vk::ImageMemoryBarrier2> transition(const ImageAccess &access);

    std::expected<std::optional<vk::ImageMemoryBarrier2>, EngineError>
        transition_to(ImageHandle handle, vk::ImageLayout layout, vk::PipelineStageFlags2 stage,
                       vk::AccessFlags2 access);

  private:
    struct TrackedImage {
      vk::Image image{};
      ImageState state{};
    };

    std::unordered_map<std::uint32_t, TrackedImage> _images;
  };

  class FrameContext {
  public:
    FrameContext() = default;

    // Engine-internal constructor binding the context to an acquired frame.
    FrameContext(std::uint32_t image_index, vk::Semaphore image_available, FrameRecorder *recorder,
                 ImageHandle swapchain_image)
        : _image_index(image_index), _image_available(image_available), _recorder(recorder),
          _swapchain_image(swapchain_image) {}

    [[nodiscard]] ImageHandle swapchain_image() const { return _swapchain_image; }

    void add_pass(PassDescription pass) { _description.passes.push_back(std::move(pass)); }
    void set_color_target(const ColorAttachmentDescription &color) { default_pass().color = color; }
    void add_draw(const DrawItem &draw) { default_pass().draws.push_back(draw); }

    // Deferred raw recording path: declared now, returns NotImplemented.
    [[nodiscard]] std::expected<FrameRecorder *, EngineError> raw() {
      return std::unexpected(EngineError::NotImplemented);
    }

    [[nodiscard]] std::uint32_t image_index() const { return _image_index; }
    [[nodiscard]] vk::Semaphore image_available() const { return _image_available; }
    [[nodiscard]] FrameRecorder *recorder() const { return _recorder; }
    [[nodiscard]] const FrameDescription &description() const { return _description; }

  private:
    PassDescription &default_pass() {
      if (!_default_pass_index.has_value()) {
        _default_pass_index = _description.passes.size();
        _description.passes.emplace_back();
      }
      return _description.passes[*_default_pass_index];
    }

    std::uint32_t _image_index = 0U;
    vk::Semaphore _image_available{};
    FrameRecorder *_recorder = nullptr;
    ImageHandle _swapchain_image{};
    FrameDescription _description{};
    std::optional<std::size_t> _default_pass_index;
  };
}

#endif //KISHA_RENDERING_HPP
