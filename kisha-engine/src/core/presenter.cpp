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
  std::expected<SurfaceCapabilities, EngineInitError> Presenter::capabilities() const {
    return util::query_surface_capabilities(_physical_device, _surface);
  }

  std::expected<void, EngineInitError> Presenter::create_swapchain(const vk::raii::Device &device,
                                                                   const SwapchainConfig &config) {
    if (_swapchain.has_value()) {
      return recreate_swapchain(device, config);
    }

    return Swapchain::create(device, _physical_device, _surface, _present_queue_family, config)
        .and_then([this, &device, &config](Swapchain swapchain) -> std::expected<void, EngineInitError> {
          _swapchain = std::move(swapchain);
          _active_config = config;
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
        .and_then([this, &device, &config](Swapchain swapchain) -> std::expected<void, EngineInitError> {
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

  std::expected<void, EngineInitError> Presenter::recreate_for_current_surface(const vk::raii::Device &device) {
    return recreate_swapchain(device, _active_config);
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

  std::expected<AcquiredFrame, EngineInitError> Presenter::acquire_next_image(const vk::raii::Device &device) {
    if (!_swapchain.has_value() || !_frame_context.has_value()) {
      spdlog::error("Cannot acquire an image before the swapchain is created");
      return std::unexpected(EngineInitError::ImageAcquisitionFailed);
    }

    const std::uint32_t frame = _frame_context->current_frame();
    const vk::Fence in_flight = *_frame_context->in_flight(frame);
    const vk::Semaphore image_available = *_frame_context->image_available(frame);

    // Block until the previous use of this frame has completed.
    if (const vk::Result wait =
            device.waitForFences(in_flight, VK_TRUE, std::numeric_limits<std::uint64_t>::max());
        wait != vk::Result::eSuccess) {
      spdlog::error("Failed to wait on in-flight fence: {}", vk::to_string(wait));
      return std::unexpected(EngineInitError::ImageAcquisitionFailed);
    }

    const vk::ResultValue<std::uint32_t> acquired =
        _swapchain->handle().acquireNextImage(std::numeric_limits<std::uint64_t>::max(), image_available);
    const vk::Result result = acquired.result;
    if (result == vk::Result::eErrorOutOfDateKHR) {
      spdlog::info("Swapchain out of date on acquire; recreating");
      if (std::expected<void, EngineInitError> recreated = recreate_for_current_surface(device); !recreated) {
        return std::unexpected(recreated.error());
      }
      return AcquiredFrame{.result = vk::Result::eErrorOutOfDateKHR};
    }
    if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR) {
      spdlog::error("Failed to acquire swapchain image: {}", vk::to_string(result));
      return std::unexpected(EngineInitError::ImageAcquisitionFailed);
    }

    const std::uint32_t image_index = acquired.value;

    if (const std::expected<void, vk::Result> reset = device.resetFences(in_flight); !reset) {
      spdlog::error("Failed to reset in-flight fence: {}", vk::to_string(reset.error()));
      return std::unexpected(EngineInitError::ImageAcquisitionFailed);
    }

    _frame_context->set_current_image_index(image_index);
    return AcquiredFrame{
        .result = result,
        .image_index = image_index,
        .image_available = image_available,
        .render_finished = *_swapchain->render_finished(image_index),
        .in_flight = in_flight,
    };
  }

  std::expected<vk::Result, EngineInitError> Presenter::present(const vk::raii::Device &device,
                                                                const vk::raii::Queue &queue,
                                                                const std::uint32_t image_index) {
    if (!_swapchain.has_value() || !_frame_context.has_value()) {
      spdlog::error("Cannot present before the swapchain is created");
      return std::unexpected(EngineInitError::PresentFailed);
    }

    prune_signaled_present_fences(device);

    std::expected<vk::raii::Fence, vk::Result> fence = device.createFence(vk::FenceCreateInfo{});
    if (!fence) {
      spdlog::error("Failed to create present fence: {}", vk::to_string(fence.error()));
      return std::unexpected(EngineInitError::PresentFailed);
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
      return std::unexpected(EngineInitError::PresentFailed);
    }

    if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
      _present_fences.push_back(std::move(*fence));
    }
    _frame_context->advance_frame();

    if (result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR) {
      spdlog::info("Swapchain {} on present; recreating", vk::to_string(result));
      if (std::expected<void, EngineInitError> recreated = recreate_for_current_surface(device); !recreated) {
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
