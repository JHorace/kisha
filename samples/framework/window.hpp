#ifndef KISHA_SAMPLES_WINDOW_HPP
#define KISHA_SAMPLES_WINDOW_HPP

#include <cstdint>
#include <expected>
#include <memory>
#include <string>

#include "sample_error.hpp"

// GLFW's own opaque window type; kept as a forward declaration so that no GLFW
// header is pulled into framework consumers. All real GLFW usage is confined to
// window.cpp / native_handle.cpp.
struct GLFWwindow;

namespace kisha::samples {
  struct WindowConfig {
    std::string title = "kisha sample";
    int width = 1280;
    int height = 720;
    bool visible = true;
    bool resizable = true;
  };

  struct FramebufferExtent {
    std::uint32_t width = 0U;
    std::uint32_t height = 0U;
  };

  class GlfwContext {
  public:
    [[nodiscard]] static std::expected<GlfwContext, SampleError> create();

    GlfwContext(GlfwContext &&other) noexcept;
    GlfwContext &operator=(GlfwContext &&other) noexcept;
    GlfwContext(const GlfwContext &) = delete;
    GlfwContext &operator=(const GlfwContext &) = delete;
    ~GlfwContext();

  private:
    explicit GlfwContext(bool owns) : _owns(owns) {}
    bool _owns = false;
  };

  struct WindowState;

  class Window {
  public:
    [[nodiscard]] static std::expected<Window, SampleError> create(const WindowConfig &config);

    Window(Window &&other) noexcept;
    Window &operator=(Window &&other) noexcept;
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    ~Window();

    [[nodiscard]] bool should_close() const;
    void poll_events() const;
    [[nodiscard]] FramebufferExtent framebuffer_extent() const;
    [[nodiscard]] bool consume_framebuffer_resized();
    [[nodiscard]] GLFWwindow *handle() const { return _window; }

  private:
    Window(GLFWwindow *window, std::unique_ptr<WindowState> state) noexcept;
    GLFWwindow *_window = nullptr;
    std::unique_ptr<WindowState> _state;
  };
}

#endif //KISHA_SAMPLES_WINDOW_HPP
