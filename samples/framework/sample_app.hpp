#ifndef KISHA_SAMPLES_SAMPLE_APP_HPP
#define KISHA_SAMPLES_SAMPLE_APP_HPP

#include <cstddef>
#include <expected>
#include <functional>
#include <string>

#include "engine.hpp"
#include "sample_error.hpp"
#include "window.hpp"

namespace kisha::samples {
  struct AppConfig {
    std::string title = "kisha sample";
    int width = 1280;
    int height = 720;
    bool visible = true;
  };

  using DrawCallback =
      std::function<std::expected<void, engine::EngineError>(engine::EngineCore &, engine::FrameContext &)>;

  class SampleApp {
  public:
    [[nodiscard]] static std::expected<SampleApp, SampleError> create(const AppConfig &config);

    SampleApp(SampleApp &&other) noexcept = default;
    SampleApp &operator=(SampleApp &&other) noexcept = default;
    SampleApp(const SampleApp &) = delete;
    SampleApp &operator=(const SampleApp &) = delete;
    ~SampleApp() = default;

    [[nodiscard]] std::expected<void, SampleError> run(const DrawCallback &draw);
    [[nodiscard]] std::expected<void, SampleError> run_frames(std::size_t frame_count, const DrawCallback &draw);

    [[nodiscard]] engine::EngineCore &engine() { return _engine; }
    [[nodiscard]] const engine::EngineCore &engine() const { return _engine; }
    [[nodiscard]] Window &window() { return _window; }
    [[nodiscard]] const Window &window() const { return _window; }

  private:
    SampleApp(GlfwContext &&glfw, Window &&window, engine::EngineCore &&engine,
              const engine::SwapchainConfig &swapchain_config) noexcept;

    [[nodiscard]] std::expected<void, SampleError> draw_frame(const DrawCallback &draw);
    [[nodiscard]] std::expected<void, SampleError> recreate_swapchain();
    void wait_idle();

    GlfwContext _glfw;
    Window _window;
    engine::EngineCore _engine;
    engine::SwapchainConfig _swapchain_config{};
  };
}

#endif //KISHA_SAMPLES_SAMPLE_APP_HPP
