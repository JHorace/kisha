#include "rendering.hpp"

namespace kisha::engine {
  namespace {
    constexpr vk::ImageSubresourceRange kColorSubresourceRange{vk::ImageAspectFlagBits::eColor, 0U,
                                                               vk::RemainingMipLevels, 0U,
                                                               vk::RemainingArrayLayers};

    [[nodiscard]] bool states_match(const ImageState &state, const vk::ImageLayout layout,
                                    const vk::PipelineStageFlags2 stage, const vk::AccessFlags2 access) {
      return state.layout == layout && state.stage == stage && state.access == access;
    }

    [[nodiscard]] vk::ImageMemoryBarrier2 make_barrier(const vk::Image image, const ImageState &from,
                                                       const vk::ImageLayout layout,
                                                       const vk::PipelineStageFlags2 stage,
                                                       const vk::AccessFlags2 access) {
      return vk::ImageMemoryBarrier2{}
          .setSrcStageMask(from.stage)
          .setSrcAccessMask(from.access)
          .setDstStageMask(stage)
          .setDstAccessMask(access)
          .setOldLayout(from.layout)
          .setNewLayout(layout)
          .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
          .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
          .setImage(image)
          .setSubresourceRange(kColorSubresourceRange);
    }
  }

  void ResourceStateTracker::register_image(const ImageHandle handle, const vk::Image image,
                                            const ImageState &initial) {
    _images[handle.id] = TrackedImage{image, initial};
  }

  void ResourceStateTracker::reset_image(const ImageHandle handle, const vk::ImageLayout layout) {
    // For new images (this includes acquired swapchain images), the past state is irrelevant, so set to none.
    if (const auto tracked = _images.find(handle.id); tracked != _images.end()) {
      tracked->second.state = ImageState{layout, vk::PipelineStageFlagBits2::eTopOfPipe,
                                         vk::AccessFlagBits2::eNone};
    }
  }

  std::optional<vk::ImageMemoryBarrier2> ResourceStateTracker::transition(const ImageAccess &access) {
    const auto tracked = _images.find(access.image.id);
    if (tracked == _images.end()) {
      return std::nullopt;
    }

    if (states_match(tracked->second.state, access.layout, access.stage, access.access)) {
      return std::nullopt;
    }

    const vk::ImageMemoryBarrier2 barrier =
        make_barrier(tracked->second.image, tracked->second.state, access.layout, access.stage, access.access);
    tracked->second.state = ImageState{access.layout, access.stage, access.access};
    return barrier;
  }

  std::expected<std::optional<vk::ImageMemoryBarrier2>, EngineError>
  ResourceStateTracker::transition_to(const ImageHandle handle, const vk::ImageLayout layout,
                                      const vk::PipelineStageFlags2 stage, const vk::AccessFlags2 access) {
    const auto tracked = _images.find(handle.id);
    if (tracked == _images.end()) {
      return std::unexpected(EngineError::UnknownResource);
    }

    if (states_match(tracked->second.state, layout, stage, access)) {
      return std::optional<vk::ImageMemoryBarrier2>{};
    }

    const vk::ImageMemoryBarrier2 barrier =
        make_barrier(tracked->second.image, tracked->second.state, layout, stage, access);
    tracked->second.state = ImageState{layout, stage, access};
    return std::optional<vk::ImageMemoryBarrier2>{barrier};
  }

  std::expected<PassBarriers, EngineError> ResourceStateTracker::barriers_for(const PassDescription &pass) {
    PassBarriers result;
    result.image_barriers.reserve(pass.image_accesses.size());

    for (const ImageAccess &access : pass.image_accesses) {
      std::expected<std::optional<vk::ImageMemoryBarrier2>, EngineError> barrier =
          transition_to(access.image, access.layout, access.stage, access.access);
      if (!barrier) {
        return std::unexpected(barrier.error());
      }
      if (barrier->has_value()) {
        result.image_barriers.push_back(**barrier);
      }
    }

    return result;
  }
}
