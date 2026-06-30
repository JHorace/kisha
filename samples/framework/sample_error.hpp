#ifndef KISHA_SAMPLES_SAMPLE_ERROR_HPP
#define KISHA_SAMPLES_SAMPLE_ERROR_HPP

#include <cstdint>
#include <string_view>

namespace kisha::samples {
  enum class SampleError : std::uint8_t {
    Unknown = 0U,
    GlfwInitFailed,
    WindowCreationFailed,
    InvalidWindow,
    UnsupportedPlatform,
    NativeHandleUnavailable,
    FileNotFound,
    FileReadFailed,
    InvalidSpirv,
    EngineInitFailed,
    PresenterCreationFailed,
    SwapchainCreationFailed,
    FrameFailed,
  };

  [[nodiscard]] constexpr std::string_view describe(SampleError error) noexcept {
    switch (error) {
      case SampleError::GlfwInitFailed:
        return "Failed to initialize GLFW";
      case SampleError::WindowCreationFailed:
        return "Failed to create the window";
      case SampleError::InvalidWindow:
        return "The window handle is null";
      case SampleError::UnsupportedPlatform:
        return "The running platform has no compiled native-handle variant";
      case SampleError::NativeHandleUnavailable:
        return "The windowing library returned a null native handle";
      case SampleError::FileNotFound:
        return "The requested file does not exist";
      case SampleError::FileReadFailed:
        return "Failed to read the requested file";
      case SampleError::InvalidSpirv:
        return "The file is not a valid SPIR-V module";
      case SampleError::EngineInitFailed:
        return "Failed to initialize the engine core";
      case SampleError::PresenterCreationFailed:
        return "Failed to create the engine presenter/surface";
      case SampleError::SwapchainCreationFailed:
        return "Failed to create or recreate the swapchain";
      case SampleError::FrameFailed:
        return "A frame failed to render";
      case SampleError::Unknown:
        break;
    }
    return "Unknown sample error";
  }
}

#endif //KISHA_SAMPLES_SAMPLE_ERROR_HPP
