// The WorldKit module: what validate rejects, what is solid in an area, the camera within an area, a
// scroll and a fade tick by tick — within a layout and into another — a seamless join, the areas
// shown and their map layers, the world's top depth, the actor slots and the sprite layer drawn at
// one, and the fade region. The world is built in code; no window or GPU is needed.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
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
using WorldKit::Place;
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

// A map of value 0 with value 1 in the given cells: a layer over part of an area.
retropp::IndexGrid patches(int width, int height, const std::vector<Vec2i>& cells) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (const Vec2i cell : cells) {
        map.values[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(cell.x)] = 1;
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

std::vector<Vec2i> both(std::vector<Vec2i> a, const std::vector<Vec2i>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// Every map value drawn as slot 0 of sheet 7 through palette 4; the map doubles as the tile map.
retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Two layouts. Every area has a ground layer at depth 0. Layout 0 is four areas: 0 is one screen with
// an east door to 1, a south road to 2 and a stairs spot fading to 3, with a canopy layer at depth 7
// and an actor slot at depth 5; 1 is two screens wide, with a well that fades into layout 1 and an
// actor slot at depth 3; 2 is one and a half wide, with its north road two cells left of 0's south
// one, a south door scrolling into layout 1 one cell askew, an east door that is only an entrance, and
// a west door joined seamlessly to layout 1's area 1; 3 is smaller than the screen in both axes, with a
// ledge in its north wall that is only an entrance. Layout 1 is two maps joined seamlessly along their
// whole east and west edges, the second two cells lower than the first: 0 is one screen, 1 is wider,
// with the door east to layout 0's area 2.
struct Maps {
    retropp::IndexGrid a0 = room(20, 16, both(column(19, 6, 9), row(15, 8, 11)));
    retropp::IndexGrid a1 = room(40, 16, column(0, 6, 9));
    retropp::IndexGrid a2 = room(30, 16, both(both(both(row(0, 2, 5), column(29, 3, 6)), row(15, 10, 13)), column(0, 3, 6)));
    retropp::IndexGrid a3 = room(12, 10, row(0, 4, 5));
    retropp::IndexGrid a0Canopy = patches(20, 16, both(row(2, 3, 6), row(3, 3, 6)));
    retropp::IndexGrid b0 = room(20, 16, both(row(0, 2, 5), column(19, 0, 15)));
    retropp::IndexGrid b1 = room(24, 16, both(column(0, 0, 15), column(23, 3, 6)));
};

WorldKit::World worldOver(const Maps& maps) {
    return WorldKit::World{.layouts = {
        WorldKit::Layout{
            .areas = {
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.a0, .art = art()},
                                          {.name = "canopy", .tiles = &maps.a0Canopy, .art = art(), .z = 7}},
                               .actorSlots = {{.name = "actors", .z = 5}},
                               .collision = &maps.a0,
                               .openings = {{.name = "east", .side = Side::East, .span = {6, 9},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 1, .opening = "west"}},
                                            {.name = "south", .side = Side::South, .span = {8, 11},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 2, .opening = "north"}},
                                            {.name = "stairs", .at = {.x = 12, .y = 4, .width = 2, .height = 1},
                                             .transition = Transition::Fade, .leadsTo = {.area = 3, .opening = "stairs"}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.a1, .art = art()}},
                               .actorSlots = {{.name = "actors", .z = 3}},
                               .collision = &maps.a1,
                               .openings = {{.name = "west", .side = Side::West, .span = {6, 9},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 0, .opening = "east"}},
                                            {.name = "well", .at = {.x = 30, .y = 2, .width = 2, .height = 1},
                                             .transition = Transition::Fade,
                                             .leadsTo = {.layout = 1, .area = 0, .opening = "well"}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.a2, .art = art()}}, .collision = &maps.a2,
                               .openings = {{.name = "north", .side = Side::North, .span = {2, 5},
                                             .transition = Transition::Scroll, .leadsTo = {.area = 0, .opening = "south"}},
                                            {.name = "east", .side = Side::East, .span = {3, 6}},
                                            {.name = "south", .side = Side::South, .span = {10, 13},
                                             .transition = Transition::Scroll,
                                             .leadsTo = {.layout = 1, .area = 0, .opening = "north", .offset = {.x = -1, .y = 0}}},
                                            {.name = "west", .side = Side::West, .span = {3, 6},
                                             .transition = Transition::Seamless,
                                             .leadsTo = {.layout = 1, .area = 1, .opening = "east"}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.a3, .art = art()}}, .collision = &maps.a3,
                               .openings = {{.name = "stairs", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                             .transition = Transition::Fade, .leadsTo = {.area = 0, .opening = "stairs"}},
                                            {.name = "ledge", .side = Side::North, .span = {4, 5}}}},
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
        WorldKit::Layout{
            .areas = {
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.b0, .art = art()}}, .collision = &maps.b0,
                               .openings = {{.name = "north", .side = Side::North, .span = {2, 5},
                                             .transition = Transition::Scroll,
                                             .leadsTo = {.layout = 0, .area = 2, .opening = "south", .offset = {.x = 1, .y = 0}}},
                                            {.name = "well", .at = {.x = 4, .y = 4, .width = 2, .height = 1},
                                             .transition = Transition::Fade,
                                             .leadsTo = {.layout = 0, .area = 1, .opening = "well"}},
                                            {.name = "east", .side = Side::East, .span = {0, 15},
                                             .transition = Transition::Seamless,
                                             .leadsTo = {.area = 1, .opening = "west", .offset = {.x = 0, .y = 2}}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.b1, .art = art()}}, .collision = &maps.b1,
                               .openings = {{.name = "west", .side = Side::West, .span = {0, 15},
                                             .transition = Transition::Seamless,
                                             .leadsTo = {.area = 0, .opening = "east", .offset = {.x = 0, .y = -2}}},
                                            {.name = "east", .side = Side::East, .span = {3, 6},
                                             .transition = Transition::Seamless,
                                             .leadsTo = {.layout = 0, .area = 2, .opening = "west"}}}},
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
    }};
}

// One layout of three areas joined only by seams, the third two seams from the first: 0 is bigger than
// the view both ways, with a seam in its north wall; 1 is smaller than the view, hung above 0 by its
// south wall, with a seam in its east wall; 2 hangs off 1's east side. In area 0's pixels, area 1's
// top-left is (88, −80) and area 2's is (184, −80); in area 2's pixels, area 1's is (−96, 0) and area
// 0's is (−184, 80).
struct Chain {
    retropp::IndexGrid c0 = room(30, 20, row(0, 12, 21));
    retropp::IndexGrid c1 = room(12, 10, both(row(9, 1, 10), column(11, 1, 8)));
    retropp::IndexGrid c2 = room(16, 10, column(0, 1, 8));
};

WorldKit::World chainOver(const Chain& maps, std::uint32_t seamDepth = 0) {
    return WorldKit::World{.layouts = {
        WorldKit::Layout{
            .areas = {
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.c0, .art = art()}}, .collision = &maps.c0,
                               .openings = {{.name = "north", .side = Side::North, .span = {12, 21},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 1, .opening = "south"}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.c1, .art = art()}}, .collision = &maps.c1,
                               .openings = {{.name = "south", .side = Side::South, .span = {1, 10},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 0, .opening = "north"}},
                                            {.name = "east", .side = Side::East, .span = {1, 8},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 2, .opening = "west"}}}},
                WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.c2, .art = art()}}, .collision = &maps.c2,
                               .openings = {{.name = "west", .side = Side::West, .span = {1, 8},
                                             .transition = Transition::Seamless, .leadsTo = {.area = 1, .opening = "east"}}}},
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
            .seamDepth = seamDepth,
        },
    }};
}

// A 10×8 box at (x, y): the usual collision box of a 16×16 walker.
IntRect box(int x, int y) {
    return IntRect{.x = x, .y = y, .width = 10, .height = 8};
}

// Whether validate throws, and what it says.
std::string rejection(const WorldKit::World& world) {
    try {
        WorldKit::validate(world);
    } catch (const std::invalid_argument& e) {
        return e.what();
    }
    return "";
}

std::optional<Place> at(int layout, int area) {
    return Place{.layout = layout, .area = area};
}

// ── Validation ───────────────────────────────────────────────────────────────────────────────

TEST(Validate, AcceptsTheWorldOfTwoLayouts) {
    const Maps maps;
    EXPECT_NO_THROW(WorldKit::validate(worldOver(maps)));
}

TEST(Validate, RejectsAnAreaWithNoCollisionMapOrNoLayers) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[1].collision = nullptr;
    EXPECT_NE(rejection(world).find("layout 0 area 1 has no collision map"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[1].layers.clear();
    EXPECT_NE(rejection(world).find("layout 0 area 1 has no layers"), std::string::npos);
    world.layouts[0].areas[1].actorSlots.clear();   // an area needs no actor slot
    world.layouts[0].areas[1].layers = {{.name = "ground", .tiles = &maps.a1, .art = art()}};
    EXPECT_NO_THROW(WorldKit::validate(world));
}

TEST(Validate, RejectsALayerOrSlotWithNoNameOrANameUsedTwice) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].layers[1].name = "";
    EXPECT_NE(rejection(world).find("layout 0 area 0 has a layer with no name"), std::string::npos);
    world.layouts[0].areas[0].layers[1].name = "ground";
    EXPECT_NE(rejection(world).find("area 0 layer 'ground' is named twice"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[0].actorSlots[0].name = "";
    EXPECT_NE(rejection(world).find("layout 0 area 0 has an actor slot with no name"), std::string::npos);
    world.layouts[0].areas[0].actorSlots[0].name = "canopy";   // a slot and a map layer share names
    EXPECT_NE(rejection(world).find("area 0 layer 'canopy' is named twice"), std::string::npos);
}

TEST(Validate, RejectsALayerWithNoMapOrAMapOfAnotherSize) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].layers[1].tiles = nullptr;
    EXPECT_NE(rejection(world).find("area 0 layer 'canopy' has no map"), std::string::npos);
    world.layouts[0].areas[0].layers[1].tiles = &maps.a1;
    EXPECT_NE(rejection(world).find("area 0 layer 'canopy''s map differs in size from the collision map"),
              std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[1].collision = &maps.a0;
    EXPECT_NE(rejection(world).find("area 1 layer 'ground''s map differs in size from the collision map"),
              std::string::npos);
}

TEST(Validate, RejectsTwoLayersOrSlotsAtOneDepth) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].layers[1].z = 0;
    EXPECT_NE(rejection(world).find("area 0 layer 'canopy' is at the same depth as layer 'ground'"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[0].actorSlots[0].z = 7;
    EXPECT_NE(rejection(world).find("area 0 layer 'actors' is at the same depth as layer 'canopy'"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[0].actorSlots.push_back({.name = "flyers", .z = 5});
    EXPECT_NE(rejection(world).find("area 0 layer 'flyers' is at the same depth as layer 'actors'"), std::string::npos);
    world.layouts[0].areas[0].actorSlots[1].z = 9;   // a second slot above the canopy
    EXPECT_NO_THROW(WorldKit::validate(world));
    world = worldOver(maps);
    world.layouts[0].areas[1].actorSlots[0].z = 7;   // depths are per area: area 0's canopy is not in area 1
    EXPECT_NO_THROW(WorldKit::validate(world));
}

TEST(Validate, RejectsAnOpeningWithNoNameOrANameUsedTwice) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[1].name = "";
    EXPECT_NE(rejection(world).find("area 0 has an opening with no name"), std::string::npos);
    world.layouts[0].areas[0].openings[1].name = "east";
    EXPECT_NE(rejection(world).find("opening 'east' is named twice"), std::string::npos);
}

TEST(Validate, RejectsAnOpeningThatIsNeitherInAWallNorAtASpotOrIsBoth) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[0].side = Side::None;
    EXPECT_NE(rejection(world).find("'east' must be in one wall or at one spot"), std::string::npos);
    world.layouts[0].areas[0].openings[0].side = Side::East;
    world.layouts[0].areas[0].openings[0].at = {.x = 1, .y = 1, .width = 1, .height = 1};
    EXPECT_NE(rejection(world).find("'east' must be in one wall or at one spot"), std::string::npos);
}

TEST(Validate, RejectsASpanOffItsWallAndASpotOffTheMap) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[0].span = {6, 16};  // the east wall has rows 0–15
    EXPECT_NE(rejection(world).find("'east''s span is off its wall"), std::string::npos);
    world.layouts[0].areas[0].openings[0].span = {9, 6};
    EXPECT_NE(rejection(world).find("'east''s span is off its wall"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[0].openings[2].at = {.x = 19, .y = 4, .width = 2, .height = 1};
    EXPECT_NE(rejection(world).find("'stairs' is off the map"), std::string::npos);
}

TEST(Validate, RejectsOpeningsThatOverlap) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings.push_back({.name = "east again", .side = Side::East, .span = {9, 12}});
    EXPECT_NE(rejection(world).find("'east again' overlaps opening 'east'"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[0].openings.push_back({.name = "hole", .at = {.x = 13, .y = 4, .width = 1, .height = 1}});
    EXPECT_NE(rejection(world).find("'hole' overlaps opening 'stairs'"), std::string::npos);
    // Two on the same wall that do not overlap are fine.
    world = worldOver(maps);
    world.layouts[0].areas[0].openings.push_back({.name = "east again", .side = Side::East, .span = {12, 13}});
    EXPECT_EQ(rejection(world), "");
}

TEST(Validate, RejectsADestinationThatDoesNotExist) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[0].leadsTo.area = 7;
    EXPECT_NE(rejection(world).find("'east' leads to area 7 of layout 0, which does not exist"), std::string::npos);
    world.layouts[0].areas[0].openings[0].leadsTo.area = 1;
    world.layouts[0].areas[0].openings[0].leadsTo.opening = "gate";
    EXPECT_NE(rejection(world).find("'east' leads to 'gate' in area 1 of layout 0, which has no such opening"),
              std::string::npos);
}

TEST(Validate, RejectsADestinationLayoutThatDoesNotExist) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[1].openings[1].leadsTo.layout = 5;
    EXPECT_NE(rejection(world).find("'well' leads to layout 5, which does not exist"), std::string::npos);
}

TEST(Validate, LooksTheAreaAndOpeningUpInTheDestinationLayout) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[1].openings[1].leadsTo.area = 3;   // layout 0 has an area 3; layout 1 does not
    EXPECT_NE(rejection(world).find("'well' leads to area 3 of layout 1, which does not exist"), std::string::npos);
    world.layouts[0].areas[1].openings[1].leadsTo.area = 0;
    world.layouts[0].areas[1].openings[1].leadsTo.opening = "stairs";   // layout 0's area 0 has stairs; layout 1's has none
    EXPECT_NE(rejection(world).find("'well' leads to 'stairs' in area 0 of layout 1, which has no such opening"),
              std::string::npos);
}

TEST(Validate, RejectsAScrollThatIsNotWallToFacingWallThroughSpansOfOneLength) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[2].transition = Transition::Scroll;  // a spot cannot scroll
    EXPECT_NE(rejection(world).find("'stairs' scrolls, so it and 'stairs' must be in facing walls"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[1].openings[0].side = Side::North;
    world.layouts[0].areas[1].openings[0].span = {6, 9};
    EXPECT_NE(rejection(world).find("'east' scrolls, so it and 'west' must be in facing walls"), std::string::npos);
    world = worldOver(maps);
    world.layouts[0].areas[1].openings[0].span = {6, 8};
    EXPECT_NE(rejection(world).find("'east' scrolls into 'west', whose span is a different length"), std::string::npos);
    // A fade between any two openings is fine.
    world = worldOver(maps);
    world.layouts[0].areas[0].openings[0].transition = Transition::Fade;
    world.layouts[0].areas[0].openings[0].leadsTo = {.area = 3, .opening = "stairs"};
    EXPECT_EQ(rejection(world), "");
}

TEST(Validate, RejectsAFadeWithAnOffset) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[2].leadsTo.offset = {.x = 1, .y = 0};
    EXPECT_NE(rejection(world).find("'stairs' fades, so it takes no offset"), std::string::npos);
}

TEST(Validate, RejectsASeamlessJoinFromASpot) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[2].transition = Transition::Seamless;
    EXPECT_NE(rejection(world).find("'stairs' joins seamlessly, so it must be in a wall"), std::string::npos);
}

TEST(Validate, RejectsASeamlessJoinWhoseFarEndDoesNotMatchIt) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    WorldKit::Opening& back = world.layouts[1].areas[1].openings[0];   // layout 1 area 1 'west'
    back.side = Side::North;
    EXPECT_NE(rejection(world).find("layout 1 area 0 opening 'east' joins seamlessly, so it and 'west' must be in facing walls"),
              std::string::npos);
    back.side = Side::West;
    back.span = {0, 14};
    EXPECT_NE(rejection(world).find("'east' joins seamlessly into 'west', whose span is a different length"),
              std::string::npos);
    back.span = {0, 15};
    back.transition = Transition::Scroll;
    EXPECT_NE(rejection(world).find("'east' joins seamlessly into 'west', which is not seamless"), std::string::npos);
    back.transition = Transition::Seamless;
    back.leadsTo.opening = "north";
    EXPECT_NE(rejection(world).find("'east' joins seamlessly into 'west', which does not lead back to it"),
              std::string::npos);
    back.leadsTo.opening = "east";
    back.leadsTo.offset = {.x = 0, .y = 2};
    EXPECT_NE(rejection(world).find("'east' joins seamlessly into 'west', whose offset is not the negation of its own"),
              std::string::npos);
    back.leadsTo.offset = {.x = 0, .y = -2};
    EXPECT_EQ(rejection(world), "");
    // Across layouts, the two must share a cell size.
    world.layouts[1].cellSize = 16;
    EXPECT_NE(rejection(world).find("layout 0 area 2 opening 'west' joins seamlessly into layout 1, whose cell size differs"),
              std::string::npos);
}

// ── Solids ───────────────────────────────────────────────────────────────────────────────────

TEST(Solids, WallCellsAreSolidAndOpenCellsAreNot) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{0, 0}.solids(world);
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
    const WorldKit::World world = worldOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{0, 0}.solids(world);
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
    const WorldKit::World world = worldOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{0, 2}.solids(world);
    EXPECT_FALSE(solids.solidAt({.x = 29 * kCell, .y = 4 * kCell}));  // the doorway cell is open
    EXPECT_TRUE(solids.solidAt({.x = 30 * kCell, .y = 4 * kCell}));   // but nothing lies past it
    EXPECT_FALSE(solids.solidAt({.x = 3 * kCell, .y = -1}));         // the north road does lead out
    const WorldKit::AreaSolids ledge = WorldKit::Camera{0, 3}.solids(world);
    EXPECT_FALSE(ledge.solidAt({.x = 4 * kCell, .y = 0}));
    EXPECT_TRUE(ledge.solidAt({.x = 4 * kCell, .y = -1}));
}

// Layout 1's area 1 sits against area 0's east wall, two cells (16 px) lower, so area 0's row r is
// area 1's row r − 2.
TEST(Solids, PastASeamlessJoinTheNextAreasCellsAnswer) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    const WorldKit::AreaSolids solids = WorldKit::Camera{1, 0}.solids(world);
    EXPECT_FALSE(solids.solidAt({.x = 160, .y = 3 * kCell}));   // area 1's open west column, row 1
    EXPECT_FALSE(solids.solidAt({.x = 168, .y = 3 * kCell}));   // area 1's floor at (1, 1)
    EXPECT_TRUE(solids.solidAt({.x = 168, .y = 2 * kCell}));    // area 1's wall at (1, 0)
    EXPECT_TRUE(solids.solidAt({.x = 160, .y = 0}));            // above area 1's top edge: solid
    EXPECT_FALSE(solids.solidAt({.x = 160, .y = 15 * kCell})); // area 1's west column, row 13
    EXPECT_FALSE(solids.solidBox(box(155, 40)));                // a box across the join, rows 5–5 → 3
    EXPECT_TRUE(solids.solidBox(box(155, 0)));                  // a box across the join above area 1
    // From the other side, area 0's cells answer, two cells up.
    const WorldKit::AreaSolids back = WorldKit::Camera{1, 1}.solids(world);
    EXPECT_FALSE(back.solidAt({.x = -1, .y = 1 * kCell}));      // area 0's open east column, row 3
    EXPECT_FALSE(back.solidAt({.x = -9, .y = 1 * kCell}));      // area 0's floor at (18, 3)
    EXPECT_TRUE(back.solidAt({.x = -9, .y = 13 * kCell}));      // area 0's bottom wall at (18, 15)
    EXPECT_TRUE(back.solidAt({.x = -1, .y = 14 * kCell}));      // below area 0's bottom edge: solid
}

// ── The camera within an area ────────────────────────────────────────────────────────────────

TEST(Camera, HoldsStillInAnAreaTheSizeOfTheView) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    EXPECT_EQ(camera.layout(), 0);
    EXPECT_EQ(camera.follow(world, box(10, 10)).move, (Vec2i{0, 0}));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    (void)camera.follow(world, box(140, 110));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(camera.scroll(world), (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_FALSE(camera.transitioning());
}

TEST(Camera, FollowsAcrossAnAreaWiderThanTheViewAndStopsAtItsEdges) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 1};
    (void)camera.follow(world, box(155, 60));   // center (160, 64)
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));
    (void)camera.follow(world, box(20, 60));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    (void)camera.follow(world, box(300, 60));
    EXPECT_EQ(camera.position(), (Vec2i{160, 0}));
}

TEST(Camera, CentersAnAreaSmallerThanTheView) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 3};
    (void)camera.follow(world, box(10, 10));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    (void)camera.follow(world, box(80, 60));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    EXPECT_EQ(camera.scroll(world), (retropp::LayerScroll{.x = -32, .y = -40}));
}

TEST(Camera, TakesTheLayoutsCameraLaw) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].camera = [](IntRect, retropp::PixelSize, Vec2i focus) { return Vec2i{.x = focus.x, .y = focus.y}; };
    WorldKit::Camera camera{0, 1};
    (void)camera.follow(world, box(100, 50));
    EXPECT_EQ(camera.position(), (Vec2i{105, 54}));
}

TEST(Camera, PlaceLooksAtTheBoxWithNoTransition) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 1};
    camera.place(world, box(155, 60));
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));
    EXPECT_FALSE(camera.transitioning());
}

// ── A scroll ─────────────────────────────────────────────────────────────────────────────────

// The box's center steps past area 0's east wall through the door: (155, 60) has its center at (160, 64).
TEST(Scroll, StartsWhenTheBoxsCenterGoesOutThroughAnExit) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(154, 60));
    EXPECT_FALSE(camera.transitioning());
    const WorldKit::Step step = camera.follow(world, box(155, 60));
    EXPECT_TRUE(camera.transitioning());
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(step.move, (Vec2i{0, 0}));
    EXPECT_EQ(step.entered, std::nullopt);
}

TEST(Scroll, DoesNotStartPastAWallBesideTheDoor) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(155, 36));   // center (160, 40): row 5, beside the door
    EXPECT_FALSE(camera.transitioning());
}

TEST(Scroll, PansTheCameraAndCarriesTheBoxThenHandsBothToTheNextAreasPixels) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(155, 60));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));

    Vec2i moved{};
    std::optional<Place> entered;
    for (int tick = 1; tick <= 16; ++tick) {
        const WorldKit::Step step = camera.follow(world, box(0, 0));  // the box is ignored mid-scroll
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
        EXPECT_EQ(step.entered, std::nullopt) << tick;
    }
    EXPECT_EQ(camera.position(), (Vec2i{80, 0}));   // halfway from 0 to area 1's left edge
    EXPECT_EQ(moved, (Vec2i{8, 0}));                // half the carry
    EXPECT_EQ(camera.area(), 0);

    for (int tick = 17; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(world, box(0, 0));
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
        if (step.entered) {
            EXPECT_EQ(tick, 32);
            entered = step.entered;
        }
    }
    EXPECT_EQ(entered, at(0, 1));
    EXPECT_EQ(camera.area(), 1);
    EXPECT_FALSE(camera.transitioning());
    // The whole carry, then into area 1's pixels: area 1's left edge was 160 px right of area 0's.
    EXPECT_EQ(moved, (Vec2i{16 - 160, 0}));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
}

TEST(Scroll, LinesTheNextAreaUpWithTheOpening) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    // Out through the south road, columns 8–11; area 2's north road is columns 2–5, so area 2 sits
    // 6 cells (48 px) to the right and 128 px down.
    (void)camera.follow(world, box(75, 124));   // center (80, 128)
    ASSERT_TRUE(camera.transitioning());
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].layout, 0);
    EXPECT_EQ(shown[0].area, 0);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_EQ(shown[1].layout, 0);
    EXPECT_EQ(shown[1].area, 2);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = -48, .y = -144}));

    Vec2i moved{};
    for (int tick = 1; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(world, box(0, 0));
        moved = {.x = moved.x + step.move.x, .y = moved.y + step.move.y};
    }
    EXPECT_EQ(camera.area(), 2);
    EXPECT_EQ(moved, (Vec2i{-48, 16 - 128}));
    // The camera slid 128 px down and not at all across: in area 2's pixels it is 48 px left of the
    // area's edge, which sits 48 px in from the left of the screen. Beside it is layout 1's area 1,
    // joined to area 2's west wall, so that is on screen too.
    EXPECT_EQ(camera.position(), (Vec2i{-48, 0}));
    ASSERT_EQ(camera.shown(world).size(), 2u);
    EXPECT_EQ(camera.shown(world)[1].layout, 1);
    EXPECT_EQ(camera.shown(world)[1].area, 1);
    // Following goes on from there. Area 2 reaches west through its seam into layout 1, so the camera
    // is where the law has it for the box, and follows it either way; the rows are one view and hold.
    (void)camera.follow(world, box(27, 4));   // the box where the scroll left it: (75 − 48, 124 + 16 − 128)
    EXPECT_EQ(camera.position(), (Vec2i{-48, 0}));
    (void)camera.follow(world, box(26, 4));
    EXPECT_EQ(camera.position(), (Vec2i{-49, 0}));
    (void)camera.follow(world, box(27, 5));
    EXPECT_EQ(camera.position(), (Vec2i{-48, 0}));
}

// With the layout's scrolls aligning, the south road's scroll also takes the camera the 48 px across
// that put the view within area 2, so area 2 lands with its edge at the edge of the screen.
TEST(Scroll, AlignsAcrossTheAxisWhenTheLayoutSaysSo) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].scrollAligns = true;
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(75, 124));
    IntRect carried = box(75, 124);
    for (int tick = 1; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(world, carried);
        carried.x += step.move.x;
        carried.y += step.move.y;
    }
    EXPECT_EQ(camera.area(), 2);
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(camera.shown(world).size(), 1u);
}

TEST(Scroll, CanBeShorterAndCarryFurther) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].scrollTicks = 4;
    world.layouts[0].carry = 10;
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(155, 60));
    EXPECT_EQ(camera.follow(world, box(0, 0)).move, (Vec2i{2, 0}));   // 10 × 1/4 = 2.5 → 2
    EXPECT_EQ(camera.follow(world, box(0, 0)).move, (Vec2i{3, 0}));   // 5 − 2
    EXPECT_EQ(camera.follow(world, box(0, 0)).move, (Vec2i{2, 0}));   // 7 − 5
    EXPECT_EQ(camera.follow(world, box(0, 0)).move, (Vec2i{3 - 160, 0}));
    EXPECT_EQ(camera.area(), 1);
}

// Area 2's south door is columns 10–13 and layout 1's area 0's north door is columns 2–5, so with the
// spans level area 0 sits 8 cells right and 128 px down; the offset of one cell left makes it 56 px
// right.
TEST(Scroll, IntoAnotherLayoutAttachesTheNextAreaWithTheOffset) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 2};
    (void)camera.follow(world, box(87, 124));   // center (92, 128): column 11
    ASSERT_TRUE(camera.transitioning());
    EXPECT_EQ(camera.position(), (Vec2i{12, 0}));   // following the center across area 2's 240 px
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].layout, 1);
    EXPECT_EQ(shown[1].area, 0);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 12 - 56, .y = -16 - 128}));

    // The box is carried as the camera says, as a game carries it.
    IntRect carried = box(87, 124);
    std::optional<Place> entered;
    for (int tick = 1; tick <= 32; ++tick) {
        const WorldKit::Step step = camera.follow(world, carried);
        carried.x += step.move.x;
        carried.y += step.move.y;
        if (step.entered) {
            entered = step.entered;
        }
    }
    EXPECT_EQ(entered, at(1, 0));
    EXPECT_EQ(camera.layout(), 1);
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(carried, box(87 - 56, 124 + 16 - 128));
    EXPECT_EQ(camera.position(), (Vec2i{12 - 56, 0}));   // slid down only: still 12 px along, 56 px left of area 0
    EXPECT_FALSE(camera.transitioning());

    // Area 0's columns reach east through its seam, so the camera follows the box along them: it
    // holds while the box stands or walks left, and comes right with the box, a step a tick, as far
    // as the law's place — which is 0 until the box's center passes x = 80, and the box after that.
    (void)camera.follow(world, carried);
    EXPECT_EQ(camera.position(), (Vec2i{-44, 0}));
    (void)camera.follow(world, box(30, 12));
    EXPECT_EQ(camera.position(), (Vec2i{-44, 0}));
    for (int k = 1; k <= 54; ++k) {
        (void)camera.follow(world, box(30 + k, 12));
    }
    EXPECT_EQ(camera.position(), (Vec2i{9, 0}));
}

// ── A fade ───────────────────────────────────────────────────────────────────────────────────

// Area 0's stairs are cells (12, 4) and (13, 4): pixels 96–111 across, 32–39 down.
TEST(Fade, StartsWhenTheBoxsCenterEntersASpotAndSwitchesAreasInTheDark) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(95, 50));   // center (100, 54): below the stairs
    EXPECT_FALSE(camera.transitioning());
    (void)camera.follow(world, box(95, 32));   // center (100, 36): on them
    ASSERT_TRUE(camera.transitioning());
    EXPECT_FLOAT_EQ(camera.fade(world), 0.0f);

    for (int tick = 1; tick <= 15; ++tick) {
        const WorldKit::Step step = camera.follow(world, box(95, 32));
        EXPECT_EQ(step.move, (Vec2i{0, 0})) << tick;
        EXPECT_EQ(step.entered, std::nullopt) << tick;
        EXPECT_EQ(camera.area(), 0) << tick;
    }
    EXPECT_FLOAT_EQ(camera.fade(world), 15.0f / 16.0f);

    // Tick 16: dark; the box jumps onto area 3's stairs, cells (5, 4)–(6, 4): pixels 40–55 × 32–39.
    const WorldKit::Step arrival = camera.follow(world, box(95, 32));
    EXPECT_FLOAT_EQ(camera.fade(world), 1.0f);
    EXPECT_EQ(arrival.entered, at(0, 3));
    EXPECT_EQ(camera.area(), 3);
    EXPECT_EQ(arrival.move, (Vec2i{43 - 95, 32 - 32}));   // the 10×8 box centered on the 16×8 spot
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));      // area 3 is centered in the view
    EXPECT_EQ(camera.shown(world).size(), 1u);
    EXPECT_EQ(camera.shown(world)[0].area, 3);

    for (int tick = 17; tick <= 31; ++tick) {
        const WorldKit::Step step = camera.follow(world, box(43, 32));
        EXPECT_EQ(step.move, (Vec2i{0, 0})) << tick;
        EXPECT_EQ(step.entered, std::nullopt) << tick;
        EXPECT_TRUE(camera.transitioning()) << tick;
    }
    EXPECT_FLOAT_EQ(camera.fade(world), 1.0f / 16.0f);
    (void)camera.follow(world, box(43, 32));
    EXPECT_FALSE(camera.transitioning());
    EXPECT_FLOAT_EQ(camera.fade(world), 0.0f);
}

TEST(Fade, DoesNotStartAgainUntilTheBoxLeavesTheSpotAndComesBack) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(95, 50));
    (void)camera.follow(world, box(95, 32));
    for (int tick = 1; tick <= 32; ++tick) {
        (void)camera.follow(world, box(43, 32));
    }
    EXPECT_EQ(camera.area(), 3);
    // Standing on area 3's stairs, where the fade left the box, starts nothing.
    (void)camera.follow(world, box(43, 32));
    (void)camera.follow(world, box(44, 32));
    EXPECT_FALSE(camera.transitioning());
    // Stepping off and back on does.
    (void)camera.follow(world, box(43, 48));
    (void)camera.follow(world, box(43, 32));
    EXPECT_TRUE(camera.transitioning());
}

TEST(Fade, PlacingOntoASpotStartsNothing) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    camera.place(world, box(95, 32));
    (void)camera.follow(world, box(95, 32));
    EXPECT_FALSE(camera.transitioning());
}

TEST(Fade, LandsJustInsideAWallsGapWhenTheEntranceIsInAWall) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].areas[0].openings[2].leadsTo = {.area = 1, .opening = "west"};   // the stairs fade to area 1's door
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(95, 50));
    (void)camera.follow(world, box(95, 32));
    WorldKit::Step arrival{};
    for (int tick = 1; tick <= 16; ++tick) {
        arrival = camera.follow(world, box(95, 32));
    }
    EXPECT_EQ(camera.area(), 1);
    // One cell in from the west wall, centered on rows 6–9 (pixels 48–79, center 64).
    EXPECT_EQ(arrival.move, (Vec2i{8 - 95, 60 - 32}));
}

// Area 1's well is cells (30, 2)–(31, 2): pixels 240–255 × 16–23. It fades to layout 1's area 0, whose
// well is cells (4, 4)–(5, 4): pixels 32–47 × 32–39.
TEST(Fade, IntoAnotherLayoutLandsThereAndTheLayoutChangesInTheDark) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[1].fadeTicks = 8;   // the way back up is in the other layout's time
    WorldKit::Camera camera{0, 1};
    (void)camera.follow(world, box(245, 30));   // center (250, 34): below the well
    (void)camera.follow(world, box(245, 12));   // center (250, 16): in it
    ASSERT_TRUE(camera.transitioning());
    WorldKit::Step arrival{};
    for (int tick = 1; tick <= 16; ++tick) {
        EXPECT_EQ(camera.layout(), 0) << tick;
        arrival = camera.follow(world, box(245, 12));
    }
    EXPECT_EQ(arrival.entered, at(1, 0));
    EXPECT_EQ(camera.layout(), 1);
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(arrival.move, (Vec2i{35 - 245, 32 - 12}));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_FLOAT_EQ(camera.fade(world), 1.0f);
    // Lightening takes layout 1's eight ticks.
    for (int tick = 1; tick <= 7; ++tick) {
        (void)camera.follow(world, box(35, 32));
        EXPECT_TRUE(camera.transitioning()) << tick;
    }
    EXPECT_FLOAT_EQ(camera.fade(world), 1.0f / 8.0f);
    (void)camera.follow(world, box(35, 32));
    EXPECT_FALSE(camera.transitioning());
}

// ── A seamless join ──────────────────────────────────────────────────────────────────────────

// Layout 1: area 0 is 160 px wide, and area 1 (192 px wide) is attached at its east wall 16 px lower,
// so area 1's top-left is (160, 16) in area 0's pixels.
TEST(Seamless, TheNextAreaIsShownWhenTheViewReachesItAndNotBefore) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{1, 0};
    (void)camera.follow(world, box(10, 60));
    EXPECT_EQ(camera.position(), (Vec2i{0, 0}));
    EXPECT_EQ(camera.shown(world).size(), 1u);   // the view ends exactly where area 1 begins

    (void)camera.follow(world, box(150, 60));    // center (155, 64): the camera follows to x = 75
    EXPECT_EQ(camera.position(), (Vec2i{75, 0}));
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].area, 0);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 75, .y = -16}));
    EXPECT_EQ(shown[1].layout, 1);
    EXPECT_EQ(shown[1].area, 1);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 75 - 160, .y = -16 - 16}));
    EXPECT_FALSE(camera.transitioning());
}

// Area 0 is one view wide, so on its own the camera would hold x at 0; joined to area 1 along its east
// wall, the law's rectangle reaches across the seam and the camera follows the box toward it. Area 1
// reaches 16 px lower, but the seam runs down, not across: area 0's rows are the law's rows.
TEST(Seamless, TheLawsBoundsReachStraightAcrossIntoTheNextAreaAndNoFurther) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{1, 0};
    (void)camera.follow(world, box(154, 116));   // center (159, 120): at the wall, low in the area
    EXPECT_EQ(camera.position(), (Vec2i{79, 0}));
    EXPECT_FALSE(camera.transitioning());
    EXPECT_EQ(camera.area(), 0);
}

TEST(Seamless, CrossingTheWallSwitchesAreasAtOnceWithNothingMovingOnScreen) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{1, 0};
    (void)camera.follow(world, box(154, 60));   // center (159, 64): still in area 0
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(camera.position(), (Vec2i{79, 0}));

    // Center (160, 64): past the wall at row 8. Within area 0 the camera goes to (80, 0) for this box,
    // and is then in area 1's pixels, 160 px right and 16 px down. Area 1 would have it 16 px lower,
    // on its own rows; that is left to settle.
    const WorldKit::Step step = camera.follow(world, box(155, 60));
    EXPECT_EQ(step.entered, at(1, 1));
    EXPECT_EQ(step.move, (Vec2i{-160, -16}));
    EXPECT_FALSE(camera.transitioning());
    EXPECT_EQ(camera.layout(), 1);
    EXPECT_EQ(camera.area(), 1);
    EXPECT_EQ(camera.position(), (Vec2i{80 - 160, 0 - 16}));
    // Each area's layer has the scroll the camera at (80, 0) gives it: nothing jumps on screen.
    std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].area, 1);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 80 - 160, .y = 0 - 16 - 16}));
    EXPECT_EQ(shown[1].area, 0);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 80, .y = 0 - 16}));

    // Walking straight back: the box in area 1's pixels is (−5, 44); one pixel further west its center
    // is at x = −1, past area 1's west wall at row 6, and area 0 is current again, the camera where
    // area 0 had it for that box.
    const WorldKit::Step back = camera.follow(world, box(-6, 44));
    EXPECT_EQ(back.entered, at(1, 0));
    EXPECT_EQ(back.move, (Vec2i{160, 16}));
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(camera.position(), (Vec2i{79, 0}));
    EXPECT_FALSE(camera.transitioning());
    (void)camera.follow(world, box(-7 + 160, 44 + 16));
    EXPECT_EQ(camera.position(), (Vec2i{78, 0}));
}

// After the switch the camera is 16 px above area 1's own rows, where area 0 had it. Area 1 is one
// view tall, so there is one place for its rows; the camera comes to it only as the box walks down,
// with the box's own steps, and not at all while the box stands or walks up.
TEST(Seamless, TheCameraMovesOnlyWithTheBoxAfterTheSwitch) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{1, 0};
    (void)camera.follow(world, box(154, 60));
    (void)camera.follow(world, box(155, 60));   // the switch
    EXPECT_EQ(camera.position(), (Vec2i{-80, -16}));
    for (int tick = 1; tick <= 8; ++tick) {
        (void)camera.follow(world, box(-5, 44));   // standing still
        EXPECT_EQ(camera.position(), (Vec2i{-80, -16})) << tick;
    }
    (void)camera.follow(world, box(-5, 43));       // up: not toward the law's place
    EXPECT_EQ(camera.position(), (Vec2i{-80, -16}));
    (void)camera.follow(world, box(-4, 43));       // right: the camera follows, centered, as ever
    EXPECT_EQ(camera.position(), (Vec2i{-79, -16}));
    for (int tick = 1; tick <= 16; ++tick) {       // down: each step brings the camera a step closer
        (void)camera.follow(world, box(-4, 43 + tick));
        EXPECT_EQ(camera.position(), (Vec2i{-79, -16 + tick})) << tick;
    }
    (void)camera.follow(world, box(-4, 60));       // in place: area 1's rows hold still
    EXPECT_EQ(camera.position(), (Vec2i{-79, 0}));
    // `place` goes straight to the law's place.
    camera = WorldKit::Camera{1, 0};
    (void)camera.follow(world, box(155, 60));
    camera.place(world, box(-5, 44));
    EXPECT_EQ(camera.position(), (Vec2i{-80, 0}));
}

// Coming back into the narrower area off its center: the camera stays where it was, with the void
// beside the area in view, and the area's fitted view returns as the box walks toward it.
TEST(Seamless, TheFittedViewReturnsAsTheBoxWalksTowardIt) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{1, 1};
    camera.place(world, box(100, 100));            // in area 1, low: the camera centered at (25, 0)
    EXPECT_EQ(camera.position(), (Vec2i{25, 0}));
    for (int x = 99; x >= -6; --x) {               // west along row 12, across the seam at x = −1
        (void)camera.follow(world, box(x, 100));
    }
    EXPECT_EQ(camera.area(), 0);
    EXPECT_EQ(camera.position(), (Vec2i{79, 16}));   // area 0's pixels: 16 px below its own rows
    (void)camera.follow(world, box(153, 116));       // on west: the camera follows, centered, and never comes up on its own
    EXPECT_EQ(camera.position(), (Vec2i{78, 16}));
    for (int tick = 1; tick <= 20; ++tick) {         // up: a step a tick until it is on area 0's rows, and no further
        (void)camera.follow(world, box(153, 116 - tick));
        EXPECT_EQ(camera.position(), (Vec2i{78, std::max(0, 16 - tick)})) << tick;
    }
}

// An area smaller than the view holds still where the transition left it, whatever the box does.
TEST(Seamless, AnAreaSmallerThanTheViewHoldsStill) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 3};                  // 12×10 cells: 96×80 px
    (void)camera.follow(world, box(10, 10));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    (void)camera.follow(world, box(80, 60));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    (void)camera.follow(world, box(12, 12));
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
}

// ── Seams deep ───────────────────────────────────────────────────────────────────────────────

TEST(SeamDepth, AnAreaTwoSeamsAwayIsShownWhenItIsInView) {
    const Chain maps;
    const WorldKit::World world = chainOver(maps);
    EXPECT_NO_THROW(WorldKit::validate(world));
    WorldKit::Camera camera{0, 0};
    camera.place(world, box(175, 36));   // center (180, 40): the camera at (80, −24), up past area 1's east wall
    EXPECT_EQ(camera.position(), (Vec2i{80, -24}));
    std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[0].area, 0);
    EXPECT_EQ(shown[0].scroll, (retropp::LayerScroll{.x = 80, .y = -40}));
    EXPECT_EQ(shown[1].area, 1);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = 80 - 88, .y = -40 + 80}));
    EXPECT_EQ(shown[2].layout, 0);
    EXPECT_EQ(shown[2].area, 2);
    EXPECT_EQ(shown[2].scroll, (retropp::LayerScroll{.x = 80 - 184, .y = -40 + 80}));

    camera.place(world, box(95, 36));    // center (100, 40): the view ends at x = 180, short of area 2
    EXPECT_EQ(camera.position(), (Vec2i{20, -24}));
    shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, 1);

    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 2u);
    camera.place(world, box(175, 36));
    const LayerKit::Layers deeper = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(deeper.size(), 3u);
    EXPECT_EQ(std::string_view{deeper[2].key}, "area:0:2:ground");
    EXPECT_EQ(deeper[2].scroll, (retropp::LayerScroll{.x = -104, .y = 40}));
}

// From the far end the same chain is walked the other way: area 0 is two seams from area 2, below
// area 1 and in view.
TEST(SeamDepth, IsCountedFromTheCurrentArea) {
    const Chain maps;
    const WorldKit::World world = chainOver(maps);
    WorldKit::Camera camera{0, 2};
    camera.place(world, box(100, 30));   // area 2 is shorter than the view: centered up and down at −24
    EXPECT_EQ(camera.position(), (Vec2i{-32, -24}));
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[0].area, 2);
    EXPECT_EQ(shown[1].area, 1);
    EXPECT_EQ(shown[1].scroll, (retropp::LayerScroll{.x = -32 + 96, .y = -40}));
    EXPECT_EQ(shown[2].area, 0);
    EXPECT_EQ(shown[2].scroll, (retropp::LayerScroll{.x = -32 + 184, .y = -40 - 80}));
}

TEST(SeamDepth, LimitsHowManySeamsAwayAnAreaIsShown) {
    const Chain maps;
    for (const std::uint32_t depth : {0u, 1u, 2u, 3u}) {
        const WorldKit::World world = chainOver(maps, depth);
        WorldKit::Camera camera{0, 0};
        camera.place(world, box(175, 36));   // areas 1 and 2 both in view
        const std::vector<WorldKit::Shown> shown = camera.shown(world);
        ASSERT_EQ(shown.size(), depth == 1 ? 2u : 3u) << depth;
        EXPECT_EQ(shown[1].area, 1) << depth;
    }
}

// The law's rectangle reaches one seam whatever the depth: from area 2 it takes in area 1 alongside, and
// not area 0 below area 1, so area 2's rows stay shorter than the view and the camera holds still up and
// down.
TEST(SeamDepth, TheLawsBoundsStayOneSeamDeep) {
    const Chain maps;
    const WorldKit::World world = chainOver(maps);
    WorldKit::Camera camera{0, 2};
    camera.place(world, box(100, 30));
    for (int y = 31; y <= 60; ++y) {
        (void)camera.follow(world, box(100, y));
        EXPECT_EQ(camera.position().y, -24) << y;
    }
    for (int x = 99; x >= 20; --x) {     // west: the camera follows into area 1's columns
        (void)camera.follow(world, box(x, 60));
    }
    EXPECT_EQ(camera.position(), (Vec2i{-55, -24}));
    EXPECT_EQ(camera.area(), 2);
}

// ── Layers and the fade region ───────────────────────────────────────────────────────────────

TEST(WorldLayers, IsOneTileLayerPerMapLayerOfEachAreaShownAtItsDepth) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(72, 56));
    LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 2u);
    EXPECT_EQ(std::string_view{layers[0].key}, "area:0:0:ground");
    EXPECT_EQ(layers[0].z, 0);
    EXPECT_EQ(std::string_view{layers[1].key}, "area:0:0:canopy");
    EXPECT_EQ(layers[1].z, 7);
    for (const retropp::DrawLayer& layer : layers) {
        EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
        // Confined to the view: the 16 rows above it go see-through.
        ASSERT_EQ(layer.regions.size(), 1u);
        ASSERT_EQ(layer.regions[0].effects.size(), 1u);
        EXPECT_EQ(layer.regions[0].effects[0].kind, retropp::ScreenSpaceEffectKind::Transparency);
        EXPECT_EQ(layer.regions[0].effects[0].stencil, retropp::StencilMode::TransparentOutside);
        EXPECT_TRUE(layer.regions[0].shape.contains({.x = 0.5f, .y = 16.5f}));
        EXPECT_TRUE(layer.regions[0].shape.contains({.x = 159.5f, .y = 143.5f}));
        EXPECT_FALSE(layer.regions[0].shape.contains({.x = 0.5f, .y = 15.5f}));
    }
    const auto& ground = std::get<retropp::TileContent>(layers[0].content);
    EXPECT_EQ(ground.widthInTiles, 20);
    EXPECT_EQ(ground.heightInTiles, 16);
    ASSERT_EQ(ground.cells.size(), 320u);
    EXPECT_EQ(ground.cells[0].tile, 1);    // a wall cell draws slot 1
    EXPECT_EQ(ground.cells[21].tile, 0);   // an open cell draws slot 0
    EXPECT_EQ(ground.cells[0].palette, retropp::PaletteId{4});
    EXPECT_EQ(ground.wrap, retropp::TileWrap::Blank);
    // The canopy's own map: value 1 at (3, 2), value 0 at (0, 0).
    const auto& canopy = std::get<retropp::TileContent>(layers[1].content);
    ASSERT_EQ(canopy.cells.size(), 320u);
    EXPECT_EQ(canopy.cells[2 * 20 + 3].tile, 0);
    EXPECT_EQ(canopy.cells[0].tile, 1);

    (void)camera.follow(world, box(155, 60));
    layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 3u);   // area 0's two, then area 1's ground
    EXPECT_EQ(std::string_view{layers[2].key}, "area:0:1:ground");
    EXPECT_EQ(layers[2].scroll, (retropp::LayerScroll{.x = -160, .y = -16}));
    EXPECT_EQ(std::get<retropp::TileContent>(layers[2].content).widthInTiles, 40);
}

TEST(WorldLayers, KeysEachLayerByLayoutAreaAndNameAndListsASeamlessNeighborsLayersInView) {
    EXPECT_EQ(WorldKit::AreaLayerKey(1, 2, "canopy"), "area:1:2:canopy");
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[1].areas[1].layers.push_back({.name = "canopy", .tiles = &maps.b1, .art = art(), .z = 3});
    WorldKit::Camera camera{1, 0};
    (void)camera.follow(world, box(150, 60));
    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 3u);
    EXPECT_EQ(std::string_view{layers[0].key}, "area:1:0:ground");
    EXPECT_EQ(std::string_view{layers[1].key}, "area:1:1:ground");
    EXPECT_EQ(std::string_view{layers[2].key}, "area:1:1:canopy");
    EXPECT_EQ(layers[2].z, 3);
    for (const std::size_t i : {1u, 2u}) {
        EXPECT_EQ(layers[i].scroll, (retropp::LayerScroll{.x = -85, .y = -32})) << i;
        EXPECT_EQ(std::get<retropp::TileContent>(layers[i].content).widthInTiles, 24) << i;
    }
}

TEST(TopZ, IsTheHighestDepthOfAnyLayerOrSlotInAnyLayout) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    EXPECT_EQ(WorldKit::TopZ(world), 7);   // area 0's canopy
    world.layouts[1].areas[1].layers[0].z = 12;
    EXPECT_EQ(WorldKit::TopZ(world), 12);
    world.layouts[0].areas[3].actorSlots.push_back({.name = "flyers", .z = 20});
    EXPECT_EQ(WorldKit::TopZ(world), 20);
}

// ── Actor slots ──────────────────────────────────────────────────────────────────────────────

TEST(ActorZ, IsTheDepthOfTheNamedSlotInThatArea) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    EXPECT_EQ(WorldKit::ActorZ(world, {.layout = 0, .area = 0}, "actors"), 5);
    EXPECT_EQ(WorldKit::ActorZ(world, {.layout = 0, .area = 1}, "actors"), 3);
    try {
        (void)WorldKit::ActorZ(world, {.layout = 0, .area = 2}, "actors");
        ADD_FAILURE() << "an area with no such slot throws";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string{e.what()}.find("layout 0 area 2 has no actor slot 'actors'"), std::string::npos);
    }
    EXPECT_THROW((void)WorldKit::ActorZ(world, {.layout = 0, .area = 0}, "flyers"), std::out_of_range);
}

retropp::Sprite marker(int x, int y) {
    return retropp::Sprite{.key = "marker", .x = x, .y = y, .size = {.width = 8, .height = 8}};
}

TEST(Actors, IsASpriteLayerAtTheCurrentAreasSlotScrolledByTheCameraAndConfinedToTheView) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(72, 56));
    const retropp::DrawLayer layer = WorldKit::Actors(camera, world, "actors", {{marker(72, 56)}, {marker(80, 60)}});
    EXPECT_EQ(std::string_view{layer.key}, "actors:actors");
    EXPECT_EQ(layer.z, 5);
    EXPECT_EQ(layer.scroll, camera.scroll(world));
    EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
    const auto& sprites = std::get<retropp::SpriteContent>(layer.content).sprites;
    ASSERT_EQ(sprites.size(), 2u);
    EXPECT_EQ(sprites[1].x, 80);
    ASSERT_EQ(layer.regions.size(), 1u);
    EXPECT_EQ(layer.regions[0].effects[0].stencil, retropp::StencilMode::TransparentOutside);
    EXPECT_TRUE(layer.regions[0].shape.contains({.x = 0.5f, .y = 16.5f}));
    EXPECT_FALSE(layer.regions[0].shape.contains({.x = 0.5f, .y = 15.5f}));
}

TEST(Actors, KeepsItsKeyAcrossAreasAndTakesEachAreasDepth) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    IntRect walker = box(140, 60);
    camera.place(world, walker);
    const retropp::DrawLayer before = WorldKit::Actors(camera, world, "actors", {{marker(140, 60)}});
    for (int tick = 0; tick < 200 && camera.area() == 0; ++tick) {   // east, out through the door into area 1
        walker.x += camera.transitioning() ? 0 : 1;
        const WorldKit::Step step = camera.follow(world, walker);
        walker.x += step.move.x;
        walker.y += step.move.y;
    }
    ASSERT_EQ(camera.area(), 1);
    const retropp::DrawLayer after = WorldKit::Actors(camera, world, "actors", {{marker(8, 60)}});
    EXPECT_EQ(std::string_view{before.key}, std::string_view{after.key});
    EXPECT_EQ(before.z, 5);
    EXPECT_EQ(after.z, 3);
}

TEST(Actors, KeepsTheCallersOptionsButTheScrollAndThrowsForASlotTheAreaLacks) {
    const Maps maps;
    const WorldKit::World world = worldOver(maps);
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(72, 56));
    const LayerKit::Regions clip = WorldKit::ViewStencil({.size = {.width = 8, .height = 8}});
    const retropp::DrawLayer layer = WorldKit::Actors(camera, world, "actors", {{marker(72, 56)}},
                                                      {.scroll = {.x = 99, .y = 99}, .alpha = 0.5f, .regions = clip});
    EXPECT_EQ(layer.scroll, (retropp::LayerScroll{.x = 0, .y = -16}));
    EXPECT_FLOAT_EQ(layer.alpha, 0.5f);
    ASSERT_EQ(layer.regions.size(), 2u);   // the caller's region, then the view's stencil
    EXPECT_FALSE(layer.regions[0].shape.contains({.x = 8.5f, .y = 0.5f}));
    EXPECT_TRUE(layer.regions[1].shape.contains({.x = 8.5f, .y = 16.5f}));
    WorldKit::Camera elsewhere{0, 2};
    elsewhere.place(world, box(100, 60));
    EXPECT_THROW((void)WorldKit::Actors(elsewhere, world, "actors", {{marker(100, 60)}}), std::out_of_range);
}

TEST(FadeRegion, IsNothingUntilAFadeRunsThenOneFillAtTheFadesOpacity) {
    const Maps maps;
    WorldKit::World world = worldOver(maps);
    world.layouts[0].fadeColor = retropp::Rgba8{10, 20, 30};
    WorldKit::Camera camera{0, 0};
    (void)camera.follow(world, box(95, 50));
    EXPECT_TRUE(WorldKit::Fade(camera, world).empty());
    (void)camera.follow(world, box(95, 32));
    for (int tick = 1; tick <= 8; ++tick) {
        (void)camera.follow(world, box(95, 32));
    }
    const LayerKit::Regions regions = WorldKit::Fade(camera, world);
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
