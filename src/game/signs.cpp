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

constexpr std::array<Sign, 2> kSigns{{
    {.layout = Layout::Overworld, .area = Area::Meadow, .cell = {.x = 6, .y = 4}, .material = Material::Wood,
     .text = kMeadowSign},
    {.layout = Layout::Overworld, .area = Area::Lake, .cell = {.x = 28, .y = 2}, .material = Material::Stone,
     .text = kDungeonRock},
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
