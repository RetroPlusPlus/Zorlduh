// WorldKit's depths: an area alone on screen drawn at the depths the world declares for it, and the
// areas on screen spread so that every lower declared depth is drawn under every higher one and each
// layer has a depth of its own — across a seam, during a scroll and through a chain of seams; the declared and
// the drawn depth of an actor slot and an extra layer; the world's top depth; the check on depths that
// leave the engine's range once spread. The world is built in code, and the tests run headless.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/world_layers.h"

namespace {

using retropp::IntRect;
using retropp::Vec2i;
using WorldKit::Side;
using WorldKit::Transition;

enum class Cell : std::uint16_t { Wall = 0, Open = 1 };

constexpr int kCell = 8;
constexpr CameraKit::View kView{.size = {.width = 160, .height = 128}, .origin = {.x = 0, .y = 16}};

// The depths every area of layout 0 declares: the ground, the actors over it, rain over the actors and
// a canopy over the rain. Layout 1 declares its own, all below zero.
constexpr std::int32_t kGroundZ = 0;
constexpr std::int32_t kActorsZ = 10;
constexpr std::int32_t kRainZ = 15;
constexpr std::int32_t kCanopyZ = 20;

constexpr std::int32_t kPitGroundZ = -6;
constexpr std::int32_t kPitMistZ = -4;
constexpr std::int32_t kPitActorsZ = -2;

// The number of areas in the world below.
constexpr std::int64_t kAreas = 6;

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

std::vector<Vec2i> both(std::vector<Vec2i> a, const std::vector<Vec2i>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

retropp::TileCatalog art() {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = retropp::AtlasId{7}, .slot = 1, .palette = retropp::PaletteId{4}},
        {.id = 1, .sheet = retropp::AtlasId{7}, .slot = 0, .palette = retropp::PaletteId{4}},
    }};
}

// Layout 0, four areas. 0, the yard, is one screen, joined seamlessly along its whole east wall to 1 and
// scrolling through a south door into 2. 1, the lane, is 12 cells wide, narrower than the screen, joined
// along its whole west wall to 0 and its whole east wall to 3, so the three are a chain of seams. 2, the
// cellar, is one screen with the door back up. 3, the field, is one screen; the other three reserve rain. Layout 1, two
// areas, the pit and the ledge, one screen each, joined along their whole east and west walls, at
// depths below zero.
struct Maps {
    retropp::IndexGrid yard = room(20, 16, both(column(19, 0, 15), row(15, 8, 11)));
    retropp::IndexGrid lane = room(12, 16, both(column(0, 0, 15), column(11, 0, 15)));
    retropp::IndexGrid cellar = room(20, 16, row(0, 8, 11));
    retropp::IndexGrid field = room(20, 16, column(0, 0, 15));
    retropp::IndexGrid pit = room(20, 16, column(19, 0, 15));
    retropp::IndexGrid ledge = room(20, 16, column(0, 0, 15));
};

enum Area : std::uint8_t { Yard, Lane, Cellar, Field };
enum PitArea : std::uint8_t { Pit, Ledge };

// An area of layout 0: its ground and canopy, the depth it reserves for rain unless `rain` is false,
// and its actor slot.
WorldKit::Area layoutZeroArea(std::string name, const retropp::IndexGrid& map,
                              std::vector<WorldKit::Opening> openings, bool rain = true) {
    WorldKit::Area area{.name = std::move(name),
                        .layers = {{.name = "ground", .tiles = &map, .art = art(), .z = kGroundZ},
                                   {.name = "canopy", .tiles = &map, .art = art(), .z = kCanopyZ}},
                        .actorSlots = {{.name = "actors", .z = kActorsZ}},
                        .collision = &map,
                        .openings = std::move(openings)};
    if (rain) {
        area.layers.push_back(WorldKit::ReservedLayer{.name = "rain", .z = kRainZ});
    }
    return area;
}

// An area of layout 1: its ground, the depth it reserves for mist over it, and its actor slot over the
// mist, all below zero.
WorldKit::Area pitArea(std::string name, const retropp::IndexGrid& map, std::vector<WorldKit::Opening> openings) {
    return WorldKit::Area{.name = std::move(name),
                          .layers = {{.name = "ground", .tiles = &map, .art = art(), .z = kPitGroundZ},
                                     WorldKit::ReservedLayer{.name = "mist", .z = kPitMistZ}},
                          .actorSlots = {{.name = "actors", .z = kPitActorsZ}},
                          .collision = &map,
                          .openings = std::move(openings)};
}

WorldKit::Layout pitLayout(const Maps& maps) {
    return WorldKit::Layout{
        .name = "pits",
        .areas = {
            pitArea("pit", maps.pit, {{.name = "east", .side = Side::East, .span = {0, 15}, .transition = Transition::Seamless,
                                .leadsTo = {.area = PitArea::Ledge, .opening = "west"}}}),
            pitArea("ledge", maps.ledge, {{.name = "west", .side = Side::West, .span = {0, 15}, .transition = Transition::Seamless,
                                  .leadsTo = {.area = PitArea::Pit, .opening = "east"}}}),
        },
        .cellSize = kCell,
        .open = Cell::Open,
        .view = kView,
    };
}

WorldKit::World worldOver(const Maps& maps) {
    return WorldKit::World{.layouts = {
        WorldKit::Layout{
            .name = "farm",
            .areas = {
                layoutZeroArea("yard", maps.yard, {{.name = "east", .side = Side::East, .span = {0, 15},
                                            .transition = Transition::Seamless,
                                            .leadsTo = {.area = Area::Lane, .opening = "west"}},
                                           {.name = "south", .side = Side::South, .span = {8, 11},
                                            .transition = Transition::Scroll,
                                            .leadsTo = {.area = Area::Cellar, .opening = "north"}}}),
                layoutZeroArea("lane", maps.lane, {{.name = "west", .side = Side::West, .span = {0, 15},
                                            .transition = Transition::Seamless,
                                            .leadsTo = {.area = Area::Yard, .opening = "east"}},
                                           {.name = "east", .side = Side::East, .span = {0, 15},
                                            .transition = Transition::Seamless,
                                            .leadsTo = {.area = Area::Field, .opening = "west"}}}),
                layoutZeroArea("cellar", maps.cellar, {{.name = "north", .side = Side::North, .span = {8, 11},
                                              .transition = Transition::Scroll,
                                              .leadsTo = {.area = Area::Yard, .opening = "south"}}}),
                layoutZeroArea("field", maps.field,
                               {{.name = "west", .side = Side::West, .span = {0, 15}, .transition = Transition::Seamless,
                                 .leadsTo = {.area = Area::Lane, .opening = "east"}}},
                               /*rain=*/false),
            },
            .cellSize = kCell,
            .open = Cell::Open,
            .view = kView,
        },
        pitLayout(maps),
    }};
}

// A 10×8 box at (x, y).
IntRect box(int x, int y) {
    return IntRect{.x = x, .y = y, .width = 10, .height = 8};
}

// A camera in the yard looking at a box whose center is at x = 150: the view reaches 70 px into the
// lane, so the yard and the lane are both on screen.
WorldKit::Camera acrossTheSeam(const WorldKit::World& world) {
    WorldKit::Camera camera{0, Area::Yard};
    camera.place(world, box(145, 60));
    return camera;
}

// A camera in the yard on the tick its scroll through the south door into the cellar starts.
WorldKit::Camera scrollingDown(const WorldKit::World& world) {
    WorldKit::Camera camera{0, Area::Yard};
    camera.place(world, box(75, 116));
    (void)camera.follow(world, box(75, 124));   // center (80, 128): out through the door
    return camera;
}

// A camera in the lane looking at a box in its middle: the view takes in the yard to its west, the lane
// and the field to its east.
WorldKit::Camera alongTheChain(const WorldKit::World& world) {
    WorldKit::Camera camera{0, Area::Lane};
    camera.place(world, box(43, 60));
    return camera;
}

// The layer of `layers` keyed `key`.
const retropp::DrawLayer& keyed(const LayerKit::Layers& layers, std::string_view key) {
    for (const retropp::DrawLayer& layer : layers) {
        if (std::string_view{layer.key} == key) {
            return layer;
        }
    }
    throw std::out_of_range("no layer keyed " + std::string{key});
}

// One layer or slot of an area on screen: the depth its area declares and the depth it is drawn at.
struct Placed {
    std::string what;
    std::int32_t declared = 0;
    std::int32_t drawn = 0;
};

// Every layer and actor slot of every area the camera shows: layers with a map as WorldLayers draws
// them, reserved layers and slots as LayerZ and ActorZ place them for that area.
std::vector<Placed> placedOnScreen(const WorldKit::Camera& camera, const WorldKit::World& world) {
    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    std::vector<Placed> placed;
    for (const WorldKit::Shown& shown : camera.shown(world)) {
        const WorldKit::Area& area =
            world.layouts[static_cast<std::size_t>(shown.layout)].areas[static_cast<std::size_t>(shown.area)];
        for (const WorldKit::MapLayer& layer : area.layers) {
            const std::string key = WorldKit::AreaLayerKey(shown.layout, shown.area, layer.name);
            const std::int32_t drawn =
                layer.tiles == nullptr ? WorldKit::LayerZ(world, shown, layer.name) : keyed(layers, key).z;
            placed.push_back({key, layer.z, drawn});
        }
        for (const WorldKit::ActorSlot& slot : area.actorSlots) {
            placed.push_back({slot.name, slot.z, WorldKit::ActorZ(world, shown, slot.name)});
        }
    }
    return placed;
}

// Whether the area `shown` names reserves a layer named "rain".
bool reservesRain(const WorldKit::World& world, const WorldKit::Shown& shown) {
    for (const WorldKit::MapLayer& layer :
         world.layouts[static_cast<std::size_t>(shown.layout)].areas[static_cast<std::size_t>(shown.area)].layers) {
        if (layer.name == "rain") {
            return true;
        }
    }
    return false;
}

// No two drawn at one depth, every lower declared depth drawn under every higher one, and every drawn
// depth at or under the world's top.
void expectInOrder(const std::vector<Placed>& placed, const WorldKit::World& world) {
    for (std::size_t i = 0; i < placed.size(); ++i) {
        EXPECT_LE(placed[i].drawn, WorldKit::TopZ(world)) << placed[i].what;
        for (std::size_t j = 0; j < placed.size(); ++j) {
            if (i == j) {
                continue;
            }
            EXPECT_NE(placed[i].drawn, placed[j].drawn) << placed[i].what << " and " << placed[j].what;
            if (placed[i].declared < placed[j].declared) {
                EXPECT_LT(placed[i].drawn, placed[j].drawn) << placed[i].what << " under " << placed[j].what;
            }
        }
    }
}

// A whole frame as a game builds one: the world's layers; for each area on screen a sprite layer of its
// own at its actor slot and, where it reserves one, a rain layer of its own at the reserved depth, each
// with that area's scroll and the view's stencil; and a HUD above the world.
LayerKit::Layers frameOf(const WorldKit::Camera& camera, const WorldKit::World& world) {
    LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    for (const WorldKit::Shown& shown : camera.shown(world)) {
        const std::string area = std::to_string(shown.layout) + ":" + std::to_string(shown.area);
        layers.push_back(LayerKit::SpriteLayer("things:" + area, WorldKit::ActorZ(world, shown, "actors"), {},
                                               {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(kView)}));
        if (reservesRain(world, shown)) {
            layers.push_back(LayerKit::SpriteLayer("rain:" + area, WorldKit::LayerZ(world, shown, "rain"), {},
                                                   {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(kView)}));
        }
    }
    layers.push_back(LayerKit::SpriteLayer("hud", WorldKit::TopZ(world) + 1, {}));
    return layers;
}

void expectNoTwoAtOneDepth(const LayerKit::Layers& layers) {
    EXPECT_EQ(retropp::findLayerKeyCollision(layers), std::nullopt);
    EXPECT_NO_THROW((void)retropp::layerDrawOrder(layers, retropp::LayerKeyCollisionPolicy::Throw));
}

class Depths : public ::testing::Test {
protected:
    Maps maps;
    WorldKit::World world = worldOver(maps);

    void SetUp() override { WorldKit::validate(world); }
};

// ── The declared depth ───────────────────────────────────────────────────────────────────────────────

TEST_F(Depths, ActorZAndLayerZByPlaceAreTheDeclaredDepthsAndThrowForANameTheAreaLacks) {
    EXPECT_EQ(WorldKit::ActorZ(world, {.layout = 0, .area = Area::Yard}, "actors"), kActorsZ);
    EXPECT_EQ(WorldKit::LayerZ(world, {.layout = 0, .area = Area::Yard}, "rain"), kRainZ);
    EXPECT_EQ(WorldKit::ActorZ(world, {.layout = 1, .area = PitArea::Ledge}, "actors"), kPitActorsZ);
    EXPECT_EQ(WorldKit::LayerZ(world, {.layout = 1, .area = PitArea::Ledge}, "mist"), kPitMistZ);
    EXPECT_EQ(WorldKit::LayerZ(world, {.layout = 0, .area = Area::Yard}, "canopy"), kCanopyZ);   // a layer with a map
    try {
        (void)WorldKit::ActorZ(world, {.layout = 0, .area = Area::Yard}, "flyers");
        ADD_FAILURE() << "a slot the area lacks throws";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string{e.what()}.find("layout 0 'farm' area 0 'yard' has no actor slot 'flyers'"), std::string::npos);
    }
    try {
        (void)WorldKit::LayerZ(world, {.layout = 0, .area = Area::Field}, "rain");
        ADD_FAILURE() << "a layer the area lacks throws";
    } catch (const std::out_of_range& e) {
        EXPECT_NE(std::string{e.what()}.find("layout 0 'farm' area 3 'field' has no layer 'rain'"), std::string::npos);
    }
}

// ── The drawn depth ──────────────────────────────────────────────────────────────────────────────────

TEST_F(Depths, AnAreaAloneOnScreenIsDrawnAtItsDeclaredDepths) {
    WorldKit::Camera camera{0, Area::Cellar};
    camera.place(world, box(75, 60));
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 1u);
    EXPECT_EQ(shown[0].position, 0);
    EXPECT_EQ(shown[0].count, 1);

    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 2u);   // the ground and the canopy; the reserved rain is the game's to build
    EXPECT_EQ(keyed(layers, "area:0:2:ground").z, kGroundZ);
    EXPECT_EQ(keyed(layers, "area:0:2:canopy").z, kCanopyZ);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[0], "actors"), kActorsZ);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[0], "rain"), kRainZ);
    EXPECT_EQ(WorldKit::Actors(camera, world, "actors", {}).z, kActorsZ);
    expectInOrder(placedOnScreen(camera, world), world);

    // A Shown written by hand, its position and count left at their defaults, is an area alone on screen.
    EXPECT_EQ(WorldKit::ActorZ(world, WorldKit::Shown{.layout = 0, .area = Area::Yard}, "actors"), kActorsZ);
}

// The yard first, the lane second: a layer declared at d is drawn at 2d in the yard and 2d + 1 in the lane.
TEST_F(Depths, TwoAreasAcrossASeamSpreadEveryDepthAndKeepEveryLowerOneUnderEveryHigherOne) {
    const WorldKit::Camera camera = acrossTheSeam(world);
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[0].area, Area::Yard);
    EXPECT_EQ(shown[1].area, Area::Lane);
    EXPECT_EQ(shown[0].position, 0);
    EXPECT_EQ(shown[1].position, 1);
    EXPECT_EQ(shown[0].count, 2);
    EXPECT_EQ(shown[1].count, 2);

    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    EXPECT_EQ(keyed(layers, "area:0:0:ground").z, 0);
    EXPECT_EQ(keyed(layers, "area:0:1:ground").z, 1);
    EXPECT_EQ(keyed(layers, "area:0:0:canopy").z, kCanopyZ * 2);
    EXPECT_EQ(keyed(layers, "area:0:1:canopy").z, kCanopyZ * 2 + 1);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[0], "actors"), kActorsZ * 2);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[1], "actors"), kActorsZ * 2 + 1);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[0], "rain"), kRainZ * 2);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[1], "rain"), kRainZ * 2 + 1);
    EXPECT_EQ(WorldKit::Actors(camera, world, "actors", {}).z, kActorsZ * 2);
    expectInOrder(placedOnScreen(camera, world), world);
}

TEST_F(Depths, DuringAScrollTheSameAndOnItsLastTickTheEnteredAreaIsAloneAtItsDeclaredDepths) {
    WorldKit::Camera camera = scrollingDown(world);
    ASSERT_TRUE(camera.transitioning());
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    EXPECT_EQ(shown[1].area, Area::Cellar);
    LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    EXPECT_EQ(keyed(layers, "area:0:0:canopy").z, kCanopyZ * 2);
    EXPECT_EQ(keyed(layers, "area:0:2:canopy").z, kCanopyZ * 2 + 1);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[1], "actors"), kActorsZ * 2 + 1);
    expectInOrder(placedOnScreen(camera, world), world);

    IntRect carried = box(75, 124);
    std::optional<WorldKit::Place> entered;
    while (!entered) {
        const WorldKit::Step step = camera.follow(world, carried);
        carried.x += step.move.x;
        carried.y += step.move.y;
        entered = step.entered;
    }
    EXPECT_EQ(camera.area(), Area::Cellar);
    ASSERT_EQ(camera.shown(world).size(), 1u);
    layers = WorldKit::WorldLayers(camera, world);
    EXPECT_EQ(keyed(layers, "area:0:2:ground").z, kGroundZ);
    EXPECT_EQ(keyed(layers, "area:0:2:canopy").z, kCanopyZ);
    EXPECT_EQ(WorldKit::Actors(camera, world, "actors", {}).z, kActorsZ);
}

TEST_F(Depths, DepthsBelowZeroOrderTheSameWay) {
    WorldKit::Camera camera{1, PitArea::Pit};
    camera.place(world, box(15, 60));   // the view ends where the ledge begins: the pit alone
    ASSERT_EQ(camera.shown(world).size(), 1u);
    EXPECT_EQ(keyed(WorldKit::WorldLayers(camera, world), "area:1:0:ground").z, kPitGroundZ);
    EXPECT_EQ(WorldKit::LayerZ(world, camera.shown(world)[0], "mist"), kPitMistZ);
    EXPECT_EQ(WorldKit::Actors(camera, world, "actors", {}).z, kPitActorsZ);
    expectInOrder(placedOnScreen(camera, world), world);

    camera.place(world, box(145, 60));   // the ledge in view too
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 2u);
    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    EXPECT_EQ(keyed(layers, "area:1:0:ground").z, kPitGroundZ * 2);
    EXPECT_EQ(keyed(layers, "area:1:1:ground").z, kPitGroundZ * 2 + 1);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[0], "mist"), kPitMistZ * 2);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[1], "mist"), kPitMistZ * 2 + 1);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[0], "actors"), kPitActorsZ * 2);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[1], "actors"), kPitActorsZ * 2 + 1);
    expectInOrder(placedOnScreen(camera, world), world);
}

// The lane is current; its west seam is listed first, so the yard is second and the field third.
TEST_F(Depths, ThreeAreasThroughAChainOfSeamsTakeThreePlaces) {
    const WorldKit::Camera camera = alongTheChain(world);
    const std::vector<WorldKit::Shown> shown = camera.shown(world);
    ASSERT_EQ(shown.size(), 3u);
    EXPECT_EQ(shown[0].area, Area::Lane);
    EXPECT_EQ(shown[1].area, Area::Yard);
    EXPECT_EQ(shown[2].area, Area::Field);
    for (int i = 0; i < 3; ++i) {
        EXPECT_EQ(shown[static_cast<std::size_t>(i)].position, i);
        EXPECT_EQ(shown[static_cast<std::size_t>(i)].count, 3);
    }
    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    EXPECT_EQ(keyed(layers, "area:0:1:ground").z, 0);
    EXPECT_EQ(keyed(layers, "area:0:0:ground").z, 1);
    EXPECT_EQ(keyed(layers, "area:0:3:ground").z, 2);
    EXPECT_EQ(keyed(layers, "area:0:3:canopy").z, kCanopyZ * 3 + 2);
    EXPECT_EQ(WorldKit::ActorZ(world, shown[2], "actors"), kActorsZ * 3 + 2);
    EXPECT_EQ(WorldKit::LayerZ(world, shown[1], "rain"), kRainZ * 3 + 1);
    expectInOrder(placedOnScreen(camera, world), world);
}

// ── The world's top ──────────────────────────────────────────────────────────────────────────────────

TEST_F(Depths, TopZIsTheHighestDepthAnyLayerIsDrawnAt) {
    EXPECT_EQ(WorldKit::TopZ(world), kCanopyZ * kAreas + kAreas - 1);
    world.layouts[0].areas[Area::Yard].layers.push_back(WorldKit::ReservedLayer{.name = "birds", .z = 30});
    EXPECT_EQ(WorldKit::TopZ(world), 30 * kAreas + kAreas - 1);
    EXPECT_EQ(WorldKit::TopZ(WorldKit::World{}), std::numeric_limits<std::int32_t>::min());

    // A world whose every depth is below zero: an area alone draws its highest at its declared depth.
    const WorldKit::World pits{.layouts = {pitLayout(maps)}};
    WorldKit::validate(pits);
    EXPECT_EQ(WorldKit::TopZ(pits), kPitActorsZ);
    WorldKit::Camera camera{0, PitArea::Pit};
    camera.place(pits, box(15, 60));
    EXPECT_EQ(WorldKit::Actors(camera, pits, "actors", {}).z, WorldKit::TopZ(pits));
    camera.place(pits, box(145, 60));
    expectInOrder(placedOnScreen(camera, pits), pits);
}

// ── The check ────────────────────────────────────────────────────────────────────────────────────────

// Whether validate throws, and what it says.
std::string rejection(const WorldKit::World& world) {
    try {
        WorldKit::validate(world);
    } catch (const std::invalid_argument& e) {
        return e.what();
    }
    return "";
}

// With 6 areas, a depth z is drawn between z × 6 and z × 6 + 5: the highest that fits is
// 357913940 (… 2147483645) and the lowest −357913941 (−2147483646 …).
TEST_F(Depths, ValidateRejectsADepthOutOfRangeOnceSpreadAndChecksAReservedLayerAsItChecksAnyLayer) {
    WorldKit::Area& yard = world.layouts[0].areas[Area::Yard];
    WorldKit::MapLayer& rain = yard.layers[2];
    ASSERT_EQ(rain.name, "rain");
    EXPECT_EQ(rain.tiles, nullptr);
    yard.layers[1].z = 357913940;
    EXPECT_EQ(rejection(world), "");
    yard.layers[1].z = 357913941;
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'canopy' is at a depth out of range for a world of 6 areas");
    yard.layers[1].z = kCanopyZ;
    yard.layers[0].z = -357913941;
    EXPECT_EQ(rejection(world), "");
    yard.layers[0].z = -357913942;
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'ground' is at a depth out of range for a world of 6 areas");
    yard.layers[0].z = kGroundZ;
    rain.z = 357913941;
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'rain' is at a depth out of range for a world of 6 areas");
    rain.z = kRainZ;
    world.layouts[0].areas[Area::Lane].actorSlots[0].z = -357913942;
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 1 'lane' layer 'actors' is at a depth out of range for a world of 6 areas");
    world.layouts[0].areas[Area::Lane].actorSlots[0].z = kActorsZ;
    EXPECT_EQ(rejection(world), "");

    rain.name = "canopy";
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'canopy' is named twice");
    rain.name = "actors";   // the slots come after the layers, so the slot is the one named twice
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'actors' is named twice");
    rain.name = "";
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' has a layer with no name");
    rain = WorldKit::ReservedLayer{.name = "rain", .z = kCanopyZ};
    EXPECT_EQ(rejection(world), "WorldKit: layout 0 'farm' area 0 'yard' layer 'rain' is at the same depth as layer 'canopy'");
}

// ── No two layers at one depth ───────────────────────────────────────────────────────────────────────

// The defect: two areas on screen, every layer at a depth of its own.
TEST_F(Depths, TwoAreasAcrossASeamSubmitNoTwoLayersAtOneDepth) {
    const WorldKit::Camera camera = acrossTheSeam(world);
    ASSERT_EQ(camera.shown(world).size(), 2u);
    const LayerKit::Layers layers = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(layers.size(), 4u);
    expectNoTwoAtOneDepth(layers);
}

TEST_F(Depths, DuringAScrollNoTwoLayersShareADepth) {
    const WorldKit::Camera camera = scrollingDown(world);
    ASSERT_EQ(camera.shown(world).size(), 2u);
    expectNoTwoAtOneDepth(WorldKit::WorldLayers(camera, world));
}

TEST_F(Depths, AWholeFrameBuiltAsAGameBuildsItHasNoTwoLayersAtOneDepth) {
    const LayerKit::Layers seam = frameOf(acrossTheSeam(world), world);
    EXPECT_EQ(seam.size(), 4u + 2u * 2u + 1u);   // four map layers, a slot and rain for each area, the HUD
    expectNoTwoAtOneDepth(seam);
    expectNoTwoAtOneDepth(frameOf(scrollingDown(world), world));
    const LayerKit::Layers chain = frameOf(alongTheChain(world), world);
    EXPECT_EQ(chain.size(), 6u + 3u + 2u + 1u);   // rain for the lane and the yard alone
    expectNoTwoAtOneDepth(chain);
}

// ── Keys and scrolls ─────────────────────────────────────────────────────────────────────────────────

TEST_F(Depths, WhenANeighborComesIntoViewOrTheCurrentAreaChangesEveryLayerKeepsItsKeyAndScroll) {
    // The yard alone, the camera at x = 0; a step right brings the lane into view.
    WorldKit::Camera camera{0, Area::Yard};
    camera.place(world, box(75, 60));
    const LayerKit::Layers alone = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(alone.size(), 2u);
    (void)camera.follow(world, box(76, 60));
    const LayerKit::Layers joined = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(joined.size(), 4u);
    for (const char* key : {"area:0:0:ground", "area:0:0:canopy"}) {
        EXPECT_EQ(keyed(alone, key).scroll, (retropp::LayerScroll{.x = 0, .y = -16})) << key;
        EXPECT_EQ(keyed(joined, key).scroll, camera.scroll(world)) << key;   // exactly the camera's step
    }
    EXPECT_EQ(keyed(alone, "area:0:0:canopy").z, kCanopyZ);
    EXPECT_EQ(keyed(joined, "area:0:0:canopy").z, kCanopyZ * 2);

    // Across the seam: the camera moves a pixel with the box, and the lane becomes current. Every layer
    // keeps its key, every scroll moves by that pixel alone, and the two areas trade places.
    camera.place(world, box(154, 60));   // center (159, 64): still in the yard
    ASSERT_EQ(camera.area(), Area::Yard);
    const LayerKit::Layers before = WorldKit::WorldLayers(camera, world);
    const WorldKit::Step step = camera.follow(world, box(155, 60));   // center (160, 64): past the wall
    ASSERT_EQ(step.entered, (WorldKit::Place{.layout = 0, .area = Area::Lane}));
    const LayerKit::Layers after = WorldKit::WorldLayers(camera, world);
    ASSERT_EQ(before.size(), 4u);
    ASSERT_EQ(after.size(), 4u);
    for (const retropp::DrawLayer& layer : before) {
        const retropp::DrawLayer& same = keyed(after, std::string_view{layer.key});
        EXPECT_EQ(same.scroll.x - layer.scroll.x, 1) << std::string_view{layer.key};
        EXPECT_EQ(same.scroll.y, layer.scroll.y) << std::string_view{layer.key};
    }
    EXPECT_EQ(keyed(before, "area:0:0:ground").z, 0);
    EXPECT_EQ(keyed(after, "area:0:0:ground").z, 1);
    EXPECT_EQ(keyed(before, "area:0:1:ground").z, 1);
    EXPECT_EQ(keyed(after, "area:0:1:ground").z, 0);
}

}  // namespace
