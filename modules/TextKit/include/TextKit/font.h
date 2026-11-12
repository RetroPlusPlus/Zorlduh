// Font — a glyph sheet and the character each of its slots is.
//
//     const TextKit::Font font{.sheet = glyphSheet, .glyphs = "ABCDEFGHIJKLMNOPQRSTUVWXYZ",
//                              .palette = white, .shadow = black};
//
// The sheet is an AtlasManifest as the renderer's loadAtlas or uploadAtlas returns it: its slots are
// the glyphs, in read order, each with its tile and its size. The font adds what the sheet cannot say,
// which character each slot draws.
#pragma once

#include <cctype>
#include <cstddef>
#include <optional>
#include <string_view>

#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/palette.h"

namespace TextKit {

// A font: a glyph sheet carved into slots, which character each slot is, and how the glyphs are
// colored. `glyphs` is the sheet's slot order written as a string — character n is what slot n draws
// — so a sheet in any order is described by writing its string in that order. A character is drawn
// by its own glyph when the font has one; by the glyph of its other case when the font has only that
// (an uppercase-only sheet draws "Hello" as HELLO, a lowercase-only one draws "HELLO" as hello, a mixed
// sheet draws each as written); a character the font has in neither case has no glyph. Space draws
// nothing: it advances the pen. A sheet may hold a blank cell for it, listed as ' ' in `glyphs` to keep
// the slots after it in place; that cell is never drawn.
//
// `glyphs` is a view: the string it views outlives the font. The sheet is copied in.
struct Font {
    retropp::AtlasManifest sheet;               // a Tileset or SpriteSeries; its transparent index is
                                                // what shows around each glyph
    std::string_view glyphs;                    // the character of each slot, in slot order
    retropp::PaletteId palette{};               // the glyphs' colors
    std::optional<retropp::PaletteId> shadow;   // when set, each glyph is also drawn one pixel right
                                                // and one down through it, beneath the glyph
    retropp::Vec2i advance{.x = 8, .y = 8};     // the pen's step per character, and per line
};

// The slot of `character` in `font` by the rule above: the first slot listing the character, else the
// first listing its other case, else std::nullopt. Space has no slot, even when `glyphs` lists it.
// Only the letters A–Z and a–z have another case.
[[nodiscard]] inline std::optional<std::size_t> glyphOf(const Font& font, char character) noexcept {
    if (character == ' ') {
        return std::nullopt;
    }
    if (const std::size_t slot = font.glyphs.find(character); slot != std::string_view::npos) {
        return slot;
    }
    const auto byte = static_cast<unsigned char>(character);
    if (std::isalpha(byte) == 0) {
        return std::nullopt;
    }
    const char other = static_cast<char>(std::isupper(byte) != 0 ? std::tolower(byte) : std::toupper(byte));
    if (const std::size_t slot = font.glyphs.find(other); slot != std::string_view::npos) {
        return slot;
    }
    return std::nullopt;
}

}  // namespace TextKit
