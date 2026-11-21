// Where a box other than the one the camera follows goes through an opening, as WorldKit works it out:
// an empty step while the box is inside its area, beside an opening or at one that leads nowhere; through a
// scroll, carried in and into the next area's pixels, where a camera leaves the box it follows; across a
// seam and onto a spot, the move a camera hands the box it follows; one cell in from a wall entrance;
// into another layout with the leaving layout's carry. The world is built in code, and the tests run headless.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

namespace {

using retropp::IntRect;
using retropp::Vec2i;
using WorldKit::Place;
using WorldKit::Side;
using WorldKit::Step;
using WorldKit::Transition;

enum class Cell : std::uint16_t { Wall = 0, Open = 1 };

constexpr int kCell = 8;
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

retropp::IndexGrid room(int width, int height, const std::vector<Vec2i>& doorways = {}) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            map.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(edge ? Cell::Wall : Cell::Open);
        }
    }
    for (const Vec2i door : doorways) {
        map.values[static_cast<std::size_t>(door.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(door.x)] =
            static_cast<std::uint16_t>(Cell::Open);
    }
    return map;
}

std::vector<Vec2i> column(int x, int first, int last) {
    std::vector<Vec2i> cells;
    for (int y = first; y <= last; ++y) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<Vec2i> row(int y, int first, int last) {
    std::vector<Vec2i> cells;
    for (int x = first; x <= last; ++x) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<Vec2i> joined(std::vector<std::vector<Vec2i>> parts) {
    std::vector<Vec2i> cells;
    for (const std::vector<Vec2i>& part : parts) {
        cells.insert(cells.end(), part.begin(), part.end());
    }
    return cells;
}

retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Layout 0, the keep, four areas. 0, the court, one screen: an east door scrolling into 1; its north wall
// joined seamlessly to 2, two cells to the right; stairs at (12, 4) fading onto 3's stairs; a well at
// (4, 10) fading to 1's west door; a postern in its west wall that leads nowhere; a south door scrolling
// into layout 1. 1, the hall, one screen, its west door back. 2, the garden, 24 × 12 cells, its south wall
// back. 3, the cellar, its stairs back up. Layout 1, the moat, one area, the bank, its north door back up,
// with a carry of its own.
struct Maps {
    retropp::IndexGrid court = room(20, 16, joined({column(19, 6, 9), row(0, 1, 18), column(0, 6, 9), row(15, 8, 11)}));
    retropp::IndexGrid hall = room(20, 16, column(0, 6, 9));
    retropp::IndexGrid garden = room(24, 12, row(11, 3, 20));
    retropp::IndexGrid cellar = room(12, 10);
    retropp::IndexGrid bank = room(20, 16, row(0, 8, 11));
};

enum Keep : std::uint8_t { Court, Hall, Garden, Cellar };

WorldKit::Area area(std::string name, const retropp::IndexGrid& map, std::vector<WorldKit::Opening> openings) {
    return WorldKit::Area{.name = std::move(name),
                          .layers = {{.name = "ground", .tiles = &map, .art = art()}},
                          .collision = &map,
                          .openings = std::move(openings)};
}

WorldKit::World worldOver(const Maps& maps) {
    return WorldKit::World{.layouts = {
        WorldKit::Layout{
            .name = "keep",
            .areas = {
                area("court", maps.court,
                     {{.name = "east door", .side = Side::East, .span = {6, 9}, .transition = Transition::Scroll,
                       .leadsTo = {.area = Keep::Hall, .opening = "west door"}},
                      {.name = "north wall", .side = Side::North, .span = {1, 18}, .transition = Transition::Seamless,
                       .leadsTo = {.area = Keep::Garden, .opening = "south wall", .offset = {.x = 2, .y = 0}}},
                      {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1}, .transition = Transition::Fade,
                       .leadsTo = {.area = Keep::Cellar, .opening = "stairs"}},
                      {.name = "well", .at = {.x = 4, .y = 10, .width = 2, .height = 1}, .transition = Transition::Fade,
                       .leadsTo = {.area = Keep::Hall, .opening = "west door"}},
                      {.name = "postern", .side = Side::West, .span = {6, 9}},
                      {.name = "south door", .side = Side::South, .span = {8, 11}, .transition = Transition::Scroll,
                       .leadsTo = {.layout = 1, .area = 0, .opening = "north door"}}}),
                area("hall", maps.hall,
                     {{.name = "west door", .side = Side::West, .span = {6, 9}, .transition = Transition::Scroll,
                       .leadsTo = {.area = Keep::Court, .opening = "east door"}}}),
                area("garden", maps.garden,
                     {{.name = "south wall", .side = Side::South, .span = {3, 20}, .transition = Transition::Seamless,
                       .leadsTo = {.area = Keep::Court, .opening = "north wall", .offset = {.x = -2, .y = 0}}}}),
                area("cellar", maps.cellar,
                     {{.name = "stairs", .at = {.x = 5, .y = 4, .width = 2, .height = 1}, .transition = Transition::Fade,
                       .leadsTo = {.area = Keep::Court, .opening = "stairs"}}}),
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
        WorldKit::Layout{
            .name = "moat",
            .areas = {area("bank", maps.bank,
                           {{.name = "north door", .side = Side::North, .span = {8, 11}, .transition = Transition::Scroll,
                             .leadsTo = {.layout = 0, .area = Keep::Court, .opening = "south door"}}})},
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
            .carry = 24,
        },
    }};
}

constexpr Place kCourt{.layout = 0, .area = Keep::Court};

// A 10×8 box at (x, y).
IntRect box(int x, int y) {
    return IntRect{.x = x, .y = y, .width = 10, .height = 8};
}

bool empty(const Step& step) {
    return step.move == Vec2i{} && !step.entered.has_value();
}

// The moves a camera hands the box it follows, from the tick its center goes from `was` to `box` to the
// tick it enters the next place, summed, the box carried as a game carries it.
Step followedThrough(const WorldKit::World& world, IntRect was, IntRect box) {
    WorldKit::Camera camera{kCourt.layout, kCourt.area};
    camera.place(world, was);
    Step total{};
    IntRect carried = box;
    for (int tick = 0; tick < 200 && !total.entered; ++tick) {
        const Step step = camera.follow(world, carried);
        carried.x += step.move.x;
        carried.y += step.move.y;
        total.move = {.x = total.move.x + step.move.x, .y = total.move.y + step.move.y};
        total.entered = step.entered;
    }
    return total;
}

class Landing : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);

    void SetUp() override { WorldKit::validate(world); }
};

TEST_F(Landing, NothingWhileTheBoxsCenterIsInsideTheArea) {
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(70, 60), box(71, 60))));
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(149, 60), box(150, 60))));   // up to the east door
}

TEST_F(Landing, NothingPastAWallBesideAnOpeningOrThroughOneThatLeadsNowhere) {
    // Center (160, 24): past the east wall at row 3, outside the door's rows 6–9.
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(154, 20), box(155, 20))));
    // Center (−1, 64): out through the postern, which leads nowhere.
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(-4, 60), box(-6, 60))));
}

// The hall's top-left is (160, 0) in the court's pixels: carried 16 px east, then 160 px back.
TEST_F(Landing, AScrollCarriesTheBoxInAndIntoTheNextAreasPixels) {
    const Step step = WorldKit::detail::through(world, kCourt, box(154, 60), box(155, 60));   // center (160, 64)
    EXPECT_EQ(step.move, (Vec2i{16 - 160, 0}));
    EXPECT_EQ(step.entered, (Place{.layout = 0, .area = Keep::Hall}));
}

TEST_F(Landing, AScrollLandsTheBoxWhereACameraLeavesTheBoxItFollows) {
    const Step step = WorldKit::detail::through(world, kCourt, box(154, 60), box(155, 60));
    const Step followed = followedThrough(world, box(154, 60), box(155, 60));
    EXPECT_EQ(step.move, followed.move);
    EXPECT_EQ(step.entered, followed.entered);
}

// The garden sits above the court, its south span (cells 3–20) level with the court's north span
// (1–18), then two cells right: its top-left is (0, −96) in the court's pixels.
TEST_F(Landing, ASeamMovesTheBoxIntoTheNextAreasPixelsAsTheCameraDoes) {
    const Step step = WorldKit::detail::through(world, kCourt, box(70, 1), box(70, -5));   // center (75, −1)
    EXPECT_EQ(step.move, (Vec2i{0, 96}));
    EXPECT_EQ(step.entered, (Place{.layout = 0, .area = Keep::Garden}));

    WorldKit::Camera camera{kCourt.layout, kCourt.area};
    camera.place(world, box(70, 1));
    const Step followed = camera.follow(world, box(70, -5));
    EXPECT_EQ(step.move, followed.move);
    EXPECT_EQ(step.entered, followed.entered);
}

// The court's stairs are pixels 96–111 × 32–39; the cellar's are 40–55 × 32–39.
TEST_F(Landing, ASpotLandsTheBoxCenteredOnTheEntranceAsTheCameraDoesAtTheMiddleOfItsFade) {
    const Step step = WorldKit::detail::through(world, kCourt, box(95, 50), box(95, 32));   // center into the stairs
    EXPECT_EQ(step.move, (Vec2i{43 - 95, 32 - 32}));
    EXPECT_EQ(step.entered, (Place{.layout = 0, .area = Keep::Cellar}));
    const Step followed = followedThrough(world, box(95, 50), box(95, 32));
    EXPECT_EQ(step.move, followed.move);
    EXPECT_EQ(step.entered, followed.entered);
}

TEST_F(Landing, ABoxAlreadyOnASpotOrHandedInTwiceStays) {
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(95, 32), box(96, 32))));    // on the stairs, still
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(95, 32), box(95, 32))));
    EXPECT_TRUE(empty(WorldKit::detail::through(world, kCourt, box(155, 60), box(155, 60))));  // past the east door, still
}

// The well fades to the hall's west door, rows 6–9 (pixels 48–79, center 64).
TEST_F(Landing, AFadeIntoAWallEntranceLandsOneCellInCenteredAlongItsSpan) {
    const Step step = WorldKit::detail::through(world, kCourt, box(35, 90), box(35, 80));   // center (40, 84): in the well
    EXPECT_EQ(step.move, (Vec2i{8 - 35, 60 - 80}));
    EXPECT_EQ(step.entered, (Place{.layout = 0, .area = Keep::Hall}));
}

// The bank sits below the court, 128 px down; the keep's carry is 16, the moat's 24.
TEST_F(Landing, IntoAnotherLayoutTheCarryIsTheLeavingLayouts) {
    const Step step = WorldKit::detail::through(world, kCourt, box(70, 119), box(70, 124));   // center (75, 128)
    EXPECT_EQ(step.move, (Vec2i{0, 16 - 128}));
    EXPECT_EQ(step.entered, (Place{.layout = 1, .area = 0}));
    const Step followed = followedThrough(world, box(70, 119), box(70, 124));
    EXPECT_EQ(step.move, followed.move);
    EXPECT_EQ(step.entered, followed.entered);
}

}  // namespace
