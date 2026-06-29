#include <cstdlib>
#include <expected>
#include <utility>

#include <spdlog/spdlog.h>

#include "engine.hpp"
#include "sample_app.hpp"
#include "spirv_loader.hpp"

namespace {
  using kisha::samples::AppConfig;
  using kisha::samples::SampleApp;
  using kisha::samples::SampleError;

  kisha::samples::DrawCallback make_triangle_draw(kisha::engine::ShaderProgramHandle program) {
    return [program](kisha::engine::EngineCore & /*engine*/, kisha::engine::FrameContext &frame)
               -> std::expected<void, kisha::engine::EngineError> {
      kisha::engine::PassDescription pass{};

      pass.image_accesses.push_back(kisha::engine::ImageAccess{
          .image = frame.swapchain_image(),
          .layout = vk::ImageLayout::eColorAttachmentOptimal,
          .stage = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
          .access = vk::AccessFlagBits2::eColorAttachmentWrite,
      });
      pass.color = kisha::engine::ColorAttachmentDescription{
          .target = frame.swapchain_image(),
          .clear_color = {0.0F, 0.0F, 0.05F, 1.0F},
      };
      pass.draws.push_back(kisha::engine::DrawItem{.program = program, .vertex_count = 3U});
      frame.add_pass(std::move(pass));
      return {};
    };
  }

  int run() {
    std::expected<std::vector<std::uint32_t>, SampleError> vertex_spirv =
        kisha::samples::load_spirv(KISHA_TRIANGLE_VERT_SPV);
    if (!vertex_spirv) {
      spdlog::error("Failed to load triangle vertex SPIR-V '{}': {}", KISHA_TRIANGLE_VERT_SPV,
                    kisha::samples::describe(vertex_spirv.error()));
      return EXIT_FAILURE;
    }

    std::expected<std::vector<std::uint32_t>, SampleError> fragment_spirv =
        kisha::samples::load_spirv(KISHA_TRIANGLE_FRAG_SPV);
    if (!fragment_spirv) {
      spdlog::error("Failed to load triangle fragment SPIR-V '{}': {}", KISHA_TRIANGLE_FRAG_SPV,
                    kisha::samples::describe(fragment_spirv.error()));
      return EXIT_FAILURE;
    }

    AppConfig config{};
    config.title = "kisha - windowed triangle";
    std::expected<SampleApp, SampleError> app = SampleApp::create(config);
    if (!app) {
      spdlog::error("Failed to create the sample app: {}", kisha::samples::describe(app.error()));
      return EXIT_FAILURE;
    }

    const kisha::engine::ShaderProgramDescription description{
        .vertex_spirv = *vertex_spirv,
        .fragment_spirv = *fragment_spirv,
    };
    std::expected<kisha::engine::ShaderProgramHandle, kisha::engine::EngineError> program =
        app->engine().create_shader_program(description);
    if (!program) {
      spdlog::error("Failed to create the triangle shader program (engine error {})",
                    static_cast<int>(program.error()));
      return EXIT_FAILURE;
    }

    if (std::expected<void, SampleError> result = app->run(make_triangle_draw(*program)); !result) {
      spdlog::error("Sample run failed: {}", kisha::samples::describe(result.error()));
      return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
  }
}

int main() {
  return run();
}
