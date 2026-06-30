/**
 * @file presenter.hpp
 * @brief Presentation surface ownership and surface-capability queries.
 */

#ifndef KISHA_PRESENTER_HPP
#define KISHA_PRESENTER_HPP

#include <vulkan/vulkan_raii.hpp>
#include <expected>
#include <optional>
#include <variant>

#include "errors.hpp"
#include "swapchain.hpp"

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

  struct AcquiredFrame {
    vk::Result result = vk::Result::eErrorOutOfDateKHR;
    std::uint32_t image_index = 0U;
    vk::Semaphore image_available{};
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

    [[nodiscard]] static std::expected<Presenter, EngineError> create(vk::raii::SurfaceKHR &&surface,
                                                                       vk::raii::PhysicalDevice physical_device,
                                                                       std::uint32_t present_queue_family,
                                                                       const vk::raii::Device &device,
                                                                       std::uint32_t frames_in_flight);

    [[nodiscard]] const vk::raii::SurfaceKHR &surface() const { return _surface; }
    [[nodiscard]] std::expected<SurfaceCapabilities, EngineError> capabilities() const;

    [[nodiscard]] std::expected<void, EngineError> create_swapchain(const vk::raii::Device &device,
                                                                        const SwapchainConfig &config);
    [[nodiscard]] std::expected<void, EngineError> recreate_swapchain(const vk::raii::Device &device,
                                                                          const SwapchainConfig &config);

    // Set present mode w/o recreating the swapchain. Totally unnecessary, but easily enabled w/ VK_EXT_swapchain_maintenance1 and surface_maintenance1 so why not
    [[nodiscard]] std::expected<void, EngineError> set_present_mode(vk::PresentModeKHR present_mode);

    [[nodiscard]] std::expected<AcquiredFrame, EngineError> acquire_next_image(const vk::raii::Device &device, uint32_t frame_index);

    [[nodiscard]] std::expected<vk::Result, EngineError> present(const vk::raii::Device &device,
                                                                     const vk::raii::Queue &queue,
                                                                     std::uint32_t image_index);

    std::size_t prune_retired_swapchains(const vk::raii::Device &device);
    [[nodiscard]] std::size_t retired_swapchain_count() const { return _retired_swapchains.size(); }
    [[nodiscard]] bool has_swapchain() const { return _swapchain.has_value(); }
    [[nodiscard]] const Swapchain &swapchain() const { return *_swapchain; }
    [[nodiscard]] const vk::raii::SwapchainKHR &swapchain_handle() const { return _swapchain->handle(); }
    [[nodiscard]] const std::vector<vk::Image> &swapchain_images() const { return _swapchain->images(); }
    [[nodiscard]] vk::Format swapchain_format() const { return _swapchain->format(); }
    [[nodiscard]] vk::Extent2D swapchain_extent() const { return _swapchain->extent(); }
    [[nodiscard]] vk::PresentModeKHR present_mode() const { return _swapchain->present_mode(); }
    [[nodiscard]] const std::vector<vk::PresentModeKHR> &compatible_present_modes() const {
      return _swapchain->compatible_present_modes();
    }
  private:
    friend class EngineCore;

    Presenter(vk::raii::SurfaceKHR &&surface, vk::raii::PhysicalDevice physical_device,
              std::uint32_t present_queue_family)
        : _surface(std::move(surface)), _physical_device(std::move(physical_device)),
          _present_queue_family(present_queue_family) {}

    // A swapchain that has been replaced (resize/format/present-mode change) but
    // cannot be destroyed yet: it is kept alive together with the present fences
    // of the in-flight presents that referenced it, until those fences signal.
    struct RetiredSwapchain {
      Swapchain swapchain;
      std::vector<vk::raii::Fence> present_fences;
    };

    [[nodiscard]] std::expected<void, EngineError> recreate_for_current_surface(const vk::raii::Device &device);

    [[nodiscard]] std::expected<void, EngineError> create_frame_semaphores(const vk::raii::Device &device,
                                                                           std::uint32_t frames_in_flight);
    // Presenter owns the surface it presents to
    vk::raii::SurfaceKHR _surface{nullptr};
    std::optional<Swapchain> _swapchain;
    SwapchainConfig _active_config;
    std::vector<vk::raii::Semaphore> _image_available_semaphores;
    std::vector<vk::raii::Fence> _present_fences;
    std::vector<RetiredSwapchain> _retired_swapchains;
    // Presenter doesn't own the physical device
    // these aren't real handles, so this is valid as long as the Presenter doesn't outlive the instance.
    vk::raii::PhysicalDevice _physical_device{nullptr};
    std::uint32_t _present_queue_family = 0U;
  };
}

#endif //KISHA_PRESENTER_HPP
