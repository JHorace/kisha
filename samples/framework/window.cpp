#include "window.hpp"

#include <utility>

#include <GLFW/glfw3.h>

namespace kisha::samples {
  struct WindowState {
    bool framebuffer_resized = false;
  };

  namespace {
    void framebuffer_size_callback(GLFWwindow *window, int /*width*/, int /*height*/) {
      if (auto *state = static_cast<WindowState *>(glfwGetWindowUserPointer(window))) {
        state->framebuffer_resized = true;
      }
    }
  }

  std::expected<GlfwContext, SampleError> GlfwContext::create() {
    if (glfwInit() != GLFW_TRUE) {
      return std::unexpected(SampleError::GlfwInitFailed);
    }
    return GlfwContext{true};
  }

  GlfwContext::GlfwContext(GlfwContext &&other) noexcept : _owns(std::exchange(other._owns, false)) {}

  GlfwContext &GlfwContext::operator=(GlfwContext &&other) noexcept {
    if (this != &other) {
      if (_owns) {
        glfwTerminate();
      }
      _owns = std::exchange(other._owns, false);
    }
    return *this;
  }

  GlfwContext::~GlfwContext() {
    if (_owns) {
      glfwTerminate();
    }
  }

  std::expected<Window, SampleError> Window::create(const WindowConfig &config) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, config.visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    GLFWwindow *handle = glfwCreateWindow(config.width, config.height,
                                          config.title.c_str(), nullptr, nullptr);
    if (handle == nullptr) {
      return std::unexpected(SampleError::WindowCreationFailed);
    }

    auto state = std::make_unique<WindowState>();
    glfwSetWindowUserPointer(handle, state.get());
    glfwSetFramebufferSizeCallback(handle, &framebuffer_size_callback);

    return Window{handle, std::move(state)};
  }

  Window::Window(GLFWwindow *window, std::unique_ptr<WindowState> state) noexcept
      : _window(window), _state(std::move(state)) {}

  Window::Window(Window &&other) noexcept
      : _window(std::exchange(other._window, nullptr)), _state(std::move(other._state)) {}

  Window &Window::operator=(Window &&other) noexcept {
    if (this != &other) {
      if (_window != nullptr) {
        glfwDestroyWindow(_window);
      }
      _window = std::exchange(other._window, nullptr);
      _state = std::move(other._state);
    }
    return *this;
  }

  Window::~Window() {
    if (_window != nullptr) {
      glfwDestroyWindow(_window);
    }
  }

  bool Window::should_close() const {
    return glfwWindowShouldClose(_window) != 0;
  }

  void Window::poll_events() const {
    glfwPollEvents();
  }

  FramebufferExtent Window::framebuffer_extent() const {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(_window, &width, &height);
    return FramebufferExtent{static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
  }

  bool Window::consume_framebuffer_resized() {
    if (_state && _state->framebuffer_resized) {
      _state->framebuffer_resized = false;
      return true;
    }
    return false;
  }
}
