/**
 * @file presenter.hpp
 * @brief Presentation surface ownership and surface-capability queries.
 */

#ifndef KISHA_PRESENTER_HPP
#define KISHA_PRESENTER_HPP

#include <vulkan/vulkan_raii.hpp>
#include <expected>
#include <variant>

#include "errors.hpp"

namespace kisha::engine {
  class EngineCore;

#ifdef VK_USE_PLATFORM_WAYLAND_KHR
  struct WaylandWindowHandle {
    wl_display *display = nullptr;
    wl_surface *surface = nullptr;
  };
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
  struct XcbWindowHandle {
    xcb_connection_t *connection = nullptr;
    xcb_window_t window{};
  };
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
  struct XlibWindowHandle {
    Display *display = nullptr;
    Window window{};
  };
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
  struct Win32WindowHandle {
    HINSTANCE hinstance = nullptr;
    HWND hwnd = nullptr;
  };
#endif

  /**
   * @brief A native window handle the engine can create a surface from.
   * std::monostate represents a headless window
   */
  using NativeWindowHandle = std::variant<
      std::monostate
#ifdef VK_USE_PLATFORM_WAYLAND_KHR
      , WaylandWindowHandle
#endif
#ifdef VK_USE_PLATFORM_XCB_KHR
      , XcbWindowHandle
#endif
#ifdef VK_USE_PLATFORM_XLIB_KHR
      , XlibWindowHandle
#endif
#ifdef VK_USE_PLATFORM_WIN32_KHR
      , Win32WindowHandle
#endif
      >;

  struct PresentModeCapabilities {
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    std::uint32_t min_image_count = 0U;
    std::uint32_t max_image_count = 0U;
    std::vector<vk::PresentModeKHR> compatible_present_modes;
    vk::PresentScalingFlagsKHR supported_scaling{};
    vk::PresentGravityFlagsKHR supported_gravity_x{};
    vk::PresentGravityFlagsKHR supported_gravity_y{};
    vk::Extent2D min_scaled_image_extent{};
    vk::Extent2D max_scaled_image_extent{};
  };

  struct SurfaceCapabilities {
    vk::SurfaceCapabilitiesKHR capabilities{};
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
    std::vector<PresentModeCapabilities> per_present_mode;
  };

  /**
   * @brief Owns the engine-created presentation surface, and eventually the swapchain.
   */
  class Presenter {
  public:
    Presenter(Presenter &&other) noexcept = default;
    Presenter &operator=(Presenter &&other) noexcept = default;

    Presenter(const Presenter &) = delete;
    Presenter &operator=(const Presenter &) = delete;

    [[nodiscard]] const vk::raii::SurfaceKHR &surface() const { return _surface; }
    [[nodiscard]] std::expected<SurfaceCapabilities, EngineInitError> capabilities() const;
  private:
    friend class EngineCore;

    Presenter(vk::raii::SurfaceKHR &&surface, vk::raii::PhysicalDevice physical_device)
        : _surface(std::move(surface)), _physical_device(std::move(physical_device)) {}

    // Presenter owns the surface it presents to
    vk::raii::SurfaceKHR _surface{nullptr};
    // ...it doesn't own the physical device
    // these aren't real handles, so this is valid as long as the Presenter doesn't outlive the instance.
    vk::raii::PhysicalDevice _physical_device{nullptr};
  };
}

#endif //KISHA_PRESENTER_HPP
