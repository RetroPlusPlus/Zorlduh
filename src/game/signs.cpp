#include "game/signs.h"

#include <array>

namespace zorlduh {

namespace {

constexpr std::string_view kMeadowSign =
    "Welcome to the meadow. The lake lies east through the door, and the field south along the road. "
    "A cave opens in the rock to the north. Mind the stairs: they lead down into the dark, and what waits "
    "there is not friendly. Press the button again to close this sign.";

constexpr std::string_view kDungeonRock =
    "Here the dungeon begins. Those who go down should go ready: the dark below does not forgive the "
    "careless.";

// The shrine's six signs, on a grid of three columns and two rows, west to east along the north row and
// then the south, each read in a message box that opens and closes in its own style.
constexpr std::string_view kGrowSign =
    "Grow. This box grows out of its middle as a whole box, border and all, and shrinks back into it.";
constexpr std::string_view kClipSign =
    "Clip. This box is all there from the start, seen through a window that opens from its middle.";
constexpr std::string_view kSquashSign =
    "Squash. This box opens out from a line across its middle, its border stretched with it.";
constexpr std::string_view kFadeSign = "Fade. This box fades in where it stands, and fades out again.";
constexpr std::string_view kSlideSign = "Slide. This box rises from below the screen, and sinks back down.";
constexpr std::string_view kPopSign =
    "Pop. This box stands as it is while it opens and closes. The words wait until it is open.";

constexpr std::array<Sign, 8> kSigns{{
    {.layout = Layout::Overworld, .area = Area::Meadow, .cell = {.x = 6, .y = 4}, .material = Material::Wood,
     .text = kMeadowSign},
    {.layout = Layout::Overworld, .area = Area::Lake, .cell = {.x = 28, .y = 2}, .material = Material::Stone,
     .text = kDungeonRock},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 3, .y = 2}, .material = Material::Wood,
     .style = MessageStyle::Grow, .text = kGrowSign},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 6, .y = 2}, .material = Material::Wood,
     .style = MessageStyle::Clip, .text = kClipSign},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 9, .y = 2}, .material = Material::Wood,
     .style = MessageStyle::Squash, .text = kSquashSign},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 3, .y = 6}, .material = Material::Wood,
     .style = MessageStyle::Fade, .text = kFadeSign},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 6, .y = 6}, .material = Material::Wood,
     .style = MessageStyle::Slide, .text = kSlideSign},
    {.layout = Layout::Overworld, .area = Area::Shrine, .cell = {.x = 9, .y = 6}, .material = Material::Wood,
     .style = MessageStyle::Pop, .text = kPopSign},
}};

// Division rounding toward negative infinity, so a point left of or above an area is in a cell
// outside it.
int floorDiv(int a, int b) noexcept {
    const int q = a / b;
    return (a % b != 0 && (a < 0) != (b < 0)) ? q - 1 : q;
}

}  // namespace

std::span<const Sign> signs() noexcept {
    return kSigns;
}

const Sign* signFaced(Layout layout, int area, retropp::IntRect box, retropp::Dir facing) noexcept {
    retropp::Vec2i point{.x = box.x + box.width / 2, .y = box.y + box.height / 2};
    switch (facing) {
    case retropp::Dir::Up:
        point.y = box.y - 1;
        break;
    case retropp::Dir::Down:
        point.y = box.y + box.height;
        break;
    case retropp::Dir::Left:
        point.x = box.x - 1;
        break;
    case retropp::Dir::Right:
        point.x = box.x + box.width;
        break;
    case retropp::Dir::None:
        return nullptr;
    }
    const retropp::Vec2i cell{.x = floorDiv(point.x, kCellSize), .y = floorDiv(point.y, kCellSize)};
    for (const Sign& sign : kSigns) {
        if (sign.layout == layout && sign.area == area && sign.cell == cell) {
            return &sign;
        }
    }
    return nullptr;
}

}  // namespace zorlduh
