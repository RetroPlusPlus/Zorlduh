// The WorldKit module: what validate rejects, what is solid in an area, the camera within an area, a
// scroll and a fade tick by tick, the areas shown and their layers, and the fade region. The areas are
// built in code; no window or GPU is needed.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/solids.h"
#include "WorldKit/world_layers.h"

namespace {

using retropp::IntRect;
using retropp::Vec2i;
using WorldKit::Side;
using WorldKit::Transition;

enum class Cell : std::uint16_t { Wall = 0, Open = 1 };

constexpr int kCell = 8;
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

// A map ringed by wall, open inside, with the given wall cells opened as doorways.
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

// Every map value drawn as slot 0 of sheet 7 through palette 4; the map doubles as the tile map.
retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Four areas: 0 is one screen with an east door to 1, a south road to 2 and a stairs spot fading to 3;
// 1 is two screens wide; 2 is one and a half wide with its north road two cells left of 0's south one
// and an east door that is only an entrance; 3 is smaller than the screen in both axes.
struct Maps {
    retropp::IndexGrid a0 = room(20, 16, [] {
        std::vector<Vec2i> doors = column(19, 6, 9);
        const std::vector<Vec2i> south = row(15, 8, 11);
        doors.insert(doors.end(), south.begin(), south.end());
        return doors;
    }());
    retropp::IndexGrid a1 = room(40, 16, column(0, 6, 9));
    retropp::IndexGrid a2 = room(30, 16, [] {
        std::vector<Vec2i> doors = row(0, 2, 5);
        const std::vector<Vec2i> east = column(29, 3, 6);
        doors.insert(doors.end(), east.begin(), east.end());
        return doors;
    }());
    retropp::IndexGrid a3 = room(12, 10);
};

WorldKit::Layout layoutOver(const Maps& maps) {
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{.tiles = &maps.a0, .art = art(), .collision = &maps.a0,
                           .openings = {{.name = "east", .side = Side::East, .span = {6, 9},
                                         .transition = Transition::Scroll, .leadsTo = {.area = 1, .opening = "west"}},
                                        {.name = "south", .side = Side::South, .span = {8, 11},
                                         .transition = Transition::Scroll, .leadsTo = {.area = 2, .opening = "north"}},
                                        {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade, .leadsTo = {.area = 3, .opening = "stairs"}}}},
            WorldKit::Area{.tiles = &maps.a1, .art = art(), .collision = &maps.a1,
                           .openings = {{.name = "west", .side = Side::West, .span = {6, 9},
                                         .transition = Transition::Scroll, .leadsTo = {.area = 0, .opening = "east"}}}},
            WorldKit::Area{.tiles = &maps.a2, .art = art(), .collision = &maps.a2,
                           .openings = {{.name = "north", .side = Side::North, .span = {2, 5},
                                         .transition = Transition::Scroll, .leadsTo = {.area = 0, .opening = "south"}},
                                        {.name = "east", .side = Side::East, .span = {3, 6}}}},
            WorldKit::Area{.tiles = &maps.a3, .art = art(), .collision = &maps.a3,
                           .openings = {{.name = "stairs", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade, .leadsTo = {.area = 0, .opening = "stairs"}}}},
        },
        .cellSize = kCell,
        .open = Cell::Open,
        .view = kView,
    };
}

// A 10×8 box at (x, y): the usual collision box of a 16×16 walker.
IntRect box(int x, int y) {
    return IntRect{.x = x, .y = y, .width = 10, .height = 8};
}

// Whether validate throws, and what it says.
std::string rejection(const WorldKit::Layout& layout) {
    try {
        WorldKit::validate(layout);
    } catch (const std::invalid_argument& e) {
        return e.what();
    }
    return "";
}

// ── Validation ───────────────────────────────────────────────────────────────────────────────

TEST(Validate, AcceptsTheFourAreas) {
    const Maps maps;
    EXPECT_NO_THROW(WorldKit::validate(layoutOver(maps)));
}

TEST(Validate, RejectsAnAreaWithoutBothMapsOfOneSize) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[1].collision = nullptr;
    EXPECT_NE(rejection(layout).find("area 1 has no tile map or no collision map"), std::string::npos);
    layout.areas[1].collision = &maps.a0;
    EXPECT_NE(rejection(layout).find("area 1's tile map and collision map differ in size"), std::string::npos);
}

TEST(Validate, RejectsAnOpeningWithNoNameOrANameUsedTwice) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[1].name = "";
    EXPECT_NE(rejection(layout).find("area 0 has an opening with no name"), std::string::npos);
    layout.areas[0].openings[1].name = "east";
    EXPECT_NE(rejection(layout).find("opening 'east' is named twice"), std::string::npos);
}

TEST(Validate, RejectsAnOpeningThatIsNeitherInAWallNorAtASpotOrIsBoth) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[0].side = Side::None;
    EXPECT_NE(rejection(layout).find("'east' must be in one wall or at one spot"), std::string::npos);
    layout.areas[0].openings[0].side = Side::East;
    layout.areas[0].openings[0].at = {.x = 1, .y = 1, .width = 1, .height = 1};
    EXPECT_NE(rejection(layout).find("'east' must be in one wall or at one spot"), std::string::npos);
}

TEST(Validate, RejectsASpanOffItsWallAndASpotOffTheMap) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[0].span = {6, 16};  // the east wall has rows 0–15
    EXPECT_NE(rejection(layout).find("'east''s span is off its wall"), std::string::npos);
    layout.areas[0].openings[0].span = {9, 6};
    EXPECT_NE(rejection(layout).find("'east''s span is off its wall"), std::string::npos);
    layout = layoutOver(maps);
    layout.areas[0].openings[2].at = {.x = 19, .y = 4, .width = 2, .height = 1};
    EXPECT_NE(rejection(layout).find("'stairs' is off the map"), std::string::npos);
}

TEST(Validate, RejectsOpeningsThatOverlap) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings.push_back({.name = "east again", .side = Side::East, .span = {9, 12}});
    EXPECT_NE(rejection(layout).find("'east again' overlaps opening 'east'"), std::string::npos);
    layout = layoutOver(maps);
    layout.areas[0].openings.push_back({.name = "hole", .at = {.x = 13, .y = 4, .width = 1, .height = 1}});
    EXPECT_NE(rejection(layout).find("'hole' overlaps opening 'stairs'"), std::string::npos);
    // Two on the same wall that do not overlap are fine.
    layout = layoutOver(maps);
    layout.areas[0].openings.push_back({.name = "east again", .side = Side::East, .span = {12, 13}});
    EXPECT_EQ(rejection(layout), "");
}

TEST(Validate, RejectsADestinationThatDoesNotExist) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[0].leadsTo.area = 7;
    EXPECT_NE(rejection(layout).find("'east' leads to area 7, which does not exist"), std::string::npos);
    layout.areas[0].openings[0].leadsTo.area = 1;
    layout.areas[0].openings[0].leadsTo.opening = "gate";
    EXPECT_NE(rejection(layout).find("'east' leads to 'gate' in area 1, which has no such opening"),
              std::string::npos);
}

TEST(Validate, RejectsAScrollThatIsNotWallToFacingWallThroughSpansOfOneLength) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[2].transition = Transition::Scroll;  // a spot cannot scroll
    EXPECT_NE(rejection(layout).find("'stairs' scrolls, so it and 'stairs' must be in facing walls"),
              std::string::npos);
    layout = layoutOver(maps);
    layout.areas[1].openings[0].side = Side::North;
    layout.areas[1].openings[0].span = {6, 9};
    EXPECT_NE(rejection(layout).find("'east' scrolls, so it and 'west' must be in facing walls"), std::string::npos);
    layout = layoutOver(maps);
    layout.areas[1].openings[0].span = {6, 8};
    EXPECT_NE(rejection(layout).find("'east' scrolls into 'west', whose span is a different length"),
              std::string::npos);
    // A fade between any two openings is fine.
    layout = layoutOver(maps);
    layout.areas[0].openings[0].transition = Transition::Fade;
    layout.areas[0].openings[0].leadsTo = {.area = 3, .opening = "stairs"};
    EXPECT_EQ(rejection(layout), "");
}

// ── Solids ───────────────────────────────────────────────────────────────────────────────────

TEST(Solids, WallCellsAreSolidAndOpenCellsAreNot) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{0}.solids(layout);
    EXPECT_TRUE(solids.solidAt({.x = 0, .y = 0}));
    EXPECT_TRUE(solids.solidAt({.x = 7, .y = 7}));
    EXPECT_FALSE(solids.solidAt({.x = 8, .y = 8}));
    EXPECT_FALSE(solids.solidAt({.x = 19 * kCell, .y = 6 * kCell}));  // the east doorway
    EXPECT_TRUE(solids.solidAt({.x = 19 * kCell, .y = 5 * kCell}));   // the wall above it
    EXPECT_TRUE(solids.solidBox(box(143, 40)));   // reaches column 19 at row 5
    EXPECT_FALSE(solids.solidBox(box(142, 40)));
    EXPECT_FALSE(solids.solidBox({.x = 0, .y = 0, .width = 0, .height = 8}));
}

TEST(Solids, OffTheMapIsSolidExceptStraightOutThroughAnExit) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{0}.solids(layout);
    EXPECT_FALSE(solids.solidAt({.x = 160, .y = 6 * kCell}));   // past the east door
    EXPECT_FALSE(solids.solidAt({.x = 999, .y = 9 * kCell + 7}));
    EXPECT_TRUE(solids.solidAt({.x = 160, .y = 5 * kCell}));    // past the wall beside it
    EXPECT_TRUE(solids.solidAt({.x = 160, .y = 10 * kCell}));
    EXPECT_FALSE(solids.solidAt({.x = 8 * kCell, .y = 128}));   // past the south road
    EXPECT_TRUE(solids.solidAt({.x = 7 * kCell, .y = 128}));
    EXPECT_TRUE(solids.solidAt({.x = -1, .y = 6 * kCell}));     // no opening in the west wall
    EXPECT_TRUE(solids.solidAt({.x = 160, .y = 128}));          // past the corner
    EXPECT_FALSE(solids.solidBox(box(155, 6 * kCell)));         // a box half out through the door
    EXPECT_TRUE(solids.solidBox(box(155, 5 * kCell)));
}

TEST(Solids, AnOpeningThatLeadsNowhereIsNotAWayOut) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{2}.solids(layout);
    EXPECT_FALSE(solids.solidAt({.x = 29 * kCell, .y = 4 * kCell}));  // the doorway cell is open
    EXPECT_TRUE(solids.solidAt({.x = 30 * kCell, .y = 4 * kCell}));   // but nothing lies past it
    EXPECT_FALSE(solids.solidAt({.x = 3 * kCell, .y = -1}));         // the north road does lead out
}

// ── The camera within an area ────────────────────────────────────────────────────────────────

TEST(Camera, HoldsStillInAnAreaTheSizeOfTheView) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    EXPECT_EQ(camera.follow(layout, box(10, 10)).move, (Vec2i{0, 0}));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    (void)camera.follow(layout, box(140, 110));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(camera.scroll(layout), (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_FALSE(camera.transitioning());
}

TEST(Camera, FollowsAcrossAnAreaWiderThanTheViewAndStopsAtItsEdges) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{1};
    (void)camera.follow(layout, box(155, 60));   // center (160, 64)
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));
    (void)camera.follow(layout, box(20, 60));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    (void)camera.follow(layout, box(300, 60));
    EXPECT_EQ(camera.position(), (Vec2i{160, 0}));
}

TEST(Camera, CentersAnAreaSmallerThanTheView) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{3};
    (void)camera.follow(layout, box(10, 10));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    (void)camera.follow(layout, box(80, 60));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    EXPECT_EQ(camera.scroll(layout), (retropp::LayerScroll{.x = -32, .y = -40}));
}

TEST(Camera, TakesTheLayoutsCameraLaw) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.camera = [](IntRect, retropp::PixelSize, Vec2i focus) { return Vec2i{.x = focus.x, .y = focus.y}; };
    WorldKit::Camera camera{1};
    (void)camera.follow(layout, box(100, 50));
    EXPECT_EQ(camera.position(), (Vec2i{105, 54}));
}

TEST(Camera, PlaceLooksAtTheBoxWithNoTransition) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{1};
    camera.place(layout, box(155, 60));
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));
    EXPECT_FALSE(camera.transitioning());
}

// ── A scroll ─────────────────────────────────────────────────────────────────────────────────

// The box's center steps past area 0's east wall through the door: (155, 60) has its center at (160, 64).
TEST(Scroll, StartsWhenTheBoxsCenterGoesOutThroughAnExit) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(154, 60));
    EXPECT_FALSE(camera.transitioning());
    const WorldKit::Step step = camera.follow(layout, box(155, 60));
    EXPECT_TRUE(camera.transitioning());
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(step.move, (Vec2i{0, 0}));
    EXPECT_EQ(step.entered, std::nullopt);
}

TEST(Scroll, DoesNotStartPastAWallBesideTheDoor) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(155, 36));   // center (160, 40): row 5, beside the door
    EXPECT_FALSE(camera.transitioning());
}

TEST(Scroll, PansTheCameraAndCarriesTheBoxThenHandsBothToTheNextAreasPixels) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(155, 60));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));

    Vec2i moved{};
    std::optional<int> entered;
    for (int tick = 1; tick <= 16; ++tick) {
        const WorldKit::Step step = camera.follow(layout, box(0, 0));  // the box is ignored mid-scroll
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
        EXPECT_EQ(step.entered, std::nullopt) << tick;
    }
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));   // halfway from 0 to area 1's left edge
    EXPECT_EQ(moved, (Vec2i{8, 0}));                // half the carry
    EXPECT_EQ(camera.area(), 0);

    for (int tick = 17; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(layout, box(0, 0));
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
        if (step.entered) {
            EXPECT_EQ(tick, 32);
            entered = step.entered;
        }
    }
    EXPECT_EQ(entered, std::optional<int>{1});
    EXPECT_EQ(camera.area(), 1);
    EXPECT_FALSE(camera.transitioning());
    // The whole carry, then into area 1's pixels: area 1's left edge was 160 px right of area 0's.
    EXPECT_EQ(moved, (Vec2i{16 - 160, 0}));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
}

TEST(Scroll, LinesTheNextAreaUpWithTheOpening) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    // Out through the south road, columns 8–11; area 2's north road is columns 2–5, so area 2 sits
    // 6 cells (48 px) to the right and 128 px down.
    (void)camera.follow(layout, box(75, 124));   // center (80, 128)
    ASSERT_TRUE(camera.transitioning());
    const std::vector<WorldKit::Shown> shown = camera.shown(layout);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].area, 0);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_EQ(shown[1].area, 2);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = -48, .y = -144}));

    Vec2i moved{};
    for (int tick = 1; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(layout, box(0, 0));
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
    }
    EXPECT_EQ(camera.area(), 2);
    EXPECT_EQ(moved, (Vec2i{-48, 16 - 128}));
    // The camera lands where the law puts it in area 2 for the box's landing point: x = 80 − 48 = 32,
    // centered → 32 − 80 clamped to 0; y = 0.
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(camera.shown(layout).size(), 1u);
}

TEST(Scroll, CanBeShorterAndCarryFurther) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.scrollTicks = 4;
    layout.carry = 10;
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(155, 60));
    EXPECT_EQ(camera.follow(layout, box(0, 0)).move, (Vec2i{2, 0}));   // 10 × 1/4 = 2.5 → 2
    EXPECT_EQ(camera.follow(layout, box(0, 0)).move, (Vec2i{3, 0}));   // 5 − 2
    EXPECT_EQ(camera.follow(layout, box(0, 0)).move, (Vec2i{2, 0}));   // 7 − 5
    EXPECT_EQ(camera.follow(layout, box(0, 0)).move, (Vec2i{3 - 160, 0}));
    EXPECT_EQ(camera.area(), 1);
}

// ── A fade ───────────────────────────────────────────────────────────────────────────────────

// Area 0's stairs are cells (12, 4) and (13, 4): pixels 96–111 across, 32–39 down.
TEST(Fade, StartsWhenTheBoxsCenterEntersASpotAndSwitchesAreasInTheDark) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(95, 50));   // center (100, 54): below the stairs
    EXPECT_FALSE(camera.transitioning());
    (void)camera.follow(layout, box(95, 32));   // center (100, 36): on them
    ASSERT_TRUE(camera.transitioning());
    EXPECT_FLOAT_EQ(camera.fade(layout), 0.0f);

    for (int tick = 1; tick <= 15; ++tick) {
        const WorldKit::Step step = camera.follow(layout, box(95, 32));
        EXPECT_EQ(step.move, (Vec2i{0, 0})) << tick;
        EXPECT_EQ(step.entered, std::nullopt) << tick;
        EXPECT_EQ(camera.area(), 0) << tick;
    }
    EXPECT_FLOAT_EQ(camera.fade(layout), 15.0f / 16.0f);

    // Tick 16: dark; the box jumps onto area 3's stairs, cells (5, 4)–(6, 4): pixels 40–55 × 32–39.
    const WorldKit::Step arrival = camera.follow(layout, box(95, 32));
    EXPECT_FLOAT_EQ(camera.fade(layout), 1.0f);
    EXPECT_EQ(arrival.entered, std::optional<int>{3});
    EXPECT_EQ(camera.area(), 3);
    EXPECT_EQ(arrival.move, (Vec2i{43 - 95, 32 - 32}));   // the 10×8 box centered on the 16×8 spot
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));      // area 3 is centered in the view
    EXPECT_EQ(camera.shown(layout).size(), 1u);
    EXPECT_EQ(camera.shown(layout)[0].area, 3);

    for (int tick = 17; tick <= 31; ++tick) {
        const WorldKit::Step step = camera.follow(layout, box(43, 32));
        EXPECT_EQ(step.move, (Vec2i{0, 0})) << tick;
        EXPECT_EQ(step.entered, std::nullopt) << tick;
        EXPECT_TRUE(camera.transitioning()) << tick;
    }
    EXPECT_FLOAT_EQ(camera.fade(layout), 1.0f / 16.0f);
    (void)camera.follow(layout, box(43, 32));
    EXPECT_FALSE(camera.transitioning());
    EXPECT_FLOAT_EQ(camera.fade(layout), 0.0f);
}

TEST(Fade, DoesNotStartAgainUntilTheBoxLeavesTheSpotAndComesBack) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(95, 50));
    (void)camera.follow(layout, box(95, 32));
    for (int tick = 1; tick <= 32; ++tick) {
        (void)camera.follow(layout, box(43, 32));
    }
    EXPECT_EQ(camera.area(), 3);
    // Standing on area 3's stairs, where the fade left the box, starts nothing.
    (void)camera.follow(layout, box(43, 32));
    (void)camera.follow(layout, box(44, 32));
    EXPECT_FALSE(camera.transitioning());
    // Stepping off and back on does.
    (void)camera.follow(layout, box(43, 48));
    (void)camera.follow(layout, box(43, 32));
    EXPECT_TRUE(camera.transitioning());
}

TEST(Fade, PlacingOntoASpotStartsNothing) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    camera.place(layout, box(95, 32));
    (void)camera.follow(layout, box(95, 32));
    EXPECT_FALSE(camera.transitioning());
}

TEST(Fade, LandsJustInsideAWallsGapWhenTheEntranceIsInAWall) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.areas[0].openings[2].leadsTo = {.area = 1, .opening = "west"};   // the stairs fade to area 1's door
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(95, 50));
    (void)camera.follow(layout, box(95, 32));
    WorldKit::Step arrival{};
    for (int tick = 1; tick <= 16; ++tick) {
        arrival = camera.follow(layout, box(95, 32));
    }
    EXPECT_EQ(camera.area(), 1);
    // One cell in from the west wall, centered on rows 6–9 (pixels 48–79, center 64).
    EXPECT_EQ(arrival.move, (Vec2i{8 - 95, 60 - 32}));
}

// ── Layers and the fade region ───────────────────────────────────────────────────────────────

TEST(WorldLayers, IsOneTileLayerPerAreaShownPlacedByItsScroll) {
    const Maps maps;
    const WorldKit::Layout layout = layoutOver(maps);
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(72, 56));
    LayerKit::Layers layers = WorldKit::WorldLayers(camera, layout, 5);
    ASSERT_EQ(layers.size(), 1u);
    EXPECT_EQ(std::string_view{layers[0].key}, "area:0");
    EXPECT_EQ(layers[0].z, 5);
    EXPECT_EQ(layers[0].scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
    const auto& tiles = std::get<retropp::TileContent>(layers[0].content);
    EXPECT_EQ(tiles.widthInTiles, 20);
    EXPECT_EQ(tiles.heightInTiles, 16);
    ASSERT_EQ(tiles.cells.size(), 320u);
    EXPECT_EQ(tiles.cells[0].tile, 1);    // a wall cell draws slot 1
    EXPECT_EQ(tiles.cells[21].tile, 0);   // an open cell draws slot 0
    EXPECT_EQ(tiles.cells[0].palette, retropp::PaletteId{4});
    EXPECT_EQ(tiles.wrap, retropp::TileWrap::Blank);

    (void)camera.follow(layout, box(155, 60));
    layers = WorldKit::WorldLayers(camera, layout, 5);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(std::string_view{layers[1].key}, "area:1");
    EXPECT_EQ(layers[1].scroll, (retropp::LayerScroll{.x = -160, .y = -16}));
    EXPECT_EQ(std::get<retropp::TileContent>(layers[1].content).widthInTiles, 40);
}

TEST(FadeRegion, IsNothingUntilAFadeRunsThenOneFillAtTheFadesOpacity) {
    const Maps maps;
    WorldKit::Layout layout = layoutOver(maps);
    layout.fadeColor = retropp::Rgba8{10, 20, 30};
    WorldKit::Camera camera{0};
    (void)camera.follow(layout, box(95, 50));
    EXPECT_TRUE(WorldKit::Fade(camera, layout).empty());
    (void)camera.follow(layout, box(95, 32));
    for (int tick = 1; tick <= 8; ++tick) {
        (void)camera.follow(layout, box(95, 32));
    }
    const LayerKit::Regions regions = WorldKit::Fade(camera, layout);
    ASSERT_EQ(regions.size(), 1u);
    EXPECT_EQ(std::string_view{regions[0].key}, "world:fade");
    EXPECT_FLOAT_EQ(regions[0].alpha, 0.5f);
    ASSERT_EQ(regions[0].effects.size(), 1u);
    EXPECT_EQ(regions[0].effects[0].kind, retropp::ScreenSpaceEffectKind::ColorFill);
    EXPECT_EQ(regions[0].effects[0].fill, (retropp::Rgba8{10, 20, 30}));
    EXPECT_TRUE(regions[0].shape.contains({.x = 0.5f, .y = 0.5f}));
    EXPECT_TRUE(regions[0].shape.contains({.x = 159.5f, .y = 143.5f}));
}

}  // namespace
