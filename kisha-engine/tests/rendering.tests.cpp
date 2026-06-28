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

// --- Step 2: ResourceStateTracker diff + minimal synchronization2 barriers ---
//
// These remain headless ([core]): the tracker never touches a device, so we
// register images with a null vk::Image and only assert on the diffed
// layout/stage/access carried by the emitted vk::ImageMemoryBarrier2.

namespace {
  // A tracker holding one image registered at the default ImageState
  // (eUndefined / eTopOfPipe / eNone), matching a freshly-acquired swapchain image.
  kisha::engine::ResourceStateTracker make_tracker_with_image(const kisha::engine::ImageHandle handle) {
    kisha::engine::ResourceStateTracker tracker;
    tracker.register_image(handle, vk::Image{}, kisha::engine::ImageState{});
    return tracker;
  }
}

TEST_CASE("ResourceStateTracker emits a barrier when the requested access differs", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  const std::optional<vk::ImageMemoryBarrier2> barrier = tracker.transition(
      kisha::engine::ImageAccess{handle, vk::ImageLayout::eColorAttachmentOptimal,
                                 vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                 vk::AccessFlagBits2::eColorAttachmentWrite});

  REQUIRE(barrier.has_value());
  // The barrier carries the diff from the last-known state to the requested one.
  REQUIRE(barrier->oldLayout == vk::ImageLayout::eUndefined);
  REQUIRE(barrier->newLayout == vk::ImageLayout::eColorAttachmentOptimal);
  REQUIRE(barrier->srcStageMask == vk::PipelineStageFlagBits2::eTopOfPipe);
  REQUIRE(barrier->srcAccessMask == vk::AccessFlagBits2::eNone);
  REQUIRE(barrier->dstStageMask == vk::PipelineStageFlagBits2::eColorAttachmentOutput);
  REQUIRE(barrier->dstAccessMask == vk::AccessFlagBits2::eColorAttachmentWrite);
}

TEST_CASE("ResourceStateTracker returns no barrier when the access is unchanged", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  const kisha::engine::ImageAccess access{handle, vk::ImageLayout::eColorAttachmentOptimal,
                                          vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                          vk::AccessFlagBits2::eColorAttachmentWrite};

  REQUIRE(tracker.transition(access).has_value());       // first access transitions
  REQUIRE_FALSE(tracker.transition(access).has_value()); // identical access is a redundant no-op
}

TEST_CASE("ResourceStateTracker emits a barrier for a stage/access-only change", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  // Move into a known layout first.
  REQUIRE(tracker.transition(kisha::engine::ImageAccess{handle, vk::ImageLayout::eGeneral,
                                                        vk::PipelineStageFlagBits2::eComputeShader,
                                                        vk::AccessFlagBits2::eShaderWrite})
              .has_value());

  // Same layout but a different stage/access still needs an execution/memory dependency.
  const std::optional<vk::ImageMemoryBarrier2> barrier = tracker.transition(
      kisha::engine::ImageAccess{handle, vk::ImageLayout::eGeneral, vk::PipelineStageFlagBits2::eFragmentShader,
                                 vk::AccessFlagBits2::eShaderRead});

  REQUIRE(barrier.has_value());
  REQUIRE(barrier->oldLayout == vk::ImageLayout::eGeneral);
  REQUIRE(barrier->newLayout == vk::ImageLayout::eGeneral);
  REQUIRE(barrier->srcStageMask == vk::PipelineStageFlagBits2::eComputeShader);
  REQUIRE(barrier->dstStageMask == vk::PipelineStageFlagBits2::eFragmentShader);
}

TEST_CASE("ResourceStateTracker::reset_image restarts from the given layout", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  const kisha::engine::ImageAccess color{handle, vk::ImageLayout::eColorAttachmentOptimal,
                                         vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                         vk::AccessFlagBits2::eColorAttachmentWrite};

  REQUIRE(tracker.transition(color).has_value());       // first frame transitions
  REQUIRE_FALSE(tracker.transition(color).has_value()); // unchanged within the frame

  // Simulate the start of the next frame for a transient swapchain image.
  tracker.reset_image(handle, vk::ImageLayout::eUndefined);

  const std::optional<vk::ImageMemoryBarrier2> barrier = tracker.transition(color);
  REQUIRE(barrier.has_value()); // the same access transitions again after the reset
  REQUIRE(barrier->oldLayout == vk::ImageLayout::eUndefined);
}

TEST_CASE("ResourceStateTracker::transition_to reports unknown resources", "[engine][core]") {
  kisha::engine::ResourceStateTracker tracker; // nothing registered

  const std::expected<std::optional<vk::ImageMemoryBarrier2>, kisha::engine::EngineError> barrier =
      tracker.transition_to(kisha::engine::ImageHandle{42U}, vk::ImageLayout::ePresentSrcKHR,
                            vk::PipelineStageFlagBits2::eBottomOfPipe, vk::AccessFlagBits2::eNone);

  REQUIRE_FALSE(barrier.has_value());
  REQUIRE(barrier.error() == kisha::engine::EngineError::UnknownResource);
}

TEST_CASE("ResourceStateTracker::transition_to emits the implicit present transition", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  // Put the image into the color-attachment state, then request the present hand-off.
  REQUIRE(tracker.transition(kisha::engine::ImageAccess{handle, vk::ImageLayout::eColorAttachmentOptimal,
                                                        vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                        vk::AccessFlagBits2::eColorAttachmentWrite})
              .has_value());

  const std::expected<std::optional<vk::ImageMemoryBarrier2>, kisha::engine::EngineError> barrier =
      tracker.transition_to(handle, vk::ImageLayout::ePresentSrcKHR, vk::PipelineStageFlagBits2::eBottomOfPipe,
                            vk::AccessFlagBits2::eNone);

  REQUIRE(barrier.has_value());
  REQUIRE(barrier->has_value());
  REQUIRE((*barrier)->oldLayout == vk::ImageLayout::eColorAttachmentOptimal);
  REQUIRE((*barrier)->newLayout == vk::ImageLayout::ePresentSrcKHR);
}

TEST_CASE("ResourceStateTracker::barriers_for aggregates only changed accesses", "[engine][core]") {
  const kisha::engine::ImageHandle handle{1U};
  auto tracker = make_tracker_with_image(handle);

  kisha::engine::PassDescription pass{};
  pass.image_accesses.push_back(kisha::engine::ImageAccess{handle, vk::ImageLayout::eColorAttachmentOptimal,
                                                           vk::PipelineStageFlagBits2::eColorAttachmentOutput,
                                                           vk::AccessFlagBits2::eColorAttachmentWrite});

  const std::expected<kisha::engine::PassBarriers, kisha::engine::EngineError> barriers = tracker.barriers_for(pass);
  REQUIRE(barriers.has_value());
  REQUIRE(barriers->image_barriers.size() == 1U);

  // Re-declaring the identical pass yields no barriers (state already satisfied).
  const std::expected<kisha::engine::PassBarriers, kisha::engine::EngineError> repeat = tracker.barriers_for(pass);
  REQUIRE(repeat.has_value());
  REQUIRE(repeat->image_barriers.empty());
}

TEST_CASE("ResourceStateTracker::barriers_for surfaces unknown resources", "[engine][core]") {
  const kisha::engine::ImageHandle known{1U};
  auto tracker = make_tracker_with_image(known);

  kisha::engine::PassDescription pass{};
  pass.image_accesses.push_back(kisha::engine::ImageAccess{
      kisha::engine::ImageHandle{99U}, vk::ImageLayout::eColorAttachmentOptimal,
      vk::PipelineStageFlagBits2::eColorAttachmentOutput, vk::AccessFlagBits2::eColorAttachmentWrite});

  const std::expected<kisha::engine::PassBarriers, kisha::engine::EngineError> barriers = tracker.barriers_for(pass);
  REQUIRE_FALSE(barriers.has_value());
  REQUIRE(barriers.error() == kisha::engine::EngineError::UnknownResource);
}
