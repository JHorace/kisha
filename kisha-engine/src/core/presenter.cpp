/**
 * @file presenter.cpp
 * @brief Presenter surface-capability query and swapchain management implementation.
 */
#include "presenter.hpp"
#include "engine_init_helpers.hpp"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace kisha::engine {
  std::expected<SurfaceCapabilities, EngineInitError> Presenter::capabilities() const {
    return util::query_surface_capabilities(_physical_device, _surface);
  }

  std::expected<void, EngineInitError> Presenter::create_swapchain(const vk::raii::Device &device,
                                                                   const SwapchainConfig &config) {
    if (_swapchain.has_value()) {
      return recreate_swapchain(device, config);
    }

    return Swapchain::create(device, _physical_device, _surface, _present_queue_family, config)
        .and_then([this, &device](Swapchain swapchain) -> std::expected<void, EngineInitError> {
          _swapchain = std::move(swapchain);
          spdlog::info("Created swapchain ({} images, {}x{}, {})", _swapchain->images().size(),
                       _swapchain->extent().width, _swapchain->extent().height,
                       vk::to_string(_swapchain->present_mode()));
          return create_frame_context(device);
        });
  }

  std::expected<void, EngineInitError> Presenter::recreate_swapchain(const vk::raii::Device &device,
                                                                     const SwapchainConfig &config) {
    const vk::SwapchainKHR old_handle = _swapchain.has_value() ? *_swapchain->handle() : VK_NULL_HANDLE;

    return Swapchain::create(device, _physical_device, _surface, _present_queue_family, config, old_handle)
        .and_then([this, &device](Swapchain swapchain) -> std::expected<void, EngineInitError> {
          if (_swapchain.has_value()) {
            _retired_swapchains.push_back(RetiredSwapchain{
                .swapchain = std::move(*_swapchain),
                .present_fences = std::move(_present_fences),
            });
            _present_fences.clear();
          }
          _swapchain = std::move(swapchain);
          spdlog::info("Recreated swapchain ({} images, {}x{}, {}); {} retired swapchain(s) pending",
                       _swapchain->images().size(), _swapchain->extent().width, _swapchain->extent().height,
                       vk::to_string(_swapchain->present_mode()), _retired_swapchains.size());
          return create_frame_context(device);
        });
  }

  std::expected<void, EngineInitError> Presenter::create_frame_context(const vk::raii::Device &device) {
    const auto image_count = static_cast<std::uint32_t>(_swapchain->images().size());
    return FrameContext::create(device, image_count).transform([this](FrameContext frame_context) {
      _frame_context = std::move(frame_context);
      spdlog::info("Created frame context ({} frames in flight, {} render-finished semaphores)",
                   FrameContext::MAX_FRAMES_IN_FLIGHT, _frame_context->image_count());
    });
  }

  std::expected<void, EngineInitError> Presenter::set_present_mode(const vk::PresentModeKHR present_mode) {
    if (!_swapchain.has_value()) {
      spdlog::error("Cannot switch present mode before the swapchain is created");
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }
    const std::vector<vk::PresentModeKHR> &compatible = _swapchain->compatible_present_modes();
    if (std::ranges::find(compatible, present_mode) == compatible.end()) {
      spdlog::warn("Present mode {} is not compatible with the active swapchain; recreation required",
                   vk::to_string(present_mode));
      return std::unexpected(EngineInitError::SwapchainCreationFailed);
    }
    _swapchain->set_present_mode(present_mode);
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
    if (!_swapchain.has_value()) {
      spdlog::error("Cannot present before the swapchain is created");
      return std::unexpected(EngineInitError::PresentFailed);
    }

    prune_signaled_present_fences(device);

    std::expected<vk::raii::Fence, vk::Result> fence = device.createFence(vk::FenceCreateInfo{});
    if (!fence) {
      spdlog::error("Failed to create present fence: {}", vk::to_string(fence.error()));
      return std::unexpected(EngineInitError::PresentFailed);
    }

    const vk::PresentModeKHR present_mode = _swapchain->present_mode();
    const vk::SwapchainPresentModeInfoKHR present_mode_info =
        vk::SwapchainPresentModeInfoKHR{}.setPresentModes(present_mode);
    const vk::SwapchainPresentFenceInfoKHR present_fence_info =
        vk::SwapchainPresentFenceInfoKHR{}.setFences(**fence).setPNext(&present_mode_info);

    const vk::PresentInfoKHR present_info = vk::PresentInfoKHR{}
        .setWaitSemaphores(wait_semaphores)
        .setSwapchains(*_swapchain->handle())
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
