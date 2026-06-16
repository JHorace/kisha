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

  struct SwapchainConfig {
    vk::Extent2D extent{};
    vk::SurfaceFormatKHR surface_format{vk::Format::eB8G8R8A8Srgb, vk::ColorSpaceKHR::eSrgbNonlinear};
    vk::PresentModeKHR present_mode = vk::PresentModeKHR::eFifo;
    std::uint32_t min_image_count = 0U;
    vk::ImageUsageFlags image_usage = vk::ImageUsageFlagBits::eColorAttachment;
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

    [[nodiscard]] std::expected<void, EngineInitError> create_swapchain(const vk::raii::Device &device,
                                                                        const SwapchainConfig &config);
    [[nodiscard]] std::expected<void, EngineInitError> recreate_swapchain(const vk::raii::Device &device,
                                                                          const SwapchainConfig &config);

    // Set present mode w/o recreating the swapchain. Totally unnecessary, but easily enabled w/ VK_EXT_swapchain_maintenance1 and surface_maintenance1 so why not
    [[nodiscard]] std::expected<void, EngineInitError> set_present_mode(vk::PresentModeKHR present_mode);

    [[nodiscard]] const vk::raii::SwapchainKHR &swapchain() const { return _swapchain; }
    [[nodiscard]] const std::vector<vk::Image> &swapchain_images() const { return _swapchain_images; }
    [[nodiscard]] vk::Format swapchain_format() const { return _swapchain_format; }
    [[nodiscard]] vk::Extent2D swapchain_extent() const { return _swapchain_extent; }
    [[nodiscard]] vk::PresentModeKHR present_mode() const { return _present_mode; }
    [[nodiscard]] const std::vector<vk::PresentModeKHR> &compatible_present_modes() const {
      return _compatible_present_modes;
    }
  private:
    friend class EngineCore;

    Presenter(vk::raii::SurfaceKHR &&surface, vk::raii::PhysicalDevice physical_device,
              std::uint32_t present_queue_family)
        : _surface(std::move(surface)), _physical_device(std::move(physical_device)),
          _present_queue_family(present_queue_family) {}

    // Presenter owns the surface it presents to
    vk::raii::SurfaceKHR _surface{nullptr};
    vk::raii::SwapchainKHR _swapchain{nullptr};
    std::vector<vk::raii::SwapchainKHR> _retired_swapchains;
    std::vector<vk::Image> _swapchain_images;
    vk::Format _swapchain_format = vk::Format::eUndefined;
    vk::Extent2D _swapchain_extent{};
    vk::PresentModeKHR _present_mode = vk::PresentModeKHR::eFifo;
    std::vector<vk::PresentModeKHR> _compatible_present_modes;
    // Presenter doesn't own the physical device
    // these aren't real handles, so this is valid as long as the Presenter doesn't outlive the instance.
    vk::raii::PhysicalDevice _physical_device{nullptr};
    std::uint32_t _present_queue_family = 0U;
  };
}

#endif //KISHA_PRESENTER_HPP
