/**
 * @file swapchain.cpp
 * @brief Swapchain construction (handle, images, image views) implementation.
 */
#include "swapchain.hpp"
#include "engine_init_helpers.hpp"

#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

namespace kisha::engine {
  namespace {
    std::uint32_t choose_image_count(const vk::SurfaceCapabilitiesKHR &caps, const std::uint32_t desired) {
      std::uint32_t count = (desired == 0U) ? caps.minImageCount + 1U : desired;
      count = std::max(count, caps.minImageCount);
      if (caps.maxImageCount > 0U) {
        count = std::min(count, caps.maxImageCount);
      }
      return count;
    }

    vk::Extent2D choose_extent(const vk::SurfaceCapabilitiesKHR &caps, const vk::Extent2D desired) {
      // currentExtent of std::numeric_limits::max() means the surface size is dictated by the swapchain
      if (caps.currentExtent.width != std::numeric_limits<std::uint32_t>::max()) {
        return caps.currentExtent;
      }
      return vk::Extent2D{
          std::clamp(desired.width, caps.minImageExtent.width, caps.maxImageExtent.width),
          std::clamp(desired.height, caps.minImageExtent.height, caps.maxImageExtent.height),
      };
    }
  }

  std::expected<Swapchain, EngineInitError> Swapchain::create(const vk::raii::Device &device,
                                                              const vk::raii::PhysicalDevice &physical_device,
                                                              const vk::raii::SurfaceKHR &surface,
                                                              const std::uint32_t present_queue_family,
                                                              const SwapchainConfig &config,
                                                              const vk::SwapchainKHR old_swapchain) {
    std::expected<SurfaceCapabilities, EngineInitError> caps = util::query_surface_capabilities(physical_device, surface);
    if (!caps) {
      return std::unexpected(caps.error());
    }

    std::vector<vk::PresentModeKHR> compatible_modes;
    for (const PresentModeCapabilities &mode_caps : caps->per_present_mode) {
      if (mode_caps.present_mode == config.present_mode) {
        compatible_modes = mode_caps.compatible_present_modes;
        break;
      }
    }
    if (std::ranges::find(compatible_modes, config.present_mode) == compatible_modes.end()) {
      compatible_modes.push_back(config.present_mode);
    }

    const vk::Extent2D extent = choose_extent(caps->capabilities, config.extent);
    const std::uint32_t image_count = choose_image_count(caps->capabilities, config.min_image_count);

    vk::SwapchainCreateInfoKHR create_info = vk::SwapchainCreateInfoKHR{}
        .setSurface(*surface)
        .setMinImageCount(image_count)
        .setImageFormat(config.surface_format.format)
        .setImageColorSpace(config.surface_format.colorSpace)
        .setImageExtent(extent)
        .setImageArrayLayers(1U)
        .setImageUsage(config.image_usage)
        .setImageSharingMode(vk::SharingMode::eExclusive)
        .setQueueFamilyIndices(present_queue_family)
        .setPreTransform(caps->capabilities.currentTransform)
        .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
        .setPresentMode(config.present_mode)
        .setClipped(true)
        .setOldSwapchain(old_swapchain);

    const vk::SwapchainPresentModesCreateInfoKHR present_modes_info =
        vk::SwapchainPresentModesCreateInfoKHR{}.setPresentModes(compatible_modes);
    create_info.setPNext(&present_modes_info);

    std::expected<vk::raii::SwapchainKHR, vk::Result> swapchain = device.createSwapchainKHR(create_info);
    if (!swapchain) {
      spdlog::error("Failed to create swapchain: {}", vk::to_string(swapchain.error()));
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }

    std::expected<std::vector<vk::Image>, vk::Result> images = swapchain->getImages();
    if (!images) {
      spdlog::error("Failed to retrieve swapchain images: {}", vk::to_string(images.error()));
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }

    std::vector<vk::raii::ImageView> image_views;
    image_views.reserve(images->size());
    for (const vk::Image &image : *images) {
      const vk::ImageViewCreateInfo view_info = vk::ImageViewCreateInfo{}
          .setImage(image)
          .setViewType(vk::ImageViewType::e2D)
          .setFormat(config.surface_format.format)
          .setComponents(vk::ComponentMapping{})
          .setSubresourceRange(vk::ImageSubresourceRange{}
              .setAspectMask(vk::ImageAspectFlagBits::eColor)
              .setBaseMipLevel(0U)
              .setLevelCount(1U)
              .setBaseArrayLayer(0U)
              .setLayerCount(1U));

      std::expected<vk::raii::ImageView, vk::Result> view = device.createImageView(view_info);
      if (!view) {
        spdlog::error("Failed to create swapchain image view: {}", vk::to_string(view.error()));
        return std::unexpected(EngineInitError::SwapchainCreationFailed);
      }
      image_views.push_back(std::move(*view));
    }

    std::vector<vk::raii::Semaphore> render_finished;
    render_finished.reserve(image_count);
    for (std::uint32_t image = 0U; image < image_count; ++image) {
      std::expected<vk::raii::Semaphore, vk::Result> semaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
      if (!semaphore) {
        spdlog::error("Failed to create render-finished semaphore: {}", vk::to_string(semaphore.error()));
        return std::unexpected(EngineInitError::FrameSyncCreationFailed);
      }
      render_finished.push_back(std::move(*semaphore));
    }

    Swapchain result;
    result._swapchain = std::move(*swapchain);
    result._images = std::move(*images);
    result._image_views = std::move(image_views);
    result._format = config.surface_format.format;
    result._extent = extent;
    result._present_mode = config.present_mode;
    result._compatible_present_modes = std::move(compatible_modes);
    result._render_finished = std::move(render_finished);
    return result;
  }
}
