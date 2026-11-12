// Text — a passage as sprites, one per glyph, and the wrapping and measuring that place it.
//
//     LayerKit::SpriteLayer("hud", kHudZ, {TextKit::Text("title", "THE CAVE", {.x = 8, .y = 4}, font)});
//
//     const std::string message = TextKit::wrap(passage, 18);
//     TextKit::Text("message", message, {.x = 8, .y = 96}, font, {.shown = reveal.shown()});
//
// This is the one TextKit header that builds draw types (retropp::Sprite). The sprites are in the
// pixels of whatever sprite layer they are put on.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "TextKit/font.h"

namespace TextKit {

// Where each line sits within the width a passage is laid out in.
enum class Align : std::uint8_t { Left, Center, Right };

struct TextOptions {
    std::size_t shown = std::string_view::npos;   // draw only the first n characters of the passage
    Align align = Align::Left;
    int width = 0;                                // the width lines align within; 0 is the passage's own
    std::int32_t z = 0;                           // the glyphs' z within their layer; shadows sit one below
};

namespace detail {

// The number of characters in each line of `text`: a passage with n '\n's has n + 1 lines, empty ones
// included.
[[nodiscard]] inline std::vector<std::size_t> lineLengths(std::string_view text) {
    std::vector<std::size_t> lengths{0};
    for (const char character : text) {
        if (character == '\n') {
            lengths.push_back(0);
        } else {
            ++lengths.back();
        }
    }
    return lengths;
}

[[nodiscard]] inline std::string describe(char character) {
    return std::string{'\''} + character + '\'';
}

}  // namespace detail

// The pixels a passage takes in `font`: its longest line times `advance.x` across, its line count
// times `advance.y` down. Spaces count as characters; a passage with n '\n's has n + 1 lines, so an
// empty passage is one line tall and no pixels wide.
[[nodiscard]] inline retropp::PixelSize measure(std::string_view text, const Font& font) noexcept {
    std::size_t longest = 0;
    std::size_t current = 0;
    int lines = 1;
    for (const char character : text) {
        if (character == '\n') {
            ++lines;
            current = 0;
        } else if (++current > longest) {
            longest = current;
        }
    }
    return retropp::PixelSize{.width = static_cast<int>(longest) * font.advance.x,
                              .height = lines * font.advance.y};
}

// A passage as sprites, one per glyph — two with a shadow — laid out from `at`: the pen starts at
// `at`, steps `advance.x` per character, and '\n' starts the next line `advance.y` down. Each line is
// placed within `options.width` (the passage's measured width when 0) by `options.align`: Center moves
// it right by half the room left over (integer division) and Right by all of it, so a line wider than
// the width moves left. A space advances the pen and draws nothing.
//
// Character n of the passage is keyed "<key>:<n>" and its shadow "<key>:<n>:shadow", counting every
// character as written, spaces and '\n' included, so a glyph keeps its key as more of the passage is
// shown and as the passage is laid out again. Each sprite is the glyph's slot of the sheet — its tile
// and its size — at the glyph's top-left, through the font's palette, at `options.z`. A shadow is the
// same slot one pixel right and one down, through the font's shadow palette, at `options.z - 1`, and
// comes before its glyph in the vector.
//
// Only the first `options.shown` characters are drawn; every character is checked. Throws
// std::out_of_range naming the character when the font has no glyph for it, shown or not, or when its
// slot is past the end of the sheet.
[[nodiscard]] inline std::vector<retropp::Sprite> Text(std::string_view key, std::string_view text,
                                                       retropp::Vec2i at, const Font& font,
                                                       const TextOptions& options = {}) {
    std::vector<std::size_t> slots(text.size(), 0);
    for (std::size_t n = 0; n < text.size(); ++n) {
        const char character = text[n];
        if (character == ' ' || character == '\n') {
            continue;
        }
        const std::optional<std::size_t> slot = glyphOf(font, character);
        if (!slot) {
            throw std::out_of_range("TextKit::Text: the font has no glyph for " + detail::describe(character));
        }
        if (*slot >= font.sheet.slots.size()) {
            throw std::out_of_range("TextKit::Text: the glyph for " + detail::describe(character) +
                                    " is past the end of the font's sheet");
        }
        slots[n] = *slot;
    }

    const std::vector<std::size_t> lengths = detail::lineLengths(text);
    const int width = options.width != 0 ? options.width : measure(text, font).width;
    const auto lineOffset = [&](std::size_t line) {
        const int room = width - static_cast<int>(lengths[line]) * font.advance.x;
        switch (options.align) {
        case Align::Center:
            return room / 2;
        case Align::Right:
            return room;
        case Align::Left:
            break;
        }
        return 0;
    };

    std::vector<retropp::Sprite> sprites;
    const std::size_t count = options.shown < text.size() ? options.shown : text.size();
    std::size_t line = 0;
    int column = 0;
    int offset = lineOffset(0);
    for (std::size_t n = 0; n < count; ++n) {
        const char character = text[n];
        if (character == '\n') {
            ++line;
            column = 0;
            offset = lineOffset(line);
            continue;
        }
        if (character != ' ') {
            const retropp::AssetSlot& slot = font.sheet.slots[slots[n]];
            const int x = at.x + offset + column * font.advance.x;
            const int y = at.y + static_cast<int>(line) * font.advance.y;
            const std::string glyphKey = std::string(key) + ":" + std::to_string(n);
            if (font.shadow) {
                sprites.push_back(retropp::Sprite{
                    .key = glyphKey + ":shadow",
                    .x = x + 1,
                    .y = y + 1,
                    .z = options.z - 1,
                    .size = slot.dimensions,
                    .atlas = font.sheet.atlasId,
                    .tile = slot.tile,
                    .palette = *font.shadow,
                });
            }
            sprites.push_back(retropp::Sprite{
                .key = glyphKey,
                .x = x,
                .y = y,
                .z = options.z,
                .size = slot.dimensions,
                .atlas = font.sheet.atlasId,
                .tile = slot.tile,
                .palette = font.palette,
            });
        }
        ++column;
    }
    return sprites;
}

// `text` with '\n' put in at spaces so that no line is longer than `columns` characters. A '\n'
// already in it is kept; the run of spaces where a line is broken is dropped; spaces anywhere else
// are kept; a word longer than `columns` takes a line to itself and runs past. A `columns` below 1
// puts each word on a line of its own.
[[nodiscard]] inline std::string wrap(std::string_view text, int columns) {
    const std::size_t limit = columns > 0 ? static_cast<std::size_t>(columns) : 0;
    std::string out;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find('\n', start);
        const std::string_view paragraph =
            text.substr(start, end == std::string_view::npos ? std::string_view::npos : end - start);

        std::string current;
        std::size_t i = 0;
        while (i < paragraph.size()) {
            const std::size_t wordStart = paragraph.find_first_not_of(' ', i);
            const std::string_view gap = paragraph.substr(i, (wordStart == std::string_view::npos
                                                                  ? paragraph.size()
                                                                  : wordStart) - i);
            if (wordStart == std::string_view::npos) {
                // Spaces closing the paragraph stay when they fit.
                if (current.size() + gap.size() <= limit) {
                    current += gap;
                }
                break;
            }
            const std::size_t wordEnd = std::min(paragraph.find(' ', wordStart), paragraph.size());
            const std::string_view word = paragraph.substr(wordStart, wordEnd - wordStart);
            const bool lineStart = current.empty();
            if (lineStart || current.size() + gap.size() + word.size() <= limit) {
                current += gap;
                current += word;
            } else {
                out += current;
                out += '\n';
                current = word;
            }
            i = wordEnd;
        }
        out += current;

        if (end == std::string_view::npos) {
            break;
        }
        out += '\n';
        start = end + 1;
    }
    return out;
}

}  // namespace TextKit
