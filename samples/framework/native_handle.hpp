#ifndef KISHA_SAMPLES_NATIVE_HANDLE_HPP
#define KISHA_SAMPLES_NATIVE_HANDLE_HPP

#include <expected>

#include "presenter.hpp"
#include "sample_error.hpp"

namespace kisha::samples {
  class Window;

  /**
   * @brief Build the engine's NativeWindowHandle for the given window.
   *
   * Selects the platform variant at runtime via the windowing library and fills
   * the matching kisha::engine::NativeWindowHandle alternative. Returns a
   * SampleError when the running platform has no compiled variant or the
   * library reports a null native handle. The engine still owns surface
   * creation; this only hands it the raw handles.
   */
  [[nodiscard]] std::expected<engine::NativeWindowHandle, SampleError>
  native_window_handle(const Window &window);
}

#endif //KISHA_SAMPLES_NATIVE_HANDLE_HPP
