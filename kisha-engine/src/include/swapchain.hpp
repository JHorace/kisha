#ifndef KISHA_SWAPCHAIN_HPP
#define KISHA_SWAPCHAIN_HPP

#include <vulkan/vulkan_raii.hpp>
#include <cstdint>
#include <expected>
#include <vector>

#include "errors.hpp"

namespace kisha::engine {
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

  class Swapchain {
  public:
    Swapchain(Swapchain &&other) noexcept = default;
    Swapchain &operator=(Swapchain &&other) noexcept = default;

    Swapchain(const Swapchain &) = delete;
    Swapchain &operator=(const Swapchain &) = delete;

    [[nodiscard]] static std::expected<Swapchain, EngineInitError> create(
        const vk::raii::Device &device, const vk::raii::PhysicalDevice &physical_device,
        const vk::raii::SurfaceKHR &surface, std::uint32_t present_queue_family, const SwapchainConfig &config,
        vk::SwapchainKHR old_swapchain = VK_NULL_HANDLE);

    [[nodiscard]] const vk::raii::SwapchainKHR &handle() const { return _swapchain; }
    [[nodiscard]] const std::vector<vk::Image> &images() const { return _images; }
    [[nodiscard]] const std::vector<vk::raii::ImageView> &image_views() const { return _image_views; }
    [[nodiscard]] vk::Format format() const { return _format; }
    [[nodiscard]] vk::Extent2D extent() const { return _extent; }
    [[nodiscard]] vk::PresentModeKHR present_mode() const { return _present_mode; }
    [[nodiscard]] const std::vector<vk::PresentModeKHR> &compatible_present_modes() const {
      return _compatible_present_modes;
    }

    void set_present_mode(vk::PresentModeKHR present_mode) { _present_mode = present_mode; }

  private:
    Swapchain() = default;

    vk::raii::SwapchainKHR _swapchain{nullptr};
    std::vector<vk::Image> _images;
    std::vector<vk::raii::ImageView> _image_views;
    vk::Format _format = vk::Format::eUndefined;
    vk::Extent2D _extent{};
    vk::PresentModeKHR _present_mode = vk::PresentModeKHR::eFifo;
    std::vector<vk::PresentModeKHR> _compatible_present_modes;
  };
}

#endif //KISHA_SWAPCHAIN_HPP
