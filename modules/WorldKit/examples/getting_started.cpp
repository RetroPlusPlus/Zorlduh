// A complete program built on the WorldKit module, run by its scene: a castle and the grounds outside it.
//
//   - The castle: a hall with a door in its east wall that scrolls into a wider hall; stairs in that hall
//     that fade down to a small cellar with stairs back up.
//   - The grounds: a garden joined seamlessly along the wide hall's north wall, so it is there beyond the
//     wall and walking into it the picture holds still, with trees whose canopy the marker passes under
//     and leaves falling from them; and an orchard east of the garden through a door that scrolls.
//   - Five inhabitants, each written in the world where it belongs: the marker, steered with the arrow
//     keys, WASD or a d-pad, goes anywhere and the camera follows it; a guard paces the hall and stops at
//     its door; a gardener tends the grounds, through the orchard's door and back, and stops at the seam
//     into the castle; a hound roams the castle and stops at the seam until the marker touches it, and from
//     then on goes anywhere; a bat in the cellar flies onto the stairs and is gone.
//
// Every inhabitant moves with a few lines of the program's own, a Rover; its scripts steer it. The scene
// spawns them, runs them, takes each through the openings its reach allows, and draws the world.
//
// docs/getting-started.md walks through it part by part.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/tilemap.h"
#include "retropp/timing.h"
#include "retropp/windowed_host.h"

#include "CameraKit/camera.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/types.h"
#include "ScriptKit/command.h"
#include "ScriptKit/entry.h"
#include "ScriptKit/script.h"
#include "ScriptKit/value.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/scene.h"
#include "WorldKit/world_layers.h"

#include "ActorKit/actor.h"
#include "ActorKit/commands.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"
#include "ActorKit/mover.h"
#include "ActorKit/triggers.h"

namespace {

using ScriptKit::loop;
using ScriptKit::wait;

enum class Action : std::uint8_t { Up, Down, Left, Right };

// ── The world ────────────────────────────────────────────────────────────────────────────────────

// The program's two layouts, by index in the world, and each one's areas, by index in the layout.
enum Layout : std::uint8_t { Castle, Grounds };
enum CastleArea : std::uint8_t { Hall, WideHall, Cellar };
enum GroundsArea : std::uint8_t { Garden, Orchard };

constexpr int kCell = 8;

// The whole screen is the view: nothing sits above or beside the world.
constexpr CameraKit::View kView{.size = {.width = 160, .height = 144}};

// The values in a map, drawn as the tiles of the same name.
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };

// The values in a canopy map: open sky, or the trees' leaves.
enum class Overhead : std::uint16_t { Nothing = 0, Canopy = 1 };

// Every area's depths: the ground, the inhabitants over it, the garden's falling leaves over them, and
// the canopy over the leaves.
constexpr std::int32_t kGroundZ = 0;
constexpr std::int32_t kActorsZ = 5;
constexpr std::int32_t kLeavesZ = 7;
constexpr std::int32_t kCanopyZ = 10;

// The cells of one wall from `first` to `last` along it: a doorway list for Room.
std::vector<retropp::Vec2i> Across(int y, int first, int last) {
    std::vector<retropp::Vec2i> cells;
    for (int x = first; x <= last; ++x) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<retropp::Vec2i> Down(int x, int first, int last) {
    std::vector<retropp::Vec2i> cells;
    for (int y = first; y <= last; ++y) {
        cells.push_back({.x = x, .y = y});
    }
    return cells;
}

std::vector<retropp::Vec2i> Both(std::vector<retropp::Vec2i> a, const std::vector<retropp::Vec2i>& b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// A map ringed by wall, floor inside, with doorways opened in the ring and stairs placed inside.
retropp::IndexGrid Room(int width, int height, const std::vector<retropp::Vec2i>& doorways,
                        const std::vector<retropp::Vec2i>& stairs) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    const auto set = [&](retropp::Vec2i cell, Cell value) {
        map.values[static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(cell.x)] = static_cast<std::uint16_t>(value);
    };
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            set({.x = x, .y = y}, edge ? Cell::Wall : Cell::Floor);
        }
    }
    for (const retropp::Vec2i door : doorways) {
        set(door, Cell::Floor);
    }
    for (const retropp::Vec2i step : stairs) {
        set(step, Cell::Stairs);
    }
    return map;
}

// The collision map of a room: its walls as they are, every other cell floor, so its stairs are walked on.
retropp::IndexGrid Walkable(retropp::IndexGrid map) {
    for (std::uint16_t& value : map.values) {
        if (value == static_cast<std::uint16_t>(Cell::Stairs)) {
            value = static_cast<std::uint16_t>(Cell::Floor);
        }
    }
    return map;
}

// A map of `width` × `height` cells holding Nothing but for Canopy in each rectangle of `trees`.
retropp::IndexGrid Canopy(int width, int height, const std::vector<retropp::IntRect>& trees) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height),
                      static_cast<std::uint16_t>(Overhead::Nothing));
    for (const retropp::IntRect tree : trees) {
        for (int y = tree.y; y < tree.y + tree.height; ++y) {
            for (int x = tree.x; x < tree.x + tree.width; ++x) {
                map.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                    static_cast<std::uint16_t>(Overhead::Canopy);
            }
        }
    }
    return map;
}

// The maps. Every area's map is its own pixel space; the openings below say how they join. The wide
// hall's north wall and the garden's south wall are open along their whole length but the corners, so
// the two meet along the full width. The garden's east wall and the orchard's west wall each have a door
// at rows 4 to 7. The two rooms with stairs have a collision map of their own, the stairs as floor; the
// other rooms are drawn from their collision maps. The garden's canopy has two stands of trees along its
// north side, the orchard's three.
struct Maps {
    retropp::IndexGrid hall = Room(20, 18, Down(19, 7, 10), {});
    retropp::IndexGrid wideHall = Room(30, 18, Both(Down(0, 7, 10), Across(0, 1, 28)), {{20, 5}, {21, 5}});
    retropp::IndexGrid wideHallFloor = Walkable(wideHall);
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
    retropp::IndexGrid cellarFloor = Walkable(cellar);
    retropp::IndexGrid garden = Room(30, 12, Both(Across(11, 1, 28), Down(29, 4, 7)), {});
    retropp::IndexGrid gardenCanopy = Canopy(30, 12, {{.x = 4, .y = 1, .width = 6, .height = 3},
                                                      {.x = 18, .y = 1, .width = 7, .height = 3}});
    retropp::IndexGrid orchard = Room(20, 12, Down(0, 4, 7), {});
    retropp::IndexGrid orchardCanopy = Canopy(20, 12, {{.x = 3, .y = 1, .width = 4, .height = 3},
                                                       {.x = 10, .y = 8, .width = 4, .height = 3},
                                                       {.x = 15, .y = 2, .width = 3, .height = 4}});
};

// ── The art ──────────────────────────────────────────────────────────────────────────────────────

// The tiles of the main sheet, 8×8 each: the three map tiles, then one for each inhabitant.
enum Tile : std::uint16_t { kFloor, kWall, kStairs, kMarker, kGuard, kGardener, kHound, kBat };

// The art: a sheet of eight 8×8 tiles; a sheet of three for what hangs over the grounds (a clear slot,
// the canopy, and a falling leaf), whose index 0 is transparent so the ground shows through; and three
// palettes, one for the halls, one for the cellar and one for the grounds.
struct Art {
    retropp::AtlasId tiles{};
    retropp::AtlasId overhead{};
    retropp::PaletteId hall{};
    retropp::PaletteId cellar{};
    retropp::PaletteId garden{};
};

// The palette index of pixel (x, y) of each inhabitant's tile: the marker a ring; the guard a helmet over
// a body; the gardener a wide hat over a body; the hound long and low, with a head at the front; the bat
// two wings beside a body. Index 0 is the sheet's transparent index, so each shows the ground around it.
std::uint8_t InhabitantPixel(Tile tile, int x, int y) {
    const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
    switch (tile) {
        case kMarker: return rim ? 3 : 0;
        case kGuard: return y <= 1 ? 3 : (x >= 2 && x <= 5 ? 2 : 0);
        case kGardener: return y == 2 ? 3 : (y > 2 && x >= 2 && x <= 5 ? 1 : 0);
        case kHound: return y >= 4 && y <= 6 ? 2 : (y >= 2 && y <= 3 && x >= 5 ? 3 : 0);
        case kBat: return (y >= 2 && y <= 4 && (x <= 2 || x >= 5)) ? 2 : (x >= 3 && x <= 4 && y >= 1 && y <= 5 ? 3 : 0);
        case kFloor:
        case kWall:
        case kStairs:
        default: return 0;
    }
}

Art uploadArt(retropp::Renderer& renderer) {
    constexpr int kWidth = 8 * kCell;
    std::array<std::uint8_t, kWidth * kCell> sheet{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
            const auto at = [&](Tile tile) -> std::uint8_t& {
                return sheet[static_cast<std::size_t>(y) * kWidth + static_cast<std::size_t>(tile) * kCell + x];
            };
            at(kFloor) = (x + y) % 4 == 0 ? 2 : 1;
            at(kWall) = rim ? 3 : 2;
            at(kStairs) = y % 2 == 0 ? 3 : 0;
            for (const Tile inhabitant : {kMarker, kGuard, kGardener, kHound, kBat}) {
                at(inhabitant) = InhabitantPixel(inhabitant, x, y);
            }
        }
    }
    // Slot 0 is every pixel index 0, so it draws nothing; slot 1 is leaves with gaps between them; slot 2
    // is one leaf, a small diamond.
    std::array<std::uint8_t, 24 * 8> overhead{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            overhead[static_cast<std::size_t>(y) * 24 + kCell + x] = (x * 3 + y * 5) % 7 == 0 ? 0 : 1 + (x + y) % 2;
            const int fromMiddle = (x > 3 ? x - 3 : 3 - x) + (y > 3 ? y - 3 : 3 - y);
            overhead[static_cast<std::size_t>(y) * 24 + 2 * kCell + x] = fromMiddle <= 2 ? 2 + (x + y) % 2 : 0;
        }
    }
    const std::array<retropp::Rgba8, 4> hall{{{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    const std::array<retropp::Rgba8, 4> cellar{{{10, 10, 20}, {60, 50, 70}, {120, 100, 130}, {230, 210, 240}}};
    const std::array<retropp::Rgba8, 4> garden{{{10, 30, 10}, {40, 110, 50}, {90, 170, 80}, {220, 240, 180}}};
    return Art{.tiles = renderer.uploadAtlas(sheet.data(), kWidth, kCell).atlasId,
               .overhead = renderer.uploadAtlas(overhead.data(), 24, 8, retropp::TransparentIndices::of({0})).atlasId,
               .hall = renderer.uploadPalette(std::span<const retropp::Rgba8>(hall)),
               .cellar = renderer.uploadPalette(std::span<const retropp::Rgba8>(cellar)),
               .garden = renderer.uploadPalette(std::span<const retropp::Rgba8>(garden))};
}

// How an area's map values are drawn: each value's tile, through one palette.
retropp::TileCatalog CatalogThrough(const Art& art, retropp::PaletteId palette) {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = art.tiles, .slot = kFloor, .palette = palette},
        {.id = 1, .sheet = art.tiles, .slot = kWall, .palette = palette},
        {.id = 2, .sheet = art.tiles, .slot = kStairs, .palette = palette},
    }};
}

// How a canopy map's values are drawn: open sky through the transparent slot, leaves through the grounds'
// palette.
retropp::TileCatalog OverheadArt(const Art& art) {
    return retropp::TileCatalog{.entries = {
        {.id = static_cast<std::uint16_t>(Overhead::Nothing), .sheet = art.overhead, .slot = 0, .palette = art.garden},
        {.id = static_cast<std::uint16_t>(Overhead::Canopy), .sheet = art.overhead, .slot = 1, .palette = art.garden},
    }};
}

// The look of one inhabitant: its tile of the main sheet through `palette`.
ActorKit::Look LookOf(const Art& art, Tile tile, retropp::PaletteId palette) {
    return ActorKit::Look{.size = {.width = kCell, .height = kCell}, .atlas = art.tiles, .tile = tile, .palette = palette};
}

// The garden's falling leaves on tick `ticks`: eight, each drifting down the garden a pixel every three
// ticks and swaying a pixel or two to the side, from the garden's top edge until its own bottom edge
// meets the garden's, then from the top again, so every leaf stays wholly inside the garden. A leaf takes
// a new key each time it starts again, so it appears at the top in place of sliding back up.
LayerKit::Sprites Leaves(std::uint32_t ticks, const retropp::IndexGrid& garden, const Art& art) {
    constexpr std::array<int, 4> kSway{0, 1, 2, 1};
    const int fall = garden.height * kCell - kCell + 1;   // the rows a leaf's top edge takes
    LayerKit::Sprites leaves;
    for (int n = 0; n < 8; ++n) {
        const int fallen = static_cast<int>(ticks / 3) + n * 13;
        const int sway = kSway[static_cast<std::size_t>((ticks / 20 + static_cast<std::uint32_t>(n)) % kSway.size())];
        leaves.push_back(retropp::Sprite{.key = "leaf:" + std::to_string(n) + ":" + std::to_string(fallen / fall),
                                         .x = 12 + n * 28 + sway,
                                         .y = fallen % fall,
                                         .size = {.width = kCell, .height = kCell},
                                         .atlas = art.overhead,
                                         .tile = 2,
                                         .palette = art.garden});
    }
    return leaves;
}

// ── The mover ────────────────────────────────────────────────────────────────────────────────────

// Which way a rover is headed: up, down, left or right, or standing.
struct Way {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
};

struct RoverConfig {
    int speed = 1;
    retropp::IntRect collision{.x = 0, .y = 0, .width = kCell, .height = kCell};
    retropp::AnimationFrame frame{};   // empty: every inhabitant shows its look
};

struct RoverShape {
    retropp::IntRect box;
    [[nodiscard]] retropp::IntRect bounds() const { return box; }
};

// The program's own mover: `speed` pixels a tick on each axis of its heading, a pixel at a time, stopping
// where its box would meet a solid, and facing the last axis it was headed along.
class Rover {
public:
    explicit Rover(retropp::Vec2i& at) : at_(&at) {}
    void bind(retropp::Vec2i& at) { at_ = &at; }

    void walk(const RoverConfig& config, Way way, const ActorKit::Solids& solids) {
        const int dx = (way.right ? 1 : 0) - (way.left ? 1 : 0);
        const int dy = (way.down ? 1 : 0) - (way.up ? 1 : 0);
        if (dx != 0) {
            facing_ = dx > 0 ? retropp::Dir::Right : retropp::Dir::Left;
        }
        if (dy != 0) {
            facing_ = dy > 0 ? retropp::Dir::Down : retropp::Dir::Up;
        }
        for (const retropp::Vec2i unit : {retropp::Vec2i{.x = dx, .y = 0}, retropp::Vec2i{.x = 0, .y = dy}}) {
            if (unit.x == 0 && unit.y == 0) {
                continue;
            }
            for (int n = 0; n < config.speed; ++n) {
                const retropp::IntRect next{.x = at_->x + unit.x + config.collision.x,
                                            .y = at_->y + unit.y + config.collision.y,
                                            .width = config.collision.width,
                                            .height = config.collision.height};
                if (solids.solidBox(next)) {
                    break;
                }
                at_->x += unit.x;
                at_->y += unit.y;
            }
        }
    }

    [[nodiscard]] retropp::Dir facing() const { return facing_; }
    [[nodiscard]] RoverShape collision(const RoverConfig& config) const {
        return {{.x = at_->x + config.collision.x, .y = at_->y + config.collision.y,
                 .width = config.collision.width, .height = config.collision.height}};
    }
    [[nodiscard]] const retropp::AnimationFrame& frame(const RoverConfig& config) const { return config.frame; }

private:
    retropp::Vec2i* at_;
    retropp::Dir facing_ = retropp::Dir::Down;
};

// ── The input and the state ──────────────────────────────────────────────────────────────────────

// What the player does on one tick: the way the pad is held.
struct Input {
    Way way;
};

Input readInput(const retropp::InputState& in) {
    return Input{.way = {.up = in.isHeld(retropp::actionId(Action::Up)),
                         .down = in.isHeld(retropp::actionId(Action::Down)),
                         .left = in.isHeld(retropp::actionId(Action::Left)),
                         .right = in.isHeld(retropp::actionId(Action::Right))}};
}

struct State {
    WorldKit::Scene scene;
    std::uint32_t ticks = 0;   // how many ticks have run: the leaves fall by it
};

// ── The inhabitants and their scripts ────────────────────────────────────────────────────────────

// The way an inhabitant's scripts steer it: each scripted inhabitant holds its own.
const ScriptKit::Value<retropp::Dir> Heading;

// What the hound's script asks of the program when the marker touches it.
const ScriptKit::Command GoAnywhere{"go anywhere"};

// The guard paces the hall: east to the door, where it is held at the edge for the rest of the leg,
// and back west.
const ScriptKit::Script pace{
    .name = "pace",
    .on = ActorKit::Enter,
    .body = {loop({set(Heading, retropp::Dir::Right), wait(160), set(Heading, retropp::Dir::Left), wait(160)})},
};

// The hound roams a clockwise round of the wide hall, standing a moment between legs: up to the north
// edge, where the seam into the grounds holds it, right to the east wall, down and left. The legs up and
// right end against what holds it, so each round lines itself up again whatever stood in its way. The leg
// up is the longer, so once the hound goes anywhere it takes it on across the seam into the garden, and
// the leg down brings it back.
const ScriptKit::Script roam{
    .name = "roam",
    .on = ActorKit::Enter,
    .body = {loop({set(Heading, retropp::Dir::Up), wait(130), set(Heading, retropp::Dir::None), wait(40),
                   set(Heading, retropp::Dir::Right), wait(140), set(Heading, retropp::Dir::None), wait(40),
                   set(Heading, retropp::Dir::Down), wait(110), set(Heading, retropp::Dir::None), wait(40),
                   set(Heading, retropp::Dir::Left), wait(104), set(Heading, retropp::Dir::None), wait(40)})},
};

// Touched by the marker, the hound asks to go anywhere.
const ScriptKit::Script goAnywhere{.name = "go anywhere", .on = ActorKit::Touch, .body = {GoAnywhere()}};

// The gardener tends the grounds: east through the garden's door to the orchard's far wall, back west into
// the middle of the garden, south to the garden's south edge, where the seam into the castle holds it,
// and north to the door's rows. The legs east and south end against what holds it, so each round lines
// itself up again whatever stood in its way.
const ScriptKit::Script tend{
    .name = "tend",
    .on = ActorKit::Enter,
    .body = {loop({set(Heading, retropp::Dir::Right), wait(320), set(Heading, retropp::Dir::None), wait(30),
                   set(Heading, retropp::Dir::Left), wait(288), set(Heading, retropp::Dir::None), wait(30),
                   set(Heading, retropp::Dir::Down), wait(80), set(Heading, retropp::Dir::None), wait(30),
                   set(Heading, retropp::Dir::Up), wait(48), set(Heading, retropp::Dir::None), wait(30)})},
};

// The bat flies east, onto the cellar's stairs.
const ScriptKit::Script flit{.name = "flit", .on = ActorKit::Enter, .body = {set(Heading, retropp::Dir::Right)}};

ActorKit::Actor Marker(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .look = LookOf(art, kMarker, art.hall),
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, &Input::way),
            .setsOff = {ActorKit::On::Touch}};
}

ActorKit::Actor Guard(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, retropp::Dir::None}},
            .look = LookOf(art, kGuard, art.hall),
            .scripts = {&pace},
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, Heading)};
}

// Passable, so the marker can walk onto it and touch it.
ActorKit::Actor Hound(const Art& art) {
    return {.slot = "actors",
            .ownValues = {{Heading, retropp::Dir::None}},
            .look = LookOf(art, kHound, art.hall),
            .scripts = {&roam, &goAnywhere},
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, Heading)};
}

ActorKit::Actor Gardener(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, retropp::Dir::None}},
            .look = LookOf(art, kGardener, art.hall),
            .scripts = {&tend},
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, Heading)};
}

ActorKit::Actor Bat(const Art& art) {
    return {.slot = "actors",
            .ownValues = {{Heading, retropp::Dir::None}},
            .look = LookOf(art, kBat, art.cellar),
            .scripts = {&flit},
            .mover = ActorKit::movedBy<Rover>(RoverConfig{}, Heading)};
}

// The one game decision in the program: the hound, once touched, goes anywhere.
const ActorKit::Handlers<State> handlers{
    {GoAnywhere, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments&) {
         state.scene.actor(asker).reach(WorldKit::Reach::World);   // this is now a global actor
     }},
};

// ── The world, with who lives where ──────────────────────────────────────────────────────────────

// An area's ground: its map through one palette, at the bottom.
WorldKit::MapLayer Ground(const retropp::IndexGrid& map, const Art& art, retropp::PaletteId palette) {
    return WorldKit::MapLayer{.name = "ground", .tiles = &map, .art = CatalogThrough(art, palette), .z = kGroundZ};
}

// The one actor slot every area has: where the inhabitants are drawn.
const std::vector<WorldKit::ActorSlot> kActorSlots{{.name = "actors", .z = kActorsZ}};

WorldKit::World CastleWorld(const Maps& maps, const Art& art) {
    using WorldKit::Side;
    using WorldKit::Transition;
    return WorldKit::World{
        .layouts = {
            WorldKit::Layout{
                .name = "castle",
                .areas = {
                    WorldKit::Area{.name = "hall", .layers = {Ground(maps.hall, art, art.hall)}, .actorSlots = kActorSlots,
                                   .collision = &maps.hall,
                                   .openings = {{.name = "east door", .side = Side::East, .span = {7, 10},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = CastleArea::WideHall, .opening = "west door"}}},
                                   .actors = {{.actor = Guard(art), .at = {40, 64}}}},
                    WorldKit::Area{.name = "wide hall", .layers = {Ground(maps.wideHall, art, art.hall)},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.wideHallFloor,
                                   .openings = {{.name = "west door", .side = Side::West, .span = {7, 10},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = CastleArea::Hall, .opening = "east door"}},
                                                {.name = "stairs down", .at = {.x = 20, .y = 5, .width = 2, .height = 1},
                                                 .transition = Transition::Fade,
                                                 .leadsTo = {.area = CastleArea::Cellar, .opening = "stairs up"}},
                                                {.name = "north edge", .side = Side::North, .span = {1, 28},
                                                 .transition = Transition::Seamless,
                                                 .leadsTo = {.layout = Layout::Grounds, .area = GroundsArea::Garden,
                                                             .opening = "south edge"}}}},
                    WorldKit::Area{.name = "cellar", .layers = {Ground(maps.cellar, art, art.cellar)},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.cellarFloor,
                                   .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                                 .transition = Transition::Fade,
                                                 .leadsTo = {.area = CastleArea::WideHall, .opening = "stairs down"}}},
                                   .actors = {{.actor = Bat(art), .at = {24, 30}, .atEdge = WorldKit::AtEdge::Despawn,
                                               .awakeWhen = WorldKit::AwakeWhen::OnScreen}}},
                },
                .actors = {{.actor = Hound(art), .in = CastleArea::WideHall, .at = {120, 80}}},
                .cellSize = kCell,
                .open = Cell::Floor,   // the one walkable value: a wall is solid
                .view = kView,
            },
            WorldKit::Layout{
                .name = "grounds",
                .areas = {
                    // The canopy is above the actor slot, so the inhabitants walk under the trees; the garden
                    // reserves a depth between them for the falling leaves the program draws itself.
                    WorldKit::Area{.name = "garden",
                                   .layers = {Ground(maps.garden, art, art.garden),
                                              WorldKit::ReservedLayer{.name = "leaves", .z = kLeavesZ},
                                              {.name = "canopy", .tiles = &maps.gardenCanopy, .art = OverheadArt(art),
                                               .z = kCanopyZ}},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.garden,
                                   .openings = {{.name = "south edge", .side = Side::South, .span = {1, 28},
                                                 .transition = Transition::Seamless,
                                                 .leadsTo = {.layout = Layout::Castle, .area = CastleArea::WideHall,
                                                             .opening = "north edge"}},
                                                {.name = "east door", .side = Side::East, .span = {4, 7},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = GroundsArea::Orchard, .opening = "west door"}}}},
                    WorldKit::Area{.name = "orchard",
                                   .layers = {Ground(maps.orchard, art, art.garden),
                                              {.name = "canopy", .tiles = &maps.orchardCanopy, .art = OverheadArt(art),
                                               .z = kCanopyZ}},
                                   .actorSlots = kActorSlots,
                                   .collision = &maps.orchard,
                                   .openings = {{.name = "west door", .side = Side::West, .span = {4, 7},
                                                 .transition = Transition::Scroll,
                                                 .leadsTo = {.area = GroundsArea::Garden, .opening = "east door"}}}},
                },
                .actors = {{.actor = Gardener(art), .in = GroundsArea::Garden, .at = {96, 40},
                            .awakeWhen = WorldKit::AwakeWhen::InLayout}},
                .cellSize = kCell,
                .open = Cell::Floor,
                .view = kView,
            },
        },
        .actors = {{.name = "marker", .actor = Marker(art), .in = {Layout::Castle, CastleArea::Hall}, .at = {76, 68}}},
    };
}

}  // namespace

int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "WorldKit Getting Started"},
        .window = {.title = "WorldKit — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));

    const Art art = uploadArt(renderer);
    const Maps maps;
    const WorldKit::World world = CastleWorld(maps, art);
    WorldKit::validate(world);

    // The scene: the world, the camera the program creates, handed over, and the program's handler. The
    // camera follows the marker, which it looks at from the first frame.
    WorldKit::Camera camera{Layout::Castle, CastleArea::Hall};
    camera.place(world, {.x = 76, .y = 68, .width = kCell, .height = kCell});
    const ActorKit::Config config{.handlers = &handlers};
    State state{.scene = WorldKit::Scene{world, camera, config}};
    state.scene.follow("marker");

    loop.simTick([&](const retropp::InputState& input) {
        ++state.ticks;
        state.scene.tick(readInput(input), state);
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        frame.layers = state.scene.layers(state);
        // The garden's leaves on a layer of the program's own, at the depth the garden reserves for them:
        // over the inhabitants and under the canopy, the garden alone on screen or beside the wide hall.
        for (const WorldKit::Shown& shown : state.scene.camera().shown(world)) {
            if (shown.layout == Layout::Grounds && shown.area == GroundsArea::Garden) {
                frame.layers.push_back(LayerKit::SpriteLayer("garden:leaves", WorldKit::LayerZ(world, shown, "leaves"),
                                                             {Leaves(state.ticks, maps.garden, art)},
                                                             {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(kView)}));
            }
        }
        frame.regions = state.scene.fade();
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
