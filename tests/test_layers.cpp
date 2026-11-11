// SpriteLayer and TileLayer: what each returned layer carries, which options reach it, and how long
// the content behind its span stays valid.

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <variant>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"
#include "retropp/transform.h"
#include "retropp/viewport.h"

#include "LayerKit/layer_options.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/tile_layer.h"
#include "LayerKit/types.h"

namespace {

using LayerKit::Cells;
using LayerKit::LayerOptions;
using LayerKit::Sprites;
using LayerKit::SpriteLayer;
using LayerKit::TileLayer;

retropp::Sprite sprite(const char* key, int x) {
    return retropp::Sprite{.key = key, .x = x};
}

retropp::TileCell cell(std::uint16_t tile) {
    return retropp::TileCell{.tile = tile};
}

const retropp::SpriteContent& spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content);
}

const retropp::TileContent& tilesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::TileContent>(layer.content);
}

// An option set in which every field differs from its default.
LayerOptions everyOptionSet() {
    return LayerOptions{
        .scroll = {.x = 3, .y = -4},
        .alpha = 0.25f,
        .blend = retropp::BlendMode::Multiply,
        .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::RowDisplacement,
                                               .amplitude = 2.0f}},
        .regions = {retropp::Region{.key = "clip",
                                    .shape = retropp::ShapePoints::rectangle({8, 16}, 32, 24)}},
        .transform = retropp::Transform::translation(5.0f, 6.0f),
        .transformEdge = retropp::DisplacementEdge::Stretch,
        .advancesEvery = 2u,
    };
}

void expectEveryOptionForwarded(const retropp::DrawLayer& layer) {
    EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = 3, .y = -4}));
    EXPECT_FLOAT_EQ(layer.alpha, 0.25f);
    EXPECT_EQ(layer.blend, retropp::BlendMode::Multiply);
    ASSERT_EQ(layer.effects.size(), 1u);
    EXPECT_EQ(layer.effects[0].kind, retropp::ScreenSpaceEffectKind::RowDisplacement);
    EXPECT_FLOAT_EQ(layer.effects[0].amplitude, 2.0f);
    ASSERT_EQ(layer.regions.size(), 1u);
    EXPECT_EQ(layer.regions[0].key, retropp::ObjectKey{"clip"});
    EXPECT_EQ(layer.regions[0].shape, retropp::ShapePoints::rectangle({8, 16}, 32, 24));
    EXPECT_EQ(layer.transform, retropp::Transform::translation(5.0f, 6.0f));
    EXPECT_EQ(layer.transformEdge, retropp::DisplacementEdge::Stretch);
    EXPECT_EQ(layer.advancesEvery, std::optional<std::uint32_t>{2u});
}

// With no options, every field the aggregate does not own is the engine's own DrawLayer default.
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
    EXPECT_EQ(layer.advancesEvery, plain.advancesEvery);
}

// Every test starts from the Game Boy Color viewport and leaves the engine's default as it found it.
class Layers : public ::testing::Test {
protected:
    void SetUp() override {
        saved_ = retropp::Renderer::defaultViewport;
        retropp::Renderer::defaultViewport = retropp::ViewportResolution::GameBoyColor;
    }
    void TearDown() override { retropp::Renderer::defaultViewport = saved_; }

private:
    retropp::ViewportResolution saved_ = retropp::ViewportResolution::GameBoyColor;
};

// ── SpriteLayer ──────────────────────────────────────────────────────────────────────────────

TEST_F(Layers, SpriteLayerCarriesItsKeyDepthAndTheViewportSize) {
    const retropp::DrawLayer layer = SpriteLayer("actors", 10, {Sprites{sprite("player", 0)}});
    EXPECT_EQ(layer.key, retropp::ObjectKey{"actors"});
    EXPECT_EQ(layer.z, 10);
    EXPECT_EQ(layer.size, (retropp::PixelSize{.width = 160, .height = 144}));
    EXPECT_TRUE(std::holds_alternative<retropp::SpriteContent>(layer.content));
}

TEST_F(Layers, SpriteLayerConcatenatesChildrenInDeclarationOrder) {
    const retropp::DrawLayer layer = SpriteLayer(
        "order", 0,
        {Sprites{sprite("a", 1), sprite("b", 2)}, Sprites{}, Sprites{sprite("c", 3)}});
    const auto& sprites = spritesOf(layer).sprites;
    ASSERT_EQ(sprites.size(), 3u);
    EXPECT_EQ(sprites[0].key, retropp::ObjectKey{"a"});
    EXPECT_EQ(sprites[1].key, retropp::ObjectKey{"b"});
    EXPECT_EQ(sprites[2].key, retropp::ObjectKey{"c"});
    EXPECT_EQ(sprites[0].x, 1);
    EXPECT_EQ(sprites[1].x, 2);
    EXPECT_EQ(sprites[2].x, 3);
}

TEST_F(Layers, SpriteLayerWithNoChildrenHasAnEmptySpan) {
    const retropp::DrawLayer layer = SpriteLayer("empty", 0, {});
    EXPECT_TRUE(spritesOf(layer).sprites.empty());
}

TEST_F(Layers, SpriteLayerForwardsEveryOption) {
    expectEveryOptionForwarded(SpriteLayer("options", 0, {Sprites{}}, everyOptionSet()));
}

TEST_F(Layers, SpriteLayerWithNoOptionsTakesTheEngineDefaults) {
    expectEngineDefaults(SpriteLayer("defaults", 0, {Sprites{}}));
}

// ── TileLayer ────────────────────────────────────────────────────────────────────────────────

TEST_F(Layers, TileLayerCarriesItsKeyDepthViewportSizeGridAndBlankWrap) {
    const retropp::DrawLayer layer = TileLayer("world", 0, Cells(6, cell(0)), 3, 2);
    EXPECT_EQ(layer.key, retropp::ObjectKey{"world"});
    EXPECT_EQ(layer.z, 0);
    EXPECT_EQ(layer.size, (retropp::PixelSize{.width = 160, .height = 144}));
    ASSERT_TRUE(std::holds_alternative<retropp::TileContent>(layer.content));
    const retropp::TileContent& tiles = tilesOf(layer);
    EXPECT_EQ(tiles.widthInTiles, 3);
    EXPECT_EQ(tiles.heightInTiles, 2);
    EXPECT_EQ(tiles.wrap, retropp::TileWrap::Blank);
}

TEST_F(Layers, TileLayerKeepsTheCellsInOrder) {
    const retropp::DrawLayer layer =
        TileLayer("cells", 0, Cells{cell(4), cell(5), cell(6), cell(7)}, 2, 2);
    const auto& cells = tilesOf(layer).cells;
    ASSERT_EQ(cells.size(), 4u);
    EXPECT_EQ(cells[0].tile, 4);
    EXPECT_EQ(cells[1].tile, 5);
    EXPECT_EQ(cells[2].tile, 6);
    EXPECT_EQ(cells[3].tile, 7);
}

TEST_F(Layers, TileLayerForwardsEveryOption) {
    expectEveryOptionForwarded(TileLayer("options", 0, Cells{cell(1)}, 1, 1, everyOptionSet()));
}

TEST_F(Layers, TileLayerWithNoOptionsTakesTheEngineDefaults) {
    expectEngineDefaults(TileLayer("defaults", 0, Cells{cell(1)}, 1, 1));
}

// ── Layer size ───────────────────────────────────────────────────────────────────────────────

TEST_F(Layers, LayerSizeFollowsTheDefaultViewportAtEachCall) {
    retropp::Renderer::defaultViewport = retropp::ViewportResolution::Snes;
    const retropp::PixelSize snes{.width = 256, .height = 224};
    EXPECT_EQ(SpriteLayer("snes-sprites", 0, {}).size, snes);
    EXPECT_EQ(TileLayer("snes-tiles", 0, Cells{cell(0)}, 1, 1).size, snes);
}

// ── What the span reads, and for how long ────────────────────────────────────────────────────

TEST_F(Layers, SpriteLayerContentSurvivesACallWithAnotherKey) {
    const retropp::DrawLayer first = SpriteLayer("first", 0, {Sprites{sprite("one", 1)}});
    const retropp::DrawLayer second =
        SpriteLayer("second", 0, {Sprites{sprite("two", 2), sprite("three", 3)}});
    ASSERT_EQ(spritesOf(first).sprites.size(), 1u);
    EXPECT_EQ(spritesOf(first).sprites[0].key, retropp::ObjectKey{"one"});
    ASSERT_EQ(spritesOf(second).sprites.size(), 2u);
    EXPECT_EQ(spritesOf(second).sprites[0].key, retropp::ObjectKey{"two"});
}

TEST_F(Layers, SpriteLayerCallWithTheSameKeyReplacesTheContent) {
    (void)SpriteLayer("same", 0, {Sprites{sprite("old-a", 1), sprite("old-b", 2)}});
    const retropp::DrawLayer second = SpriteLayer("same", 0, {Sprites{sprite("new", 3)}});
    const auto& sprites = spritesOf(second).sprites;
    ASSERT_EQ(sprites.size(), 1u);
    EXPECT_EQ(sprites[0].key, retropp::ObjectKey{"new"});
    EXPECT_EQ(sprites[0].x, 3);
}

TEST_F(Layers, TileLayerContentSurvivesACallWithAnotherKey) {
    const retropp::DrawLayer first = TileLayer("first-tiles", 0, Cells{cell(11), cell(12)}, 2, 1);
    const retropp::DrawLayer second = TileLayer("second-tiles", 0, Cells{cell(21)}, 1, 1);
    ASSERT_EQ(tilesOf(first).cells.size(), 2u);
    EXPECT_EQ(tilesOf(first).cells[0].tile, 11);
    EXPECT_EQ(tilesOf(first).cells[1].tile, 12);
    ASSERT_EQ(tilesOf(second).cells.size(), 1u);
    EXPECT_EQ(tilesOf(second).cells[0].tile, 21);
}

TEST_F(Layers, TileLayerCallWithTheSameKeyReplacesTheContent) {
    (void)TileLayer("same-tiles", 0, Cells{cell(1), cell(2), cell(3), cell(4)}, 2, 2);
    const retropp::DrawLayer second = TileLayer("same-tiles", 0, Cells{cell(9)}, 1, 1);
    const auto& cells = tilesOf(second).cells;
    ASSERT_EQ(cells.size(), 1u);
    EXPECT_EQ(cells[0].tile, 9);
}

}  // namespace
