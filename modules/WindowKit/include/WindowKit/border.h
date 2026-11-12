// Border — a sheet of the pieces a box is drawn from, and the room a box leaves inside it.
//
//     const WindowKit::Border border{.sheet = borderSheet, .palette = borderColors};
//     const retropp::IntRect room = WindowKit::inside(box, border);   // where a window's contents go
//
// The sheet is an AtlasManifest as the renderer's loadAtlas or uploadAtlas returns it. Every size here
// is read from it: a piece is the size its slot is. The header builds no draw type, so a game's
// simulation can hold a Border.
#pragma once

#include <algorithm>
#include <cstddef>

#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/palette.h"

namespace WindowKit {

// The pieces of a border sheet, in the sheet's read order: the box as it looks, three across and three
// down.
enum class Piece : std::size_t {
    TopLeft, Top, TopRight,
    Left, Fill, Right,
    BottomLeft, Bottom, BottomRight,
};

// The slot after the nine pieces, when the sheet has one: the arrow a message box shows while another
// page follows.
inline constexpr std::size_t kArrowSlot = 9;

// A border: a sheet carved into the nine pieces of a box, and the palette they are drawn through. The
// sheet's transparent index is what shows around a rounded corner. A sheet with fewer than nine slots
// draws nothing.
struct Border {
    retropp::AtlasManifest sheet;
    retropp::PaletteId palette{};
};

// The border's thickness: the size of its top-left piece. {0, 0} for a sheet with fewer than nine slots.
[[nodiscard]] inline retropp::PixelSize thickness(const Border& border) noexcept {
    if (border.sheet.slots.size() < 9) {
        return {};
    }
    const retropp::AssetDimensions corner = border.sheet.slots[static_cast<std::size_t>(Piece::TopLeft)].dimensions;
    return retropp::PixelSize{.width = corner.width, .height = corner.height};
}

// `box` with the border's thickness taken off each side: where a window's contents go. Its width and
// height are never negative: a box thinner than two thicknesses leaves 0.
[[nodiscard]] inline retropp::IntRect inside(retropp::IntRect box, const Border& border) noexcept {
    const retropp::PixelSize t = thickness(border);
    return retropp::IntRect{
        .x = box.x + t.width,
        .y = box.y + t.height,
        .width = std::max(0, box.width - 2 * t.width),
        .height = std::max(0, box.height - 2 * t.height),
    };
}

}  // namespace WindowKit
