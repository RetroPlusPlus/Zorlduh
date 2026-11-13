// The WindowKit module: a border's thickness and the room it leaves; a box as sprites — positions,
// tiles, keys, order, z, the cut on each piece past the end of its span, small and empty boxes, borders
// of larger pieces; a window as one layer; a message cut into pages, typed out, confirmed and closed;
// the arrow's blink; the page and the arrow as sprites; the message box. The borders and fonts are
// built over made-up manifests and handles. A cut piece is also drawn through a renderer on a GPU
// device and read back.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/viewport.h"

#include "LayerKit/types.h"
#include "TextKit/font.h"
#include "WindowKit/border.h"
#include "WindowKit/box.h"
#include "WindowKit/dialog.h"
#include "WindowKit/dialog_box.h"
#include "WindowKit/window.h"

namespace {

using retropp::IntRect;
using retropp::Sprite;
using WindowKit::Piece;

constexpr retropp::PaletteId kBorderColors{4};
constexpr retropp::PaletteId kTextColors{6};

// A border sheet of `cell`-sized cells, three to a row and four rows down, carved to `slots` slots.
WindowKit::Border border(int cell, std::size_t slots) {
    return WindowKit::Border{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{9},
            .slots = retropp::sliceLayout({.width = 3 * cell, .height = 4 * cell}, {.width = cell, .height = cell},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                          static_cast<int>(slots)),
            .kind = retropp::ContentKind::Tileset,
        },
        .palette = kBorderColors,
    };
}

// A font of `advance`-pixel steps over a 128×24 sheet of 8×8 cells, uppercase, digits and punctuation.
TextKit::Font font(int advance = 8) {
    return TextKit::Font{
        .sheet = retropp::AtlasManifest{
            .atlasId = retropp::AtlasId{5},
            .slots = retropp::sliceLayout({.width = 128, .height = 24}, {.width = 8, .height = 8},
                                          retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 43),
            .kind = retropp::ContentKind::Tileset,
        },
        .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:",
        .palette = kTextColors,
        .advance = {.x = advance, .y = advance},
    };
}

const Sprite* find(const std::vector<Sprite>& sprites, std::string_view key) {
    for (const Sprite& sprite : sprites) {
        if (std::string_view(sprite.key) == key) {
            return &sprite;
        }
    }
    return nullptr;
}

std::vector<std::string> keys(const std::vector<Sprite>& sprites) {
    std::vector<std::string> out;
    for (const Sprite& sprite : sprites) {
        out.emplace_back(std::string_view(sprite.key));
    }
    return out;
}

// The quad-space rectangle a cut region covers.
std::vector<retropp::Point> rect(float x, float y, float width, float height) {
    return retropp::ShapePoints::rectangle(retropp::Point{x, y}, width, height).points;
}

void expectCut(const Sprite& sprite, std::vector<std::vector<retropp::Point>> parts) {
    ASSERT_EQ(sprite.regions.size(), parts.size()) << std::string_view(sprite.key);
    for (std::size_t i = 0; i < parts.size(); ++i) {
        const retropp::Region& region = sprite.regions[i];
        EXPECT_EQ(std::string_view(region.key), "cut");
        EXPECT_EQ(region.shape.points, parts[i]) << std::string_view(sprite.key) << " region " << i;
        ASSERT_EQ(region.effects.size(), 1u);
        EXPECT_EQ(region.effects[0].kind, retropp::ScreenSpaceEffectKind::Transparency);
    }
}

std::span<const Sprite> spritesOf(const retropp::DrawLayer& layer) {
    return std::get<retropp::SpriteContent>(layer.content).sprites;
}

// The 18-by-2 room a message in the 8-pixel font is paged to in these tests.
constexpr retropp::PixelSize kRoom{.width = 144, .height = 16};
constexpr std::string_view kPassage = "The quick brown fox jumps over the lazy dog and runs far away.";

// ── Thickness and room ───────────────────────────────────────────────────────────────────────

TEST(Border, ThicknessIsTheTopLeftPieceAndInsideTakesItOffEachSide) {
    EXPECT_EQ(WindowKit::thickness(border(8, 10)), (retropp::PixelSize{.width = 8, .height = 8}));
    EXPECT_EQ(WindowKit::thickness(border(16, 9)), (retropp::PixelSize{.width = 16, .height = 16}));
    EXPECT_EQ(WindowKit::thickness(border(8, 8)), (retropp::PixelSize{}));
    EXPECT_EQ(WindowKit::inside({.x = 10, .y = 20, .width = 24, .height = 24}, border(8, 10)),
              (IntRect{.x = 18, .y = 28, .width = 8, .height = 8}));
    EXPECT_EQ(WindowKit::inside({.x = 0, .y = 0, .width = 50, .height = 50}, border(16, 9)),
              (IntRect{.x = 16, .y = 16, .width = 18, .height = 18}));
    // Thinner than two thicknesses: no room, never a negative size.
    EXPECT_EQ(WindowKit::inside({.x = 0, .y = 0, .width = 20, .height = 10}, border(8, 10)),
              (IntRect{.x = 8, .y = 8, .width = 4, .height = 0}));
}

// ── A box ────────────────────────────────────────────────────────────────────────────────────

TEST(Box, ABoxThatFitsItsPiecesIsNineUncutPiecesInOrder) {
    const WindowKit::Border b = border(8, 10);
    const std::vector<Sprite> sprites = WindowKit::Box("w", {.x = 10, .y = 20, .width = 24, .height = 24}, b);
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{
                                 "w:fill:0:0:24x24", "w:top:0:24x24", "w:bottom:0:24x24", "w:left:0:24x24",
                                 "w:right:0:24x24", "w:top-left:24x24", "w:top-right:24x24", "w:bottom-left:24x24",
                                 "w:bottom-right:24x24"}));
    const struct {
        Piece piece;
        int x;
        int y;
    } expected[] = {
        {Piece::Fill, 18, 28},    {Piece::Top, 18, 20},     {Piece::Bottom, 18, 36},
        {Piece::Left, 10, 28},    {Piece::Right, 26, 28},   {Piece::TopLeft, 10, 20},
        {Piece::TopRight, 26, 20}, {Piece::BottomLeft, 10, 36}, {Piece::BottomRight, 26, 36},
    };
    ASSERT_EQ(sprites.size(), 9u);
    for (std::size_t i = 0; i < 9; ++i) {
        const retropp::AssetSlot& slot = b.sheet.slots[static_cast<std::size_t>(expected[i].piece)];
        EXPECT_EQ(sprites[i].x, expected[i].x) << i;
        EXPECT_EQ(sprites[i].y, expected[i].y) << i;
        EXPECT_EQ(sprites[i].tile, slot.tile) << i;
        EXPECT_EQ(sprites[i].size, slot.dimensions) << i;
        EXPECT_EQ(sprites[i].atlas, b.sheet.atlasId) << i;
        EXPECT_EQ(sprites[i].palette, kBorderColors) << i;
        EXPECT_TRUE(sprites[i].regions.empty()) << i;
    }
}

TEST(Box, EachRepeatPastTheEndOfItsSpanIsCutThere) {
    const std::vector<Sprite> sprites = WindowKit::Box("w", {.x = 0, .y = 0, .width = 93, .height = 41}, border(8, 10));
    // Fill: the room is 77×25 at (8, 8): ten columns, four rows.
    EXPECT_EQ(sprites.size(), 40u + 10u + 10u + 4u + 4u + 4u);
    EXPECT_TRUE(sprites[0].regions.empty());
    expectCut(*find(sprites, "w:fill:9:0:93x41"), {rect(5, 0, 3, 8)});               // 5 px left in the room
    expectCut(*find(sprites, "w:fill:0:3:93x41"), {rect(0, 1, 8, 7)});               // 1 px left
    expectCut(*find(sprites, "w:fill:9:3:93x41"), {rect(5, 0, 3, 8), rect(0, 1, 8, 7)});  // right, then bottom
    EXPECT_EQ(find(sprites, "w:fill:10:0:93x41"), nullptr);
    EXPECT_EQ(find(sprites, "w:fill:0:4:93x41"), nullptr);
    // The top edge: ten repeats from x 8, the tenth at 80 cut at 85.
    EXPECT_EQ(find(sprites, "w:top:9:93x41")->x, 80);
    expectCut(*find(sprites, "w:top:9:93x41"), {rect(5, 0, 3, 8)});
    EXPECT_TRUE(find(sprites, "w:top:8:93x41")->regions.empty());
    EXPECT_EQ(find(sprites, "w:top:10:93x41"), nullptr);
    // The bottom and right edges against the box's bottom and right.
    EXPECT_EQ(find(sprites, "w:bottom:0:93x41")->y, 33);
    EXPECT_EQ(find(sprites, "w:right:0:93x41")->x, 85);
    expectCut(*find(sprites, "w:right:3:93x41"), {rect(0, 1, 8, 7)});
    EXPECT_EQ(find(sprites, "w:bottom-right:93x41")->x, 85);
    EXPECT_EQ(find(sprites, "w:bottom-right:93x41")->y, 33);
    EXPECT_TRUE(find(sprites, "w:bottom-right:93x41")->regions.empty());
}

TEST(Box, ABorderOfLargerPiecesRepeatsAndCutsAtItsOwnSize) {
    const WindowKit::Border b = border(16, 9);
    const std::vector<Sprite> sprites = WindowKit::Box("w", {.x = 0, .y = 0, .width = 50, .height = 50}, b);
    EXPECT_EQ(find(sprites, "w:top-left:50x50")->size, (retropp::AssetDimensions{.width = 16, .height = 16}));
    EXPECT_EQ(find(sprites, "w:top-right:50x50")->x, 34);
    EXPECT_EQ(find(sprites, "w:top:1:50x50")->x, 32);
    expectCut(*find(sprites, "w:top:1:50x50"), {rect(2, 0, 14, 16)});
    EXPECT_EQ(find(sprites, "w:top:2:50x50"), nullptr);
    expectCut(*find(sprites, "w:left:1:50x50"), {rect(0, 2, 16, 14)});
    expectCut(*find(sprites, "w:fill:1:1:50x50"), {rect(2, 0, 14, 16), rect(0, 2, 16, 14)});
    EXPECT_EQ(find(sprites, "w:fill:2:0:50x50"), nullptr);
}

TEST(Box, SmallBoxesOverlapOrCutTheirCornersAndEmptyOnesAreNothing) {
    const WindowKit::Border b = border(8, 10);
    // Narrower than two corners: they overlap, the right after the left, and no edge repeats across.
    const std::vector<Sprite> narrow = WindowKit::Box("w", {.x = 0, .y = 0, .width = 12, .height = 24}, b);
    EXPECT_EQ(find(narrow, "w:top:0:12x24"), nullptr);
    EXPECT_EQ(find(narrow, "w:top-right:12x24")->x, 4);
    EXPECT_TRUE(find(narrow, "w:top-right:12x24")->regions.empty());
    const std::vector<std::string> order = keys(narrow);
    EXPECT_LT(std::find(order.begin(), order.end(), "w:top-left:12x24"),
              std::find(order.begin(), order.end(), "w:top-right:12x24"));
    // Narrower than one corner: each corner is cut to the box.
    const std::vector<Sprite> thin = WindowKit::Box("w", {.x = 0, .y = 0, .width = 5, .height = 24}, b);
    expectCut(*find(thin, "w:top-left:5x24"), {rect(5, 0, 3, 8)});
    EXPECT_EQ(find(thin, "w:top-right:5x24")->x, -3);
    expectCut(*find(thin, "w:top-right:5x24"), {rect(0, 0, 3, 8)});
    // Nothing to draw.
    EXPECT_TRUE(WindowKit::Box("w", {.x = 0, .y = 0, .width = 0, .height = 24}, b).empty());
    EXPECT_TRUE(WindowKit::Box("w", {.x = 0, .y = 0, .width = 24, .height = -1}, b).empty());
    EXPECT_TRUE(WindowKit::Box("w", {.x = 0, .y = 0, .width = 24, .height = 24}, border(8, 8)).empty());
}

TEST(Box, EveryPieceIsAtZAndCarriesTheBoxsSizeInItsKey) {
    const WindowKit::Border b = border(8, 10);
    const std::vector<Sprite> narrow = WindowKit::Box("w", {.x = 0, .y = 0, .width = 24, .height = 24}, b, 7);
    const std::vector<Sprite> wide = WindowKit::Box("w", {.x = 0, .y = 0, .width = 32, .height = 24}, b, 7);
    for (const Sprite& sprite : wide) {
        EXPECT_EQ(sprite.z, 7) << std::string_view(sprite.key);
        EXPECT_TRUE(std::string_view(sprite.key).ends_with(":32x24")) << std::string_view(sprite.key);
    }
    for (const Sprite& sprite : narrow) {
        EXPECT_TRUE(std::string_view(sprite.key).ends_with(":24x24")) << std::string_view(sprite.key);
    }
    // A box at another size shares no key with it, so every piece is drawn in place on the tick it changes.
    for (const std::string& key : keys(narrow)) {
        EXPECT_EQ(find(wide, key), nullptr) << key;
    }
    // A box that only moves keeps every key.
    EXPECT_EQ(keys(WindowKit::Box("w", {.x = 40, .y = 9, .width = 24, .height = 24}, b, 7)), keys(narrow));
}

// ── A window ─────────────────────────────────────────────────────────────────────────────────

TEST(Window, IsOneSpriteLayerWithTheBoxBeneathTheChildrenInOrder) {
    const WindowKit::Border b = border(8, 10);
    const LayerKit::Sprites first{Sprite{.key = "first", .x = 12, .y = 12}};
    const LayerKit::Sprites second{Sprite{.key = "second", .x = 14, .y = 12, .z = -5}};
    const retropp::DrawLayer layer =
        WindowKit::Window("status", 7, {.x = 4, .y = 4, .width = 24, .height = 24}, b, {first, second},
                          {.scroll = {.x = 3, .y = 1}, .alpha = 0.5f});
    EXPECT_EQ(std::string_view(layer.key), "status");
    EXPECT_EQ(layer.z, 7);
    EXPECT_EQ(layer.size, retropp::Renderer::defaultViewport.size());
    EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = 3, .y = 1}));
    EXPECT_FLOAT_EQ(layer.alpha, 0.5f);
    const std::span<const Sprite> sprites = spritesOf(layer);
    ASSERT_EQ(sprites.size(), 11u);
    for (std::size_t i = 0; i < 9; ++i) {
        EXPECT_TRUE(std::string_view(sprites[i].key).starts_with("status:box:")) << i;
        EXPECT_EQ(sprites[i].z, std::numeric_limits<std::int32_t>::min()) << i;
    }
    EXPECT_EQ(std::string_view(sprites[9].key), "first");
    EXPECT_EQ(std::string_view(sprites[10].key), "second");
}

// ── A message ────────────────────────────────────────────────────────────────────────────────

TEST(Dialog, OpenWrapsToTheRoomAndCutsItIntoPages) {
    WindowKit::Dialog dialog;
    dialog.open(kPassage, kRoom, font());
    ASSERT_TRUE(dialog.showing());
    EXPECT_EQ(dialog.pageCount(), 2u);
    EXPECT_EQ(dialog.page(), 0u);
    EXPECT_EQ(dialog.text(), "The quick brown\nfox jumps over the");
    // A font twice the size: half the columns and half the lines.
    WindowKit::Dialog large;
    large.open(kPassage, kRoom, font(16));
    EXPECT_EQ(large.pageCount(), 7u);
    EXPECT_EQ(large.text(), "The quick");
    // A room smaller than one character still holds one column and one line: a word a page.
    WindowKit::Dialog tiny;
    tiny.open(kPassage, {.width = 3, .height = 3}, font());
    EXPECT_EQ(tiny.pageCount(), 13u);
    EXPECT_EQ(tiny.text(), "The");
    // Empty text opens nothing.
    WindowKit::Dialog empty;
    empty.open("", kRoom, font());
    EXPECT_FALSE(empty.showing());
    EXPECT_EQ(empty.text(), "");
}

TEST(Dialog, APageTypesOnAndConfirmShowsItTurnsItAndClosesAfterTheLast) {
    WindowKit::Dialog dialog;
    dialog.open(kPassage, kRoom, font());
    for (int i = 0; i < 5; ++i) {
        dialog.advance(false);
    }
    EXPECT_EQ(dialog.shown(), 2u);
    EXPECT_FALSE(dialog.pageDone());
    dialog.advance(true);  // shows the page whole
    EXPECT_TRUE(dialog.pageDone());
    EXPECT_EQ(dialog.page(), 0u);
    dialog.advance(true);  // turns it
    EXPECT_EQ(dialog.page(), 1u);
    EXPECT_EQ(dialog.shown(), 0u);
    EXPECT_EQ(dialog.text(), "lazy dog and runs\nfar away.");
    EXPECT_TRUE(dialog.lastPage());
    dialog.advance(true);  // shows the last page whole
    EXPECT_TRUE(dialog.showing());
    dialog.advance(true);  // closes it
    EXPECT_FALSE(dialog.showing());
    EXPECT_EQ(dialog.pageCount(), 0u);
    dialog.advance(true);
    EXPECT_FALSE(dialog.showing());
}

TEST(Dialog, TheArrowBlinksOnceAPageIsDoneAndAnotherFollows) {
    WindowKit::Dialog dialog;
    dialog.open(kPassage, kRoom, font());
    dialog.advance(false);
    EXPECT_FALSE(dialog.arrowOn());  // still typing
    dialog.advance(true);
    EXPECT_TRUE(dialog.arrowOn());   // done, a page follows
    for (int i = 0; i < 15; ++i) {
        dialog.advance(false);
    }
    EXPECT_TRUE(dialog.arrowOn());
    dialog.advance(false);           // 16 ticks done: off
    EXPECT_FALSE(dialog.arrowOn());
    for (int i = 0; i < 16; ++i) {
        dialog.advance(false);
    }
    EXPECT_TRUE(dialog.arrowOn());   // 32: on again
    dialog.advance(true);            // the last page
    dialog.advance(true);
    EXPECT_TRUE(dialog.pageDone());
    EXPECT_FALSE(dialog.arrowOn());

    WindowKit::Dialog steady;
    steady.arrowBlinkTicks = 0;
    steady.open(kPassage, kRoom, font());
    steady.advance(true);
    for (int i = 0; i < 40; ++i) {
        steady.advance(false);
        EXPECT_TRUE(steady.arrowOn()) << i;
    }
}

TEST(Dialog, OpeningAgainReplacesTheMessageAndCloseEndsIt) {
    WindowKit::Dialog dialog;
    dialog.open(kPassage, kRoom, font());
    dialog.advance(true);
    dialog.advance(true);
    dialog.open("Another one.", kRoom, font());
    EXPECT_EQ(dialog.page(), 0u);
    EXPECT_EQ(dialog.pageCount(), 1u);
    EXPECT_EQ(dialog.shown(), 0u);
    EXPECT_EQ(dialog.text(), "Another one.");
    dialog.close();
    EXPECT_FALSE(dialog.showing());
    EXPECT_EQ(dialog.text(), "");
}

// ── The message drawn ────────────────────────────────────────────────────────────────────────

TEST(DialogText, IsThePageUpAsFarAsItIsTypedKeyedByThePage) {
    WindowKit::Dialog dialog;
    EXPECT_TRUE(WindowKit::DialogText("msg", dialog, {.x = 8, .y = 104}, font()).empty());
    dialog.open(kPassage, kRoom, font());
    for (int i = 0; i < 10; ++i) {
        dialog.advance(false);
    }
    const std::vector<Sprite> typed = WindowKit::DialogText("msg", dialog, {.x = 8, .y = 104}, font(), 3);
    EXPECT_EQ(keys(typed), (std::vector<std::string>{"msg:0:0", "msg:0:1", "msg:0:2", "msg:0:4"}));  // "The q"
    EXPECT_EQ(typed[3].x, 8 + 4 * 8);
    EXPECT_EQ(typed[3].y, 104);
    EXPECT_EQ(typed[3].z, 3);
    dialog.advance(true);
    dialog.advance(true);
    for (const Sprite& sprite : WindowKit::DialogText("msg", dialog, {.x = 8, .y = 104}, font())) {
        EXPECT_TRUE(std::string_view(sprite.key).starts_with("msg:1:")) << std::string_view(sprite.key);
    }
}

TEST(DialogArrow, IsTheArrowSlotAtTheRoomsBottomRightWhileItIsOn) {
    const WindowKit::Border b = border(8, 10);
    const IntRect room{.x = 8, .y = 104, .width = 144, .height = 32};
    WindowKit::Dialog dialog;
    dialog.open(kPassage, kRoom, font());
    EXPECT_TRUE(WindowKit::DialogArrow("msg", dialog, room, b).empty());
    dialog.advance(true);
    const std::vector<Sprite> arrow = WindowKit::DialogArrow("msg", dialog, room, b, 1);
    ASSERT_EQ(arrow.size(), 1u);
    EXPECT_EQ(std::string_view(arrow[0].key), "msg:arrow:0");
    EXPECT_EQ(arrow[0].x, 8 + 144 - 8);
    EXPECT_EQ(arrow[0].y, 104 + 32 - 8);
    EXPECT_EQ(arrow[0].z, 1);
    EXPECT_EQ(arrow[0].tile, b.sheet.slots[WindowKit::kArrowSlot].tile);
    EXPECT_EQ(arrow[0].size, b.sheet.slots[WindowKit::kArrowSlot].dimensions);
    EXPECT_EQ(arrow[0].palette, kBorderColors);
    // A sheet of the nine pieces only has no arrow.
    EXPECT_TRUE(WindowKit::DialogArrow("msg", dialog, room, border(8, 9)).empty());
}

TEST(DialogBox, IsAnEmptyLayerWhileClosedAndTheBoxTextAndArrowWhileShowing) {
    const WindowKit::Border b = border(8, 10);
    const IntRect box{.x = 0, .y = 96, .width = 160, .height = 48};
    WindowKit::Dialog dialog;
    const retropp::DrawLayer closed = WindowKit::DialogBox("dialog", 20, box, b, font(), dialog);
    EXPECT_EQ(std::string_view(closed.key), "dialog");
    EXPECT_EQ(closed.z, 20);
    EXPECT_TRUE(spritesOf(closed).empty());

    dialog.open(kPassage, kRoom, font());  // two pages, so the arrow shows on the first
    dialog.advance(true);
    const retropp::DrawLayer showing = WindowKit::DialogBox("dialog", 20, box, b, font(), dialog);
    const std::span<const Sprite> sprites = spritesOf(showing);
    ASSERT_FALSE(sprites.empty());
    EXPECT_EQ(std::string_view(sprites.front().key), "dialog:box:fill:0:0:160x48");
    EXPECT_EQ(std::string_view(sprites.back().key), "dialog:arrow:0");
    EXPECT_EQ(sprites.back().z, 1);
    const Sprite* first = nullptr;
    for (const Sprite& sprite : sprites) {
        if (std::string_view(sprite.key) == "dialog:0:0") {
            first = &sprite;
        }
    }
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first->x, 8);
    EXPECT_EQ(first->y, 104);
}

// ── A cut piece, drawn (through a renderer on a GPU device) ─────────────────────────────────────

class CutOnDevice : public ::testing::Test {
protected:
    static inline SDL_GPUDevice* device_ = nullptr;
    static inline std::string error_;

    static void SetUpTestSuite() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            error_ = std::string("SDL_Init failed: ") + SDL_GetError();
            return;
        }
        device_ = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
            /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            error_ = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        }
    }

    static void TearDownTestSuite() {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        SDL_Quit();
    }

    void SetUp() override {
        if (device_ == nullptr) {
            FAIL() << "no GPU device: " << error_ << ". A drawn cut is read back from a renderer, which needs one.";
        }
    }
};

// A span 12 pixels long of an 8-pixel piece: the second repeat runs 4 pixels past the end, and a
// Transparency region over those 4 pixels of its own quad leaves them undrawn.
TEST_F(CutOnDevice, ARegionOverThePartPastTheEndLeavesItUndrawn) {
    constexpr int kWidth = 24;
    constexpr int kHeight = 8;
    retropp::Renderer renderer{device_, nullptr, retropp::ViewportResolution{kWidth, kHeight}};
    constexpr std::array<retropp::Rgba8, 2> kColors{{{220, 30, 30, 255}, {30, 30, 220, 255}}};
    const retropp::PaletteId palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(kColors));
    std::array<std::uint8_t, 8 * 8> piece{};
    piece.fill(0);
    const retropp::AtlasId pieceSheet = renderer.uploadAtlas(piece.data(), 8, 8).atlasId;
    std::array<std::uint8_t, 8 * 8> ground{};
    ground.fill(1);
    const retropp::AtlasId groundSheet = renderer.uploadAtlas(ground.data(), 8, 8).atlasId;

    std::vector<retropp::TileCell> cells(kWidth / 8 * (kHeight / 8),
                                         retropp::TileCell{.atlas = groundSheet, .tile = 0, .palette = palette});
    retropp::DrawLayer back{.key = "ground"};
    back.z = 0;
    back.size = retropp::PixelSize{kWidth, kHeight};
    back.content = retropp::TileContent{.widthInTiles = kWidth / 8, .heightInTiles = kHeight / 8,
                                        .cells = std::span<const retropp::TileCell>(cells)};

    const retropp::ScreenSpaceEffect hide{.kind = retropp::ScreenSpaceEffectKind::Transparency};
    std::array<retropp::Sprite, 2> sprites{
        retropp::Sprite{.key = "edge:0", .x = 0, .y = 0, .atlas = pieceSheet, .tile = 0, .palette = palette},
        retropp::Sprite{.key = "edge:1", .x = 8, .y = 0, .atlas = pieceSheet, .tile = 0, .palette = palette},
    };
    sprites[1].regions = {retropp::Region{.key = "cut",
                                          .shape = retropp::ShapePoints::rectangle(retropp::Point{4, 0}, 4, 8),
                                          .effects = {hide}}};
    retropp::DrawLayer front{.key = "box"};
    front.z = 10;
    front.size = retropp::PixelSize{kWidth, kHeight};
    front.content = retropp::SpriteContent{.sprites = std::span<const retropp::Sprite>(sprites)};

    retropp::FrameDrawState frame;
    frame.layers.push_back(back);
    frame.layers.push_back(front);
    const std::vector<retropp::Rgba8> pixels = renderer.captureViewport(frame);
    ASSERT_EQ(pixels.size(), static_cast<std::size_t>(kWidth * kHeight));

    const auto isPiece = [](retropp::Rgba8 c) { return c.r > 180 && c.b < 80; };
    const auto isGround = [](retropp::Rgba8 c) { return c.b > 180 && c.r < 80; };
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            const retropp::Rgba8 c = pixels[static_cast<std::size_t>(y * kWidth + x)];
            if (x < 12) {
                EXPECT_TRUE(isPiece(c)) << x << "," << y << ": " << +c.r << " " << +c.g << " " << +c.b;
            } else {
                EXPECT_TRUE(isGround(c)) << x << "," << y << ": " << +c.r << " " << +c.g << " " << +c.b;
            }
        }
    }
}

}  // namespace
