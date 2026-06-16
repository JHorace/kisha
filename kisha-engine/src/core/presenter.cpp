/**
 * @file presenter.cpp
 * @brief Presenter surface-capability query implementation.
 */
#include "presenter.hpp"
#include "engine_init_helpers.hpp"

namespace kisha::engine {
  std::expected<SurfaceCapabilities, EngineInitError> Presenter::capabilities() const {
    return util::query_surface_capabilities(_physical_device, _surface);
  }
}
