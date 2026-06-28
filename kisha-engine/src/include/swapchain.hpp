#ifndef KISHA_SWAPCHAIN_HPP
#define KISHA_SWAPCHAIN_HPP

#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <expected>
#include <vector>

#include "errors.hpp"

namespace kisha::engine {
  struct PresentModeCapabilities {
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    std::uint32_t min_image_count = 0U;
    std::uint32_t max_image_count = 0U;
    std::vector<vk::PresentModeKHR> compatible_present_modes;
    vk::PresentScalingFlagsKHR supported_scaling{};
    vk::PresentGravityFlagsKHR supported_gravity_x{};
    vk::PresentGravityFlagsKHR supported_gravity_y{};
    vk::Extent2D min_scaled_image_extent{};
    vk::Extent2D max_scaled_image_extent{};
  };

  struct SurfaceCapabilities {
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
    std::vector<PresentModeCapabilities> per_present_mode;
  };

  struct SwapchainConfig {
    vk::Extent2D extent{};
    vk::SurfaceFormatKHR surface_format{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    std::uint32_t min_image_count = 0U;
    vk::ImageUsageFlags image_usage = vk::ImageUsageFlagBits::eColorAttachment;
  };

  /**
   * Swapchain is a swapchain! Its a wrapper around the Vulkan handle and the associated images and image views.
   * Swapchain is always 'valid' in that it always has these resources.
   * Swapchain creation (the static Swapchain::create) can fail however, in which case no Swapchain is returned.
   * It is up to the Presenter class to handle this failure.
   */
  class Swapchain {
  public:
    Swapchain(Swapchain &&other) noexcept = default;
    Swapchain &operator=(Swapchain &&other) noexcept = default;

    Swapchain(const Swapchain &) = delete;
    Swapchain &operator=(const Swapchain &) = delete;

    [[nodiscard]] static std::expected<Swapchain, EngineError> create(
        const vk::raii::Device &device, const vk::raii::PhysicalDevice &physical_device,
        const vk::raii::SurfaceKHR &surface, std::uint32_t present_queue_family, const SwapchainConfig &config,
        vk::SwapchainKHR old_swapchain = VK_NULL_HANDLE);

    [[nodiscard]] const vk::raii::SwapchainKHR &handle() const { return _swapchain; }
    [[nodiscard]] const std::vector<vk::Image> &images() const { return _images; }
    [[nodiscard]] const std::vector<vk::raii::ImageView> &image_views() const { return _image_views; }
    [[nodiscard]] vk::Format format() const { return _format; }
    [[nodiscard]] vk::Extent2D extent() const { return _extent; }
    [[nodiscard]] vk::PresentModeKHR present_mode() const { return _present_mode; }
    [[nodiscard]] const std::vector<vk::PresentModeKHR> &compatible_present_modes() const {
      return _compatible_present_modes;
    }
    [[nodiscard]] const vk::raii::Semaphore &render_finished(std::uint32_t image_index) const {
      return _render_finished[image_index];
    }

    /**
     * We use VK_KHR_swapchain_maintenance1 and VK_KHR_surface_maintenance1, so in theory we can change the present mode w/o recreating the swapchain.
     * I haven't actually wired this up yet.
     */
    /**
     * Primer on present modes:
     *  Immediate:   Present images immediately w/o waiting for vertical blank - will cause tearing if the swapchain flips a presented image outside the vblank window
     *               AFAIK, this is not possible to fix with synchronization as while we control the presentation timing we don't control the flip timing.
     *               This mode introduces no latency cost.
     * eFifo :       Presented images are queued. The front of the queue is flipped when a vblank occurs. As the flip always waits for vblank, there is no tearing.
     *               This introduces latency if frames are rendered faster than they are presented, and can cause frame pacing issues if the application runs ahead of t
     * eFifoRelaxed: Similar to eFifo, but if the application misses the vblank window once, it will immediately flip the next image that gets presented.
     *               This reduces eFifo's latency in cases where the application can't match the refresh rate, at the cost of tearing.
     * eMailbox:     Presented images are placed in a 'mailbox' - Like the FIFO queue, images placed here must wait for the vblank window to flip, so there is no tearing.
     *               Unlike the FIFO queue, any subsequently presented images replace the image currently in the mailbox.
     *               This limits introduced latency to the length of a single refresh, but can introduce frame pacing issues if the application runs ahead of the refresh rate.
     *
     *               ...There is also eFifoLatestReady. That one is newer, and meant to be used with some newer present timing extensions. I don't fully understand it yet.
     *               The present timing extensions seem to expose ways to query the actual time displays take to present. I think the idea is for applications to use the timing
     *               to do adaptive/predictive performance-based stuff. Too advanced to deal with right now, but maybe worth looking at later.
     */
    void set_present_mode(vk::PresentModeKHR present_mode) { _present_mode = present_mode; }

  private:
    Swapchain() = default;

    vk::raii::SwapchainKHR _swapchain{nullptr};
    std::vector<vk::Image> _images;
    std::vector<vk::raii::ImageView> _image_views;
    vk::Format _format = vk::Format::eUndefined;
    vk::Extent2D _extent{};
    vk::PresentModeKHR _present_mode = vk::PresentModeKHR::eFifo;
    std::vector<vk::PresentModeKHR> _compatible_present_modes;
    std::vector<vk::raii::Semaphore> _render_finished;
  };
}

#endif //KISHA_SWAPCHAIN_HPP
