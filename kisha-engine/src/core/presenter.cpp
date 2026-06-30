/**
 * @file presenter.cpp
 * @brief Presenter surface-capability query and swapchain management implementation.
 */
#include "presenter.hpp"
#include "engine_init_helpers.hpp"

#include <algorithm>
#include <limits>
#include <utility>
#include <spdlog/spdlog.h>

namespace kisha::engine {
  std::expected<Presenter, EngineError> Presenter::create(vk::raii::SurfaceKHR &&surface,
                                                          vk::raii::PhysicalDevice physical_device,
                                                          const std::uint32_t present_queue_family,
                                                          const vk::raii::Device &device,
                                                          const std::uint32_t frames_in_flight) {
    Presenter presenter(std::move(surface), std::move(physical_device), present_queue_family);
    if (std::expected<void, EngineError> semaphores = presenter.create_frame_semaphores(device, frames_in_flight);
        !semaphores) {
      return std::unexpected(semaphores.error());
    }
    return presenter;
  }

  std::expected<SurfaceCapabilities, EngineError> Presenter::capabilities() const {
    return util::query_surface_capabilities(_physical_device, _surface);
  }

  std::expected<void, EngineError> Presenter::create_swapchain(const vk::raii::Device &device,
                                                                   const SwapchainConfig &config) {
    if (_swapchain.has_value()) {
      return recreate_swapchain(device, config);
    }

    return Swapchain::create(device, _physical_device, _surface, _present_queue_family, config)
        .transform([this, &config](Swapchain swapchain) {
          _swapchain = std::move(swapchain);
          _active_config = config;
          spdlog::info("Created swapchain ({} images, {}x{}, {})", _swapchain->images().size(),
                       _swapchain->extent().width, _swapchain->extent().height,
                       vk::to_string(_swapchain->present_mode()));
        });
  }

  std::expected<void, EngineError> Presenter::recreate_swapchain(const vk::raii::Device &device,
                                                                     const SwapchainConfig &config) {
    const vk::SwapchainKHR old_handle = _swapchain.has_value() ? *_swapchain->handle() : VK_NULL_HANDLE;

    return Swapchain::create(device, _physical_device, _surface, _present_queue_family, config, old_handle)
        .transform([this, &device, &config](Swapchain swapchain){
          if (_swapchain.has_value()) {
            _retired_swapchains.push_back(RetiredSwapchain{
                .swapchain = std::move(*_swapchain),
                .present_fences = std::move(_present_fences),
            });
            _present_fences.clear();
          }
          _swapchain = std::move(swapchain);
          _active_config = config;
          spdlog::info("Recreated swapchain ({} images, {}x{}, {}); {} retired swapchain(s) pending",
                       _swapchain->images().size(), _swapchain->extent().width, _swapchain->extent().height,
                       vk::to_string(_swapchain->present_mode()), _retired_swapchains.size());
        });
  }

  std::expected<void, EngineError> Presenter::recreate_for_current_surface(const vk::raii::Device &device) {
    return recreate_swapchain(device, _active_config);
  }

  std::expected<void, EngineError> Presenter::set_present_mode(const vk::PresentModeKHR present_mode) {
    if (!_swapchain.has_value()) {
      spdlog::error("Cannot switch present mode before the swapchain is created");
      return std::unexpected(EngineError::SwapchainCreationFailed);
    }
    const std::vector<vk::PresentModeKHR> &compatible = _swapchain->compatible_present_modes();
    if (std::ranges::find(compatible, present_mode) == compatible.end()) {
      spdlog::warn("Present mode {} is not compatible with the active swapchain; recreation required",
                   vk::to_string(present_mode));
      return std::unexpected(EngineError::SwapchainCreationFailed);
    }
    _swapchain->set_present_mode(present_mode);
    return {};
  }

  std::expected<void, EngineError> Presenter::create_frame_semaphores(const vk::raii::Device &device,
                                                                      const std::uint32_t frames_in_flight) {
    _image_available_semaphores.clear();
    _image_available_semaphores.reserve(frames_in_flight);
    for (std::uint32_t frame = 0U; frame < frames_in_flight; ++frame) {
      std::expected<vk::raii::Semaphore, vk::Result> semaphore = device.createSemaphore(vk::SemaphoreCreateInfo{});
      if (!semaphore) {
        spdlog::error("Failed to create image-available semaphore: {}", vk::to_string(semaphore.error()));
        return std::unexpected(EngineError::FrameSyncCreationFailed);
      }
      _image_available_semaphores.push_back(std::move(*semaphore));
    }
    return {};
  }

  /**
   *  This is where frames-in-flight versus swapchain images becomes relevant.
   *  The frame_index we pass in here is the application notion - The application has decided to allow recording NUM_FRAMES ahead of the current frame,
   *  so frame_index is an index into the ring of buffered frames.
   *  Below, we'll also get image_index, which is the index of the swapchain image we'll actually render the frame into.
   *  This function is essentially assigning a swapchain image to a frame.
   */
  std::expected<AcquiredFrame, EngineError> Presenter::acquire_next_image(const vk::raii::Device &device, uint32_t frame_index) {
    if (!_swapchain.has_value()) {
      spdlog::error("Cannot acquire an image before the swapchain is created");
      return std::unexpected(EngineError::ImageAcquisitionFailed);
    }

    if (frame_index >= _image_available_semaphores.size()) {
      spdlog::error("Cannot acquire an image: frame slot {} has no image-available semaphore ({} allocated)",
                    frame_index, _image_available_semaphores.size());
      return std::unexpected(EngineError::ImageAcquisitionFailed);
    }

    const vk::Semaphore image_available = *_image_available_semaphores[frame_index];

    const vk::ResultValue<std::uint32_t> acquired =
        _swapchain->handle().acquireNextImage(std::numeric_limits<std::uint64_t>::max(), image_available);
    if (acquired.result == vk::Result::eErrorOutOfDateKHR) {
      spdlog::info("Swapchain out of date on acquire; recreating");
      if (std::expected<void, EngineError> recreated = recreate_for_current_surface(device); !recreated) {
        return std::unexpected(recreated.error());
      }
      return AcquiredFrame{.result = vk::Result::eErrorOutOfDateKHR};
    }
    if (acquired.result != vk::Result::eSuccess && acquired.result != vk::Result::eSuboptimalKHR) {
      spdlog::error("Failed to acquire swapchain image: {}", vk::to_string(acquired.result));
      return std::unexpected(EngineError::ImageAcquisitionFailed);
    }

    return AcquiredFrame{
        .result = acquired.result,
        .image_index = acquired.value,
        .image_available = image_available,
    };
  }

  std::expected<vk::Result, EngineError> Presenter::present(const vk::raii::Device &device,
                                                                const vk::raii::Queue &queue,
                                                                const std::uint32_t image_index) {
    if (!_swapchain.has_value()) {
      spdlog::error("Cannot present before the swapchain is created");
      return std::unexpected(EngineError::PresentFailed);
    }

    // Reap present fences of earlier presents that have completed so the active
    // swapchain's fence list stays bounded.
    std::erase_if(_present_fences, [&device](const vk::raii::Fence &present_fence) {
      const vk::Fence handle = *present_fence;
      return device.waitForFences(handle, VK_TRUE, 0U) == vk::Result::eSuccess;
    });

    prune_retired_swapchains(device);

    std::expected<vk::raii::Fence, vk::Result> fence = device.createFence(vk::FenceCreateInfo{});
    if (!fence) {
      spdlog::error("Failed to create present fence: {}", vk::to_string(fence.error()));
      return std::unexpected(EngineError::PresentFailed);
    }

    const vk::Semaphore render_finished = *_swapchain->render_finished(image_index);
    const vk::PresentModeKHR present_mode = _swapchain->present_mode();
    const vk::SwapchainPresentModeInfoKHR present_mode_info =
        vk::SwapchainPresentModeInfoKHR{}.setPresentModes(present_mode);
    const vk::SwapchainPresentFenceInfoKHR present_fence_info =
        vk::SwapchainPresentFenceInfoKHR{}.setFences(**fence).setPNext(&present_mode_info);

    const vk::PresentInfoKHR present_info = vk::PresentInfoKHR{}
        .setWaitSemaphores(render_finished)
        .setSwapchains(*_swapchain->handle())
        .setImageIndices(image_index)
        .setPNext(&present_fence_info);

    const vk::Result result = queue.presentKHR(present_info);
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR &&
        result != vk::Result::eErrorOutOfDateKHR) {
      spdlog::error("Failed to present swapchain image: {}", vk::to_string(result));
      return std::unexpected(EngineError::PresentFailed);
    }

    // The present was submitted, so the fence references the active swapchain
    // until it signals. Retain it (it must outlive this call) so it can be moved
    // to the RetiredSwapchain if the swapchain is replaced below.
    _present_fences.push_back(std::move(*fence));

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
      spdlog::info("Swapchain {} on present; recreating", vk::to_string(result));
      if (std::expected<void, EngineError> recreated = recreate_for_current_surface(device); !recreated) {
        return std::unexpected(recreated.error());
      }
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
