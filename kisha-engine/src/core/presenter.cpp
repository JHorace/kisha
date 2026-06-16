/**
 * @file presenter.cpp
 * @brief Presenter surface-capability query and swapchain management implementation.
 */
#include "presenter.hpp"
#include "engine_init_helpers.hpp"

#include <algorithm>
#include <limits>
#include <spdlog/spdlog.h>

namespace kisha::engine {
  namespace {
    struct SwapchainBuild {
      vk::raii::SwapchainKHR swapchain{nullptr};
      std::vector<vk::Image> images;
      vk::Format format = vk::Format::eUndefined;
      vk::Extent2D extent{};
      vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
      std::vector<vk::PresentModeKHR> compatible_present_modes;
    };

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

    std::expected<SwapchainBuild, EngineInitError> make_swapchain(const vk::raii::Device &device,
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

      // VK_KHR_swapchain_maintenance1: declare up front the present modes this
      // swapchain may switch between, so per-present mode switching needs no recreation.
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

      return SwapchainBuild{
          .swapchain = std::move(*swapchain),
          .images = std::move(*images),
          .format = config.surface_format.format,
          .extent = extent,
          .present_mode = config.present_mode,
          .compatible_present_modes = std::move(compatible_modes),
      };
    }
  }

  std::expected<SurfaceCapabilities, EngineInitError> Presenter::capabilities() const {
    return util::query_surface_capabilities(_physical_device, _surface);
  }

  std::expected<void, EngineInitError> Presenter::create_swapchain(const vk::raii::Device &device,
                                                                   const SwapchainConfig &config) {
    if (*_swapchain != VK_NULL_HANDLE) {
      return recreate_swapchain(device, config);
    }

    return make_swapchain(device, _physical_device, _surface, _present_queue_family, config, VK_NULL_HANDLE)
        .transform([this](SwapchainBuild build) {
          _swapchain = std::move(build.swapchain);
          _swapchain_images = std::move(build.images);
          _swapchain_format = build.format;
          _swapchain_extent = build.extent;
          _present_mode = build.present_mode;
          _compatible_present_modes = std::move(build.compatible_present_modes);
          spdlog::info("Created swapchain ({} images, {}x{}, {})", _swapchain_images.size(), _swapchain_extent.width,
                       _swapchain_extent.height, vk::to_string(_present_mode));
        });
  }

  std::expected<void, EngineInitError> Presenter::recreate_swapchain(const vk::raii::Device &device,
                                                                     const SwapchainConfig &config) {
    const vk::SwapchainKHR old_handle = *_swapchain;

    return make_swapchain(device, _physical_device, _surface, _present_queue_family, config, old_handle)
        .transform([this](SwapchainBuild build) {
          if (*_swapchain != VK_NULL_HANDLE) {
            _retired_swapchains.push_back(RetiredSwapchain{
                .swapchain = std::move(_swapchain),
                .present_fences = std::move(_present_fences),
            });
            _present_fences.clear();
          }
          _swapchain = std::move(build.swapchain);
          _swapchain_images = std::move(build.images);
          _swapchain_format = build.format;
          _swapchain_extent = build.extent;
          _present_mode = build.present_mode;
          _compatible_present_modes = std::move(build.compatible_present_modes);
          spdlog::info("Recreated swapchain ({} images, {}x{}, {}); {} retired swapchain(s) pending",
                       _swapchain_images.size(), _swapchain_extent.width, _swapchain_extent.height,
                       vk::to_string(_present_mode), _retired_swapchains.size());
        });
  }

  std::expected<void, EngineInitError> Presenter::set_present_mode(const vk::PresentModeKHR present_mode) {
    if (*_swapchain == VK_NULL_HANDLE) {
      spdlog::error("Cannot switch present mode before the swapchain is created");
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }
    if (std::ranges::find(_compatible_present_modes, present_mode) == _compatible_present_modes.end()) {
      spdlog::warn("Present mode {} is not compatible with the active swapchain; recreation required",
                   vk::to_string(present_mode));
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }
    _present_mode = present_mode;
    return {};
  }

  void Presenter::prune_signaled_present_fences(const vk::raii::Device &device) {
    std::erase_if(_present_fences, [&device](const vk::raii::Fence &fence) {
      return device.waitForFences(*fence, VK_TRUE, 0U) == vk::Result::eSuccess;
    });
  }

  std::expected<vk::Result, EngineInitError> Presenter::present(const vk::raii::Device &device,
                                                                const vk::raii::Queue &queue,
                                                                const std::uint32_t image_index,
                                                                const vk::ArrayProxy<const vk::Semaphore>
                                                                    &wait_semaphores) {
    if (*_swapchain == VK_NULL_HANDLE) {
      spdlog::error("Cannot present before the swapchain is created");
      return std::unexpected(EngineInitError::PresentFailed);
    }

    prune_signaled_present_fences(device);

    std::expected<vk::raii::Fence, vk::Result> fence = device.createFence(vk::FenceCreateInfo{});
    if (!fence) {
      spdlog::error("Failed to create present fence: {}", vk::to_string(fence.error()));
      return std::unexpected(EngineInitError::PresentFailed);
    }

    const vk::SwapchainPresentModeInfoKHR present_mode_info =
        vk::SwapchainPresentModeInfoKHR{}.setPresentModes(_present_mode);
    const vk::SwapchainPresentFenceInfoKHR present_fence_info =
        vk::SwapchainPresentFenceInfoKHR{}.setFences(**fence).setPNext(&present_mode_info);
    vk::SwapchainPresentFenceInfoKHR{}.setFences(**fence);

    const vk::PresentInfoKHR present_info = vk::PresentInfoKHR{}
        .setWaitSemaphores(wait_semaphores)
        .setSwapchains(*_swapchain)
        .setImageIndices(image_index)
        .setPNext(&present_fence_info);

    const vk::Result result = queue.presentKHR(present_info);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR &&
        result != vk::Result::eErrorOutOfDateKHR) {
      spdlog::error("Failed to present swapchain image: {}", vk::to_string(result));
      return std::unexpected(EngineInitError::PresentFailed);
    }

    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
      _present_fences.push_back(std::move(*fence));
    }
    return result;
  }

  std::size_t Presenter::prune_retired_swapchains(const vk::raii::Device &device) {
    const std::size_t before = _retired_swapchains.size();
    std::erase_if(_retired_swapchains, [&device](const RetiredSwapchain &retired) {
      if (retired.present_fences.empty()) {
        return true;
      }
      std::vector<vk::Fence> handles;
      handles.reserve(retired.present_fences.size());
      for (const vk::raii::Fence &fence : retired.present_fences) {
        handles.push_back(*fence);
      }
      return device.waitForFences(handles, VK_TRUE, 0U) == vk::Result::eSuccess;
    });

    const std::size_t reaped = before - _retired_swapchains.size();
    if (reaped > 0U) {
      spdlog::debug("Pruned {} retired swapchain(s); {} still pending", reaped, _retired_swapchains.size());
    }
    return reaped;
  }
}
