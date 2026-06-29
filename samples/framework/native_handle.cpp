#include "native_handle.hpp"

#include "window.hpp"

// Pull in the platform window types (Display/Window, wl_display/wl_surface,
// HINSTANCE/HWND) before glfw3native.h so the native accessors resolve. These
// are the same VK_USE_PLATFORM_* gates the engine compiles its handle variants
// behind, propagated through the kisha::engine link.
#include <vulkan/vulkan_raii.hpp>

#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
#define GLFW_EXPOSE_NATIVE_X11
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
#define GLFW_EXPOSE_NATIVE_WIN32
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace kisha::samples {
  std::expected<engine::NativeWindowHandle, SampleError>
  native_window_handle(const Window &window) {
    GLFWwindow *handle = window.handle();
    if (handle == nullptr) {
      return std::unexpected(SampleError::InvalidWindow);
    }

    switch (glfwGetPlatform()) {
#if defined(VK_USE_PLATFORM_WAYLAND_KHR)
      case GLFW_PLATFORM_WAYLAND: {
        engine::WaylandWindowHandle native{};
        native.display = glfwGetWaylandDisplay();
        native.surface = glfwGetWaylandWindow(handle);
        if (native.display == nullptr || native.surface == nullptr) {
          return std::unexpected(SampleError::NativeHandleUnavailable);
        }
        return engine::NativeWindowHandle{native};
      }
#endif
#if defined(VK_USE_PLATFORM_XLIB_KHR)
      case GLFW_PLATFORM_X11: {
        engine::XlibWindowHandle native{};
        native.display = glfwGetX11Display();
        native.window = glfwGetX11Window(handle);
        if (native.display == nullptr || native.window == 0) {
          return std::unexpected(SampleError::NativeHandleUnavailable);
        }
        return engine::NativeWindowHandle{native};
      }
#endif
#if defined(VK_USE_PLATFORM_WIN32_KHR)
      case GLFW_PLATFORM_WIN32: {
        engine::Win32WindowHandle native{};
        native.hinstance = GetModuleHandle(nullptr);
        native.hwnd = glfwGetWin32Window(handle);
        if (native.hwnd == nullptr) {
          return std::unexpected(SampleError::NativeHandleUnavailable);
        }
        return engine::NativeWindowHandle{native};
      }
#endif
      default:
        break;
    }

    return std::unexpected(SampleError::UnsupportedPlatform);
  }
}
