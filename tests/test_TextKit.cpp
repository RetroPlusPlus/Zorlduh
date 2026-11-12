// The TextKit module: which slot a character draws through the case rule, a passage laid out as
// sprites — keys, positions, tiles, palettes, the shadow beneath each glyph, lines, spaces, how much is
// shown, alignment within a width, z — what throws, wrapping, measuring, and the reveal. The fonts are
// built over made-up manifests and handles; no renderer is needed.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/atlas_manifest.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"

#include "TextKit/font.h"
#include "TextKit/reveal.h"
#include "TextKit/text.h"

namespace {

using retropp::Sprite;

constexpr std::string_view kUpper = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,!?'-:";
constexpr std::string_view kLower = "abcdefghijklmnopqrstuvwxyz";
constexpr std::string_view kMixed = "AaBbCcDdEeFfGgHhIiJjKkLlMmNnOoPpQqRrSsTtUu";

constexpr retropp::PaletteId kPalette{2};
constexpr retropp::PaletteId kShadow{3};

// A 128×24 sheet of 8×8 cells, 16 to a row, carved to 43 slots: slot n's tile is n.
retropp::AtlasManifest sheet() {
    return retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{5},
        .slots = retropp::sliceLayout({.width = 128, .height = 24}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown, 43),
        .kind = retropp::ContentKind::Tileset,
    };
}

TextKit::Font font(std::string_view glyphs = kUpper) {
    return TextKit::Font{.sheet = sheet(), .glyphs = glyphs, .palette = kPalette, .shadow = kShadow};
}

TextKit::Font flat(std::string_view glyphs = kUpper) {
    return TextKit::Font{.sheet = sheet(), .glyphs = glyphs, .palette = kPalette};
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

// ── Text ─────────────────────────────────────────────────────────────────────────────────────

TEST(Text, ARunIsOneSpritePerGlyphWithItsShadowBeneathAndBeforeIt) {
    const std::vector<Sprite> sprites = TextKit::Text("t", "HI", {.x = 10, .y = 20}, font());
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"t:0:shadow", "t:0", "t:1:shadow", "t:1"}));

    const Sprite& h = sprites[1];
    EXPECT_EQ(h.x, 10);
    EXPECT_EQ(h.y, 20);
    EXPECT_EQ(h.z, 0);
    EXPECT_EQ(h.tile, 7);
    EXPECT_EQ(h.atlas, retropp::AtlasId{5});
    EXPECT_EQ(h.palette, kPalette);
    EXPECT_EQ(h.size, (retropp::AssetDimensions{.width = 8, .height = 8}));
    const Sprite& i = sprites[3];
    EXPECT_EQ(i.x, 18);
    EXPECT_EQ(i.y, 20);
    EXPECT_EQ(i.tile, 8);

    const Sprite& hShadow = sprites[0];
    EXPECT_EQ(hShadow.x, 11);
    EXPECT_EQ(hShadow.y, 21);
    EXPECT_EQ(hShadow.z, -1);
    EXPECT_EQ(hShadow.tile, 7);
    EXPECT_EQ(hShadow.palette, kShadow);
    const Sprite& iShadow = sprites[2];
    EXPECT_EQ(iShadow.x, 19);
    EXPECT_EQ(iShadow.y, 21);
    EXPECT_EQ(iShadow.tile, 8);
}

TEST(Text, AFontWithoutAShadowDrawsTheGlyphsAlone) {
    const std::vector<Sprite> sprites = TextKit::Text("t", "HI", {.x = 10, .y = 20}, flat());
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"t:0", "t:1"}));
}

TEST(Text, ANewlineStartsTheNextLineOneAdvanceDown) {
    const std::vector<Sprite> sprites = TextKit::Text("t", "A\nB", {.x = 10, .y = 20}, flat());
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"t:0", "t:2"}));
    EXPECT_EQ(sprites[1].x, 10);
    EXPECT_EQ(sprites[1].y, 28);
    EXPECT_EQ(TextKit::measure("A\nB", flat()), (retropp::PixelSize{.width = 8, .height = 16}));

    TextKit::Font tall = flat();
    tall.advance = {.x = 8, .y = 16};
    const std::vector<Sprite> spaced = TextKit::Text("t", "A\nB", {.x = 10, .y = 20}, tall);
    EXPECT_EQ(spaced[1].x, 10);
    EXPECT_EQ(spaced[1].y, 36);
}

TEST(Text, ASpaceAdvancesThePenAndDrawsNothing) {
    const std::vector<Sprite> sprites = TextKit::Text("t", "A B", {.x = 10, .y = 20}, flat());
    EXPECT_EQ(keys(sprites), (std::vector<std::string>{"t:0", "t:2"}));
    EXPECT_EQ(sprites[1].x, 26);
}

TEST(Text, ACharacterIsDrawnByItsOwnCaseOrElseByItsOtherCase) {
    const std::vector<Sprite> upper = TextKit::Text("t", "Hi", {}, flat(kUpper));
    ASSERT_EQ(upper.size(), 2u);
    EXPECT_EQ(upper[0].tile, 7);
    EXPECT_EQ(upper[1].tile, 8);

    const std::vector<Sprite> lower = TextKit::Text("t", "HI", {}, flat(kLower));
    ASSERT_EQ(lower.size(), 2u);
    EXPECT_EQ(lower[0].tile, 7);   // 'h'
    EXPECT_EQ(lower[1].tile, 8);   // 'i'

    const std::vector<Sprite> mixed = TextKit::Text("t", "Hi", {}, flat(kMixed));
    ASSERT_EQ(mixed.size(), 2u);
    EXPECT_EQ(mixed[0].tile, 14);  // 'H'
    EXPECT_EQ(mixed[1].tile, 17);  // 'i'
}

TEST(Text, ACharacterTheFontLacksThrowsNamingItShownOrNot) {
    try {
        (void)TextKit::Text("t", "A%", {}, font());
        FAIL() << "expected std::out_of_range";
    } catch (const std::out_of_range& error) {
        EXPECT_NE(std::string(error.what()).find("'%'"), std::string::npos) << error.what();
    }
    EXPECT_THROW((void)TextKit::Text("t", "A%", {}, font(), {.shown = 1}), std::out_of_range);
}

TEST(Text, OnlyTheCharactersShownAreDrawn) {
    const std::vector<Sprite> three = TextKit::Text("t", "HELLO", {}, font(), {.shown = 3});
    EXPECT_EQ(keys(three), (std::vector<std::string>{"t:0:shadow", "t:0", "t:1:shadow", "t:1", "t:2:shadow", "t:2"}));
    EXPECT_EQ(TextKit::Text("t", "HELLO", {}, font()).size(), 10u);
}

TEST(Text, EachLineIsAlignedWithinTheWidth) {
    const auto lineStarts = [](TextKit::Align align, int width) {
        const std::vector<Sprite> sprites =
            TextKit::Text("t", "AB\nABCD", {.x = 10, .y = 0}, flat(), {.align = align, .width = width});
        return std::vector<int>{find(sprites, "t:0")->x, find(sprites, "t:3")->x};
    };
    EXPECT_EQ(lineStarts(TextKit::Align::Left, 64), (std::vector<int>{10, 10}));
    EXPECT_EQ(lineStarts(TextKit::Align::Center, 64), (std::vector<int>{10 + 24, 10 + 16}));
    EXPECT_EQ(lineStarts(TextKit::Align::Right, 64), (std::vector<int>{10 + 48, 10 + 32}));
    EXPECT_EQ(lineStarts(TextKit::Align::Center, 0), (std::vector<int>{10 + 8, 10}));
}

TEST(Text, TheGlyphsSitAtTheGivenZAndTheirShadowsOneBelow) {
    const std::vector<Sprite> sprites = TextKit::Text("t", "A", {}, font(), {.z = 5});
    EXPECT_EQ(find(sprites, "t:0")->z, 5);
    EXPECT_EQ(find(sprites, "t:0:shadow")->z, 4);
}

// ── wrap and measure ─────────────────────────────────────────────────────────────────────────

TEST(Wrap, BreaksAtSpacesSoNoLineIsLongerThanTheColumns) {
    EXPECT_EQ(TextKit::wrap("the quick brown fox", 9), "the quick\nbrown fox");
    EXPECT_EQ(TextKit::wrap("to abcdefghijkl and", 9), "to\nabcdefghijkl\nand");   // a long word runs past
    EXPECT_EQ(TextKit::wrap("the\nquick brown fox", 9), "the\nquick\nbrown fox");   // a newline is kept
    EXPECT_EQ(TextKit::wrap("the quick", 9), "the quick");                          // it fits
    EXPECT_EQ(TextKit::wrap("the quick   brown", 9), "the quick\nbrown");           // the spaces at a break go
    EXPECT_EQ(TextKit::wrap("a  b", 9), "a  b");                                    // spaces elsewhere stay
}

TEST(Measure, IsTheLongestLineAcrossAndTheLineCountDown) {
    EXPECT_EQ(TextKit::measure("AB\nABCD\nA", flat()), (retropp::PixelSize{.width = 32, .height = 24}));
    EXPECT_EQ(TextKit::measure("A B", flat()), (retropp::PixelSize{.width = 24, .height = 8}));
    EXPECT_EQ(TextKit::measure("", flat()), (retropp::PixelSize{.width = 0, .height = 8}));
}

// ── Reveal ───────────────────────────────────────────────────────────────────────────────────

TEST(Reveal, ShowsOneCharacterPerTicksPerGlyphUntilSkipped) {
    TextKit::Reveal reveal;
    EXPECT_EQ(reveal.shown(), 0u);
    reveal.advance();
    reveal.advance();
    EXPECT_EQ(reveal.shown(), 1u);
    EXPECT_FALSE(reveal.done(2));
    for (int i = 0; i < 3; ++i) {
        reveal.advance();
    }
    EXPECT_EQ(reveal.shown(), 2u);
    EXPECT_TRUE(reveal.done(2));
    EXPECT_FALSE(reveal.done(100));
    reveal.skip();
    EXPECT_EQ(reveal.shown(), std::string_view::npos);
    EXPECT_TRUE(reveal.done(100));

    for (const std::uint32_t ticksPerGlyph : {0u, 1u}) {
        TextKit::Reveal quick;
        quick.ticksPerGlyph = ticksPerGlyph;
        quick.advance();
        EXPECT_EQ(quick.shown(), 1u) << ticksPerGlyph;
        quick.advance();
        EXPECT_EQ(quick.shown(), 2u) << ticksPerGlyph;
    }
}

// ── glyphOf ──────────────────────────────────────────────────────────────────────────────────

TEST(GlyphOf, FindsTheCharacterOrItsOtherCaseAndNeverSpace) {
    EXPECT_EQ(TextKit::glyphOf(font(), 'C'), std::optional<std::size_t>{2});
    EXPECT_EQ(TextKit::glyphOf(font(), 'c'), std::optional<std::size_t>{2});
    EXPECT_EQ(TextKit::glyphOf(font(), '7'), std::optional<std::size_t>{33});
    EXPECT_EQ(TextKit::glyphOf(font(), '%'), std::nullopt);
    EXPECT_EQ(TextKit::glyphOf(font(), ' '), std::nullopt);
    EXPECT_EQ(TextKit::glyphOf(font("AB C"), ' '), std::nullopt);   // a blank cell listed for space
    EXPECT_EQ(TextKit::glyphOf(font("AB C"), 'C'), std::optional<std::size_t>{3});
}

}  // namespace
