// The things in the world that can be read: a sign, a rock with words cut into it. Each stands on one
// cell of an area, which its map marks solid, and is read by pressing Item while facing it.
#pragma once

#include <span>
#include <string_view>

#include "retropp/geometry.h"
#include "retropp/input_actions.h"

#include "game/areas.h"

namespace zorlduh {

// What a readable thing is made of: the border its message is drawn in.
enum class Material { Wood, Stone };

// How a readable thing's message box opens and closes: grown from its middle, revealed through a
// rectangle growing from its middle, squashed to a line across its middle, faded, slid up from below
// the screen, or standing as it is.
enum class MessageStyle { Grow, Clip, Squash, Fade, Slide, Pop };

// A readable thing: where it stands, what it is made of, how its message box opens and closes, and what
// it says.
struct Sign {
    Layout layout;
    int area;              // an Area in the overworld, a Room in DungeonOne, a Woods in SeamlessForest
    retropp::Vec2i cell;   // the cell it stands on, in the area's cells
    Material material;
    MessageStyle style = MessageStyle::Grow;
    std::string_view text;
};

// Every readable thing in the world.
[[nodiscard]] std::span<const Sign> signs() noexcept;

// The one a walker reads from where it stands: the one on the cell holding the point one pixel beyond
// the middle of the side of `box` it faces, in `area` of `layout`. nullptr when that cell holds none.
// `box` is the walker's collision box in the area's pixels.
[[nodiscard]] const Sign* signFaced(Layout layout, int area, retropp::IntRect box, retropp::Dir facing) noexcept;

}  // namespace zorlduh
