#include "sample_app.hpp"

#include <utility>

#include <spdlog/spdlog.h>

#include "native_handle.hpp"

namespace kisha::samples {
  std::expected<SampleApp, SampleError> SampleApp::create(const AppConfig &config) {
    std::expected<GlfwContext, SampleError> glfw = GlfwContext::create();
    if (!glfw) {
      return std::unexpected(glfw.error());
    }

    const WindowConfig window_config{
        .title = config.title,
        .width = config.width,
        .height = config.height,
        .visible = config.visible,
        .resizable = true,
    };
    std::expected<Window, SampleError> window = Window::create(window_config);
    if (!window) {
      return std::unexpected(window.error());
    }

    std::expected<engine::NativeWindowHandle, SampleError> native = native_window_handle(*window);
    if (!native) {
      return std::unexpected(native.error());
    }

    engine::EngineCreateInfo create_info{};
    create_info.application_name = config.title;
    create_info.enable_validation = true;

    std::expected<engine::EngineCore, engine::EngineError> engine_core = engine::EngineCore::create(create_info);
    if (!engine_core) {
      spdlog::error("Sample engine init failed (engine error {})", static_cast<int>(engine_core.error()));
      return std::unexpected(SampleError::EngineInitFailed);
    }

    std::expected<engine::Presenter *, engine::EngineError> presenter = engine_core->create_presenter(*native);
    if (!presenter) {
      spdlog::error("Sample presenter creation failed (engine error {})", static_cast<int>(presenter.error()));
      return std::unexpected(SampleError::PresenterCreationFailed);
    }

    const FramebufferExtent extent = window->framebuffer_extent();
    engine::SwapchainConfig swapchain_config{};
    swapchain_config.extent = vk::Extent2D{extent.width, extent.height};

    if (std::expected<void, engine::EngineError> created =
            (*presenter)->create_swapchain(engine_core->device(), swapchain_config);
        !created) {
      spdlog::error("Sample swapchain creation failed (engine error {})", static_cast<int>(created.error()));
      return std::unexpected(SampleError::SwapchainCreationFailed);
    }

    return SampleApp{std::move(*glfw), std::move(*window), std::move(*engine_core), swapchain_config};
  }

  SampleApp::SampleApp(GlfwContext &&glfw, Window &&window, engine::EngineCore &&engine,
                       const engine::SwapchainConfig &swapchain_config) noexcept
      : _glfw(std::move(glfw)), _window(std::move(window)), _engine(std::move(engine)),
        _swapchain_config(swapchain_config) {}

  std::expected<void, SampleError> SampleApp::run(const DrawCallback &draw) {
    while (!_window.should_close()) {
      _window.poll_events();
      if (std::expected<void, SampleError> stepped = draw_frame(draw); !stepped) {
        return std::unexpected(stepped.error());
      }
    }
    wait_idle();
    return {};
  }

  std::expected<void, SampleError> SampleApp::run_frames(std::size_t frame_count, const DrawCallback &draw) {
    for (std::size_t frame = 0U; frame < frame_count && !_window.should_close(); ++frame) {
      _window.poll_events();
      if (std::expected<void, SampleError> stepped = draw_frame(draw); !stepped) {
        return std::unexpected(stepped.error());
      }
    }
    wait_idle();
    return {};
  }

  std::expected<void, SampleError> SampleApp::draw_frame(const DrawCallback &draw) {
    if (_window.consume_framebuffer_resized()) {
      if (std::expected<void, SampleError> recreated = recreate_swapchain(); !recreated) {
        return std::unexpected(recreated.error());
      }
    }

    std::expected<engine::FrameContext, engine::EngineError> frame = _engine.begin_frame();
    if (!frame) {
      if (frame.error() == engine::EngineError::ImageAcquisitionFailed) {
        if (std::expected<void, SampleError> recreated = recreate_swapchain(); !recreated) {
          return std::unexpected(recreated.error());
        }
        return {};
      }
      spdlog::error("begin_frame failed (engine error {})", static_cast<int>(frame.error()));
      return std::unexpected(SampleError::FrameFailed);
    }

    if (std::expected<void, engine::EngineError> drawn = draw(_engine, *frame); !drawn) {
      spdlog::error("Draw callback failed (engine error {})", static_cast<int>(drawn.error()));
      return std::unexpected(SampleError::FrameFailed);
    }

    if (std::expected<void, engine::EngineError> ended = _engine.end_frame(std::move(*frame)); !ended) {
      spdlog::error("end_frame failed (engine error {})", static_cast<int>(ended.error()));
      return std::unexpected(SampleError::FrameFailed);
    }

    return {};
  }

  std::expected<void, SampleError> SampleApp::recreate_swapchain() {
    engine::Presenter *presenter = _engine.presenter();
    if (presenter == nullptr) {
      return std::unexpected(SampleError::SwapchainCreationFailed);
    }

    const FramebufferExtent extent = _window.framebuffer_extent();
    if (extent.width == 0U || extent.height == 0U) {
      return {};
    }

    wait_idle();
    _swapchain_config.extent = vk::Extent2D{extent.width, extent.height};
    if (std::expected<void, engine::EngineError> recreated =
            presenter->recreate_swapchain(_engine.device(), _swapchain_config);
        !recreated) {
      spdlog::error("Swapchain recreation failed (engine error {})", static_cast<int>(recreated.error()));
      return std::unexpected(SampleError::SwapchainCreationFailed);
    }
    return {};
  }

  void SampleApp::wait_idle() {
    if (const std::expected<void, vk::Result> result = _engine.device().waitIdle(); !result) {
      spdlog::warn("Device waitIdle returned {}", vk::to_string(result.error()));
    }
  }
}
