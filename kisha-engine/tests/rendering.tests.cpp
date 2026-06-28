#include <array>
#include <cstdint>
#include <expected>
#include <catch2/catch_test_macros.hpp>

#include "rendering.hpp"

// These tests exercise only the declarative rendering API surface and its
// documented defaults. They are headless ([core]) and create no Vulkan objects.

TEST_CASE("Rendering resource handles default to the reserved 'invalid' id", "[engine][core]") {
  const kisha::engine::PipelineHandle pipeline{};
  const kisha::engine::ImageHandle image{};

  REQUIRE(pipeline.id == 0U);
  REQUIRE(image.id == 0U);
}

TEST_CASE("GraphicsPipelineDescription exposes documented defaults", "[engine][core]") {
  const kisha::engine::GraphicsPipelineDescription description{};

  REQUIRE(description.vertex_spirv.empty());
  REQUIRE(description.fragment_spirv.empty());
  REQUIRE(description.color_format == vk::Format::eUndefined);
  REQUIRE(description.topology == vk::PrimitiveTopology::eTriangleList);
}

TEST_CASE("ImageAccess defaults declare a no-op access on an invalid image", "[engine][core]") {
  const kisha::engine::ImageAccess access{};

  REQUIRE(access.image.id == 0U);
  REQUIRE(access.layout == vk::ImageLayout::eUndefined);
  REQUIRE(access.stage == vk::PipelineStageFlagBits2::eNone);
  REQUIRE(access.access == vk::AccessFlagBits2::eNone);
}

TEST_CASE("ColorAttachmentDescription defaults to an opaque-black clear", "[engine][core]") {
  const kisha::engine::ColorAttachmentDescription color{};

  REQUIRE(color.target.id == 0U);
  REQUIRE(color.clear_color == std::array<float, 4>{0.F, 0.F, 0.F, 1.F});
}

TEST_CASE("DrawItem defaults to a three-vertex draw on an invalid pipeline", "[engine][core]") {
  const kisha::engine::DrawItem draw{};

  REQUIRE(draw.pipeline.id == 0U);
  REQUIRE(draw.vertex_count == 3U);
}

TEST_CASE("PassDescription and FrameDescription start empty", "[engine][core]") {
  const kisha::engine::PassDescription pass{};
  REQUIRE(pass.image_accesses.empty());
  REQUIRE(pass.draws.empty());
  // The default-constructed color target references the invalid image.
  REQUIRE(pass.color.target.id == 0U);

  const kisha::engine::FrameDescription frame{};
  REQUIRE(frame.passes.empty());
}

TEST_CASE("ImageState defaults to the start-of-pipeline undefined state", "[engine][core]") {
  const kisha::engine::ImageState state{};

  REQUIRE(state.layout == vk::ImageLayout::eUndefined);
  REQUIRE(state.stage == vk::PipelineStageFlagBits2::eTopOfPipe);
  REQUIRE(state.access == vk::AccessFlagBits2::eNone);
}

TEST_CASE("FrameContext starts with an invalid swapchain image and empty description", "[engine][core]") {
  kisha::engine::FrameContext context{};

  REQUIRE(context.swapchain_image().id == 0U);
  REQUIRE(context.description().passes.empty());
}

TEST_CASE("FrameContext convenience builders create a single lazy default pass", "[engine][core]") {
  kisha::engine::FrameContext context{};

  // The first convenience call lazily creates one default pass; subsequent
  // convenience calls reuse that same pass rather than creating new ones.
  context.set_color_target(kisha::engine::ColorAttachmentDescription{kisha::engine::ImageHandle{7U}});
  context.add_draw(kisha::engine::DrawItem{kisha::engine::PipelineHandle{3U}, 3U});

  REQUIRE(context.description().passes.size() == 1U);
  REQUIRE(context.description().passes.front().color.target.id == 7U);
  REQUIRE(context.description().passes.front().draws.size() == 1U);
  REQUIRE(context.description().passes.front().draws.front().pipeline.id == 3U);
}

TEST_CASE("FrameContext::add_pass appends passes in order", "[engine][core]") {
  kisha::engine::FrameContext context{};

  kisha::engine::PassDescription first{};
  first.color.target = kisha::engine::ImageHandle{1U};
  kisha::engine::PassDescription second{};
  second.color.target = kisha::engine::ImageHandle{2U};

  context.add_pass(first);
  context.add_pass(second);

  REQUIRE(context.description().passes.size() == 2U);
  REQUIRE(context.description().passes[0].color.target.id == 1U);
  REQUIRE(context.description().passes[1].color.target.id == 2U);
}

TEST_CASE("FrameContext::raw() is reserved and returns NotImplemented", "[engine][core]") {
  kisha::engine::FrameContext context{};

  const std::expected<kisha::engine::FrameRecorder *, kisha::engine::EngineError> raw = context.raw();

  REQUIRE_FALSE(raw.has_value());
  REQUIRE(raw.error() == kisha::engine::EngineError::NotImplemented);
}
