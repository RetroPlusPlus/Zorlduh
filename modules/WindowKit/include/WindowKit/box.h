// Box — a rectangle of any size filled and framed by a border's pieces, as sprites.
//
//     LayerKit::SpriteLayer("panel", kHudZ, {WindowKit::Box("panel", {.x = 4, .y = 4, .width = 93, .height = 41}, border)});
//
// The box is in the pixels of the layer its sprites are put on. Pieces repeat to fill it, and a piece
// with less room than its own size is cut: the part past the room it has is not drawn.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "WindowKit/border.h"

namespace WindowKit {

namespace detail {

// One piece at (x, y), cut to `keep`: each part of the piece outside `keep` — right, below, left,
// above, in that order — is covered by a region of the piece's own that leaves it undrawn.
[[nodiscard]] inline retropp::Sprite piece(std::string key, const Border& border, Piece which, int x, int y,
                                           std::int32_t z, retropp::IntRect keep) {
    const retropp::AssetSlot& slot = border.sheet.slots[static_cast<std::size_t>(which)];
    retropp::Sprite sprite{
        .key = std::move(key),
        .x = x,
        .y = y,
        .z = z,
        .size = slot.dimensions,
        .atlas = border.sheet.atlasId,
        .tile = slot.tile,
        .palette = border.palette,
    };
    const int width = slot.dimensions.width;
    const int height = slot.dimensions.height;
    const int keepLeft = keep.x - x;
    const int keepTop = keep.y - y;
    const int keepRight = keep.x + keep.width - x;
    const int keepBottom = keep.y + keep.height - y;
    const auto cut = [&](int left, int top, int right, int bottom) {
        if (right <= left || bottom <= top) {
            return;
        }
        sprite.regions.push_back(retropp::Region{
            .key = "cut",
            .shape = retropp::ShapePoints::rectangle(
                retropp::Point{static_cast<float>(left), static_cast<float>(top)},
                static_cast<float>(right - left), static_cast<float>(bottom - top)),
            .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::Transparency}},
        });
    };
    cut(std::max(keepRight, 0), 0, width, height);
    cut(0, std::max(keepBottom, 0), width, height);
    cut(0, 0, std::min(keepLeft, width), height);
    cut(0, 0, width, std::min(keepTop, height));
    return sprite;
}

}  // namespace detail

// A box as sprites: `box` filled and framed by the border's pieces, in the pixels of the layer the
// sprites are put on.
//
// With `t` the border's thickness (its top-left piece's size): the four corners sit at the box's four
// corners; the top and bottom edges repeat left to right across [box.x + t.width, box.x + box.width −
// t.width), each repeat its slot's width; the left and right edges repeat top to bottom across [box.y +
// t.height, box.y + box.height − t.height), each its slot's height; the fill repeats across
// inside(box, border), left to right, then down. The right and bottom edges sit against the box's
// right and bottom.
//
// A repeat that runs past the end of its span — an edge's last, the fill's last column and last row —
// is cut there: it carries a region keyed "cut", a rectangle over the part past the end in the
// sprite's own quad, with one Transparency effect, so that part is not drawn. A fill piece cut on both
// axes carries two, the right part, then the bottom part. Every piece is also cut to the box, so a box
// narrower or shorter than a corner cuts its corners. A box narrower than two corners draws them
// overlapping, the right over the left; one shorter than two, the bottom over the top. A span of no
// length draws no repeat.
//
// Returned fill first, then the edges (top, bottom, left, right), then the corners (top-left,
// top-right, bottom-left, bottom-right), all at `z`, so the corners draw over the edges and the edges
// over the fill. Each sprite is its slot's tile and size from the border's sheet, through the border's
// palette.
//
// Keys name each piece's place in the box and the box's size: "<key>:fill:<column>:<row>:<size>",
// "<key>:top:<n>:<size>", "<key>:bottom:<n>:<size>", "<key>:left:<n>:<size>", "<key>:right:<n>:<size>",
// "<key>:top-left:<size>", "<key>:top-right:<size>", "<key>:bottom-left:<size>" and
// "<key>:bottom-right:<size>", where <size> is "<width>x<height>". The renderer eases a sprite's
// position between ticks by its key but never its regions, so a box drawn at a new size is drawn whole
// on the tick it changes, every piece in place with its cut, wherever the box grows from. A box that
// moves at one size keeps every key, and all of it eases together.
//
// A box with no width or no height, or a border whose sheet has fewer than nine slots, is no sprites.
// Box never throws.
[[nodiscard]] inline std::vector<retropp::Sprite> Box(std::string_view key, retropp::IntRect box,
                                                      const Border& border, std::int32_t z = 0) {
    std::vector<retropp::Sprite> sprites;
    if (box.width <= 0 || box.height <= 0 || border.sheet.slots.size() < 9) {
        return sprites;
    }
    const auto size = [&](Piece which) { return border.sheet.slots[static_cast<std::size_t>(which)].dimensions; };
    const auto name = [&](std::string_view part) { return std::string(key) + ":" + std::string(part); };
    const std::string sized = ":" + std::to_string(box.width) + "x" + std::to_string(box.height);
    const retropp::PixelSize t = thickness(border);
    const int right = box.x + box.width;
    const int bottom = box.y + box.height;
    const int spanLeft = box.x + t.width;
    const int spanRight = right - t.width;
    const int spanTop = box.y + t.height;
    const int spanBottom = bottom - t.height;

    const retropp::IntRect room = inside(box, border);
    const retropp::AssetDimensions fill = size(Piece::Fill);
    if (fill.width > 0 && fill.height > 0) {
        int row = 0;
        for (int y = room.y; y < room.y + room.height; y += fill.height, ++row) {
            int column = 0;
            for (int x = room.x; x < room.x + room.width; x += fill.width, ++column) {
                sprites.push_back(detail::piece(
                    name("fill:" + std::to_string(column) + ":" + std::to_string(row) + sized), border, Piece::Fill,
                    x, y, z, room));
            }
        }
    }

    const auto across = [&](Piece which, std::string_view part, int y) {
        const int step = size(which).width;
        if (step <= 0) {
            return;
        }
        const retropp::IntRect keep{.x = spanLeft, .y = box.y, .width = spanRight - spanLeft, .height = box.height};
        int n = 0;
        for (int x = spanLeft; x < spanRight; x += step, ++n) {
            sprites.push_back(detail::piece(name(std::string(part) + ":" + std::to_string(n) + sized),
                                            border, which, x, y, z, keep));
        }
    };
    const auto down = [&](Piece which, std::string_view part, int x) {
        const int step = size(which).height;
        if (step <= 0) {
            return;
        }
        const retropp::IntRect keep{.x = box.x, .y = spanTop, .width = box.width, .height = spanBottom - spanTop};
        int n = 0;
        for (int y = spanTop; y < spanBottom; y += step, ++n) {
            sprites.push_back(detail::piece(name(std::string(part) + ":" + std::to_string(n) + sized),
                                            border, which, x, y, z, keep));
        }
    };
    across(Piece::Top, "top", box.y);
    across(Piece::Bottom, "bottom", bottom - size(Piece::Bottom).height);
    down(Piece::Left, "left", box.x);
    down(Piece::Right, "right", right - size(Piece::Right).width);

    sprites.push_back(detail::piece(name("top-left" + sized), border, Piece::TopLeft, box.x, box.y, z, box));
    sprites.push_back(detail::piece(name("top-right" + sized), border, Piece::TopRight,
                                    right - size(Piece::TopRight).width, box.y, z, box));
    sprites.push_back(detail::piece(name("bottom-left" + sized), border, Piece::BottomLeft, box.x,
                                    bottom - size(Piece::BottomLeft).height, z, box));
    sprites.push_back(detail::piece(name("bottom-right" + sized), border, Piece::BottomRight,
                                    right - size(Piece::BottomRight).width, bottom - size(Piece::BottomRight).height,
                                    z, box));
    return sprites;
}

}  // namespace WindowKit
