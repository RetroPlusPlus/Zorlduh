// ImageLayer and RasterLayer: what each returned layer carries, which options reach it, how long the
// pixels behind a raster layer's span stay valid, and that RasterLayerOptions carries every
// LayerOptions field.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

#include "retropp/draw_state.h"
#include "retropp/image.h"
#include "retropp/raster_content.h"
#include "retropp/renderer.h"
#include "retropp/transform.h"
#include "retropp/viewport.h"

#include "layers/image_layer.h"
#include "layers/layer_options.h"
#include "layers/raster_layer.h"
#include "layers/types.h"

namespace {

using layers::ImageLayer;
using layers::LayerOptions;
using layers::Raster;
using layers::RasterLayer;
using layers::RasterLayerOptions;

// A 2×1 RGBA raster: two pixels whose first byte is `first` and `first + 1`.
Raster twoPixels(std::uint8_t first, std::uint64_t generation) {
    return Raster{.pixels = {first, 0, 0, 255, static_cast<std::uint8_t>(first + 1), 0, 0, 255},
                  .width = 2,
                  .height = 1,
                  .generation = generation};
}

const retropp::RasterContent& rasterOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::RasterContent>(layer.content);
}

// An option set in which every field differs from its default.
RasterLayerOptions everyOptionSet() {
    return RasterLayerOptions{
        .scroll = {.x = -8, .y = -16},
        .alpha = 0.5f,
        .blend = retropp::BlendMode::Add,
        .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::Ripple,
                                               .amplitude = 3.0f}},
        .regions = {retropp::Region{.key = "frame",
                                    .shape = retropp::ShapePoints::rectangle({0, 16}, 160, 128)}},
        .transform = retropp::Transform::translation(1.0f, 2.0f),
        .transformEdge = retropp::DisplacementEdge::Stretch,
        .advancesEvery = 4u,
        .fit = {.width = 144, .height = 112},
    };
}

void expectEveryOptionForwarded(const retropp::DrawLayer& layer) {
    EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = -8, .y = -16}));
    EXPECT_FLOAT_EQ(layer.alpha, 0.5f);
    EXPECT_EQ(layer.blend, retropp::BlendMode::Add);
    ASSERT_EQ(layer.effects.size(), 1u);
    EXPECT_EQ(layer.effects[0].kind, retropp::ScreenSpaceEffectKind::Ripple);
    EXPECT_FLOAT_EQ(layer.effects[0].amplitude, 3.0f);
    ASSERT_EQ(layer.regions.size(), 1u);
    EXPECT_EQ(layer.regions[0].key, retropp::ObjectKey{"frame"});
    EXPECT_EQ(layer.regions[0].shape, retropp::ShapePoints::rectangle({0, 16}, 160, 128));
    EXPECT_EQ(layer.transform, retropp::Transform::translation(1.0f, 2.0f));
    EXPECT_EQ(layer.transformEdge, retropp::DisplacementEdge::Stretch);
    EXPECT_EQ(layer.advancesEvery, std::optional<std::uint32_t>{4u});
    EXPECT_EQ(rasterOf(layer).fit, (retropp::PixelSize{.width = 144, .height = 112}));
}

// With no options, every field the aggregate does not own is the engine's own default.
void expectEngineDefaults(const retropp::DrawLayer& layer) {
    const retropp::DrawLayer plain{.key = "plain"};
    EXPECT_EQ(layer.scroll, plain.scroll);
    EXPECT_FLOAT_EQ(layer.alpha, plain.alpha);
    EXPECT_EQ(layer.blend, plain.blend);
    EXPECT_TRUE(layer.effects.empty());
    EXPECT_TRUE(layer.regions.empty());
    EXPECT_EQ(layer.transform, plain.transform);
    EXPECT_EQ(layer.transformEdge, plain.transformEdge);
    EXPECT_FALSE(layer.advancesEvery.has_value());
    EXPECT_EQ(rasterOf(layer).fit, retropp::RasterContent{}.fit);
}

// Every test starts from the Game Boy Color viewport and leaves the engine's default as it found it.
class RasterLayers : public ::testing::Test {
protected:
    void SetUp() override {
        saved_ = retropp::Renderer::defaultViewport;
        retropp::Renderer::defaultViewport = retropp::ViewportResolution::GameBoyColor;
    }
    void TearDown() override { retropp::Renderer::defaultViewport = saved_; }

private:
    retropp::ViewportResolution saved_ = retropp::ViewportResolution::GameBoyColor;
};

// ── RasterLayerOptions ───────────────────────────────────────────────────────────────────────

// RasterLayerOptions is every LayerOptions field, in order and of the same type, then `fit`. A field
// added to LayerOptions stops this from compiling until it is added to RasterLayerOptions and here.
TEST(RasterLayerOptionsFields, MatchLayerOptionsOneForOneThenFit) {
    const LayerOptions layer{};
    const RasterLayerOptions raster{};
    const auto& [scroll, alpha, blend, effects, regions, transform, transformEdge, advancesEvery] =
        layer;
    const auto& [rScroll, rAlpha, rBlend, rEffects, rRegions, rTransform, rTransformEdge,
                 rAdvancesEvery, rFit] = raster;
    static_assert(std::is_same_v<decltype(scroll), decltype(rScroll)>);
    static_assert(std::is_same_v<decltype(alpha), decltype(rAlpha)>);
    static_assert(std::is_same_v<decltype(blend), decltype(rBlend)>);
    static_assert(std::is_same_v<decltype(effects), decltype(rEffects)>);
    static_assert(std::is_same_v<decltype(regions), decltype(rRegions)>);
    static_assert(std::is_same_v<decltype(transform), decltype(rTransform)>);
    static_assert(std::is_same_v<decltype(transformEdge), decltype(rTransformEdge)>);
    static_assert(std::is_same_v<decltype(advancesEvery), decltype(rAdvancesEvery)>);
    static_assert(std::is_same_v<std::remove_cvref_t<decltype(rFit)>, retropp::PixelSize>);

    // The shared fields default alike.
    EXPECT_EQ(scroll, rScroll);
    EXPECT_FLOAT_EQ(alpha, rAlpha);
    EXPECT_EQ(blend, rBlend);
    EXPECT_EQ(effects.size(), rEffects.size());
    EXPECT_EQ(regions.size(), rRegions.size());
    EXPECT_EQ(transform, rTransform);
    EXPECT_EQ(transformEdge, rTransformEdge);
    EXPECT_EQ(advancesEvery, rAdvancesEvery);
    EXPECT_EQ(rFit, (retropp::PixelSize{}));
}

// ── ImageLayer ───────────────────────────────────────────────────────────────────────────────

TEST_F(RasterLayers, ImageLayerCarriesItsKeyDepthViewportSizeAndTheImage) {
    const retropp::DrawLayer layer = ImageLayer("title", 30, retropp::ImageId{7});
    EXPECT_EQ(layer.key, retropp::ObjectKey{"title"});
    EXPECT_EQ(layer.z, 30);
    EXPECT_EQ(layer.size, (retropp::PixelSize{.width = 160, .height = 144}));
    ASSERT_TRUE(std::holds_alternative<retropp::RasterContent>(layer.content));
    const retropp::RasterContent& raster = rasterOf(layer);
    EXPECT_EQ(raster.image, retropp::ImageId{7});
    EXPECT_TRUE(raster.pixels.empty());
    EXPECT_EQ(raster.generation, 0u);
}

TEST_F(RasterLayers, ImageLayerForwardsEveryOptionIncludingFit) {
    expectEveryOptionForwarded(ImageLayer("options", 0, retropp::ImageId{1}, everyOptionSet()));
}

TEST_F(RasterLayers, ImageLayerWithNoOptionsTakesTheEngineDefaults) {
    expectEngineDefaults(ImageLayer("defaults", 0, retropp::ImageId{1}));
}

// ── RasterLayer ──────────────────────────────────────────────────────────────────────────────

TEST_F(RasterLayers, RasterLayerCarriesItsKeyDepthViewportSizeAndTheRaster) {
    const retropp::DrawLayer layer = RasterLayer("scope", 40, twoPixels(10, 3));
    EXPECT_EQ(layer.key, retropp::ObjectKey{"scope"});
    EXPECT_EQ(layer.z, 40);
    EXPECT_EQ(layer.size, (retropp::PixelSize{.width = 160, .height = 144}));
    ASSERT_TRUE(std::holds_alternative<retropp::RasterContent>(layer.content));
    const retropp::RasterContent& raster = rasterOf(layer);
    EXPECT_EQ(raster.width, 2);
    EXPECT_EQ(raster.height, 1);
    EXPECT_EQ(raster.format, retropp::RasterPixelFormat::Rgba8888);
    EXPECT_EQ(raster.generation, 3u);
    EXPECT_EQ(raster.image, retropp::ImageId{});
}

TEST_F(RasterLayers, RasterLayerKeepsThePixelsInOrder) {
    const retropp::DrawLayer layer = RasterLayer("pixels", 0, twoPixels(10, 1));
    const auto& pixels = rasterOf(layer).pixels;
    ASSERT_EQ(pixels.size(), 8u);
    EXPECT_EQ(pixels[0], 10);
    EXPECT_EQ(pixels[3], 255);
    EXPECT_EQ(pixels[4], 11);
    EXPECT_EQ(pixels[7], 255);
}

TEST_F(RasterLayers, RasterLayerForwardsEveryOptionIncludingFit) {
    expectEveryOptionForwarded(RasterLayer("options", 0, twoPixels(0, 1), everyOptionSet()));
}

TEST_F(RasterLayers, RasterLayerWithNoOptionsTakesTheEngineDefaults) {
    expectEngineDefaults(RasterLayer("defaults", 0, twoPixels(0, 1)));
}

TEST_F(RasterLayers, LayerSizeFollowsTheDefaultViewportAtEachCall) {
    retropp::Renderer::defaultViewport = retropp::ViewportResolution::Snes;
    const retropp::PixelSize snes{.width = 256, .height = 224};
    EXPECT_EQ(ImageLayer("snes-image", 0, retropp::ImageId{1}).size, snes);
    EXPECT_EQ(RasterLayer("snes-raster", 0, twoPixels(0, 1)).size, snes);
}

// ── What the span reads, and for how long ────────────────────────────────────────────────────

TEST_F(RasterLayers, RasterLayerContentSurvivesACallWithAnotherKey) {
    const retropp::DrawLayer first = RasterLayer("first-raster", 0, twoPixels(20, 1));
    const retropp::DrawLayer second = RasterLayer("second-raster", 0, twoPixels(30, 1));
    ASSERT_EQ(rasterOf(first).pixels.size(), 8u);
    EXPECT_EQ(rasterOf(first).pixels[0], 20);
    EXPECT_EQ(rasterOf(first).pixels[4], 21);
    ASSERT_EQ(rasterOf(second).pixels.size(), 8u);
    EXPECT_EQ(rasterOf(second).pixels[0], 30);
}

TEST_F(RasterLayers, RasterLayerCallWithTheSameKeyReplacesTheContent) {
    (void)RasterLayer("same-raster", 0, twoPixels(50, 1));
    const retropp::DrawLayer second =
        RasterLayer("same-raster", 0,
                    Raster{.pixels = {90, 0, 0, 255}, .width = 1, .height = 1, .generation = 2});
    const retropp::RasterContent& raster = rasterOf(second);
    ASSERT_EQ(raster.pixels.size(), 4u);
    EXPECT_EQ(raster.pixels[0], 90);
    EXPECT_EQ(raster.width, 1);
    EXPECT_EQ(raster.generation, 2u);
}

}  // namespace
