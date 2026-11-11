// A complete program built on the WorldKit module: four areas joined by their openings — a hall with
// a door in its east wall that scrolls into a wider hall; stairs in that hall that fade down to a small
// cellar with stairs back up; and a garden joined seamlessly along the wider hall's north wall, so it
// is simply there beyond the wall and walking into it changes nothing on screen, with trees whose
// canopy the marker passes under — and a marker steered with the arrow keys, WASD or a d-pad. The
// marker's own movement is a few lines here; WorldKit tells it what is solid, takes it through each
// opening, and draws whatever is on screen, the marker included, at the depth each area gives it.
//
// docs/getting-started.md walks through it part by part.

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

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

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/solids.h"
#include "WorldKit/world_layers.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right };

// The program's one layout, by index in the world, and its areas, by index in the layout.
enum Layout : std::uint8_t { Castle };
enum Area : std::uint8_t { Hall, WideHall, Cellar, Garden };

constexpr int kCell = 8;

// The whole screen is the view: nothing sits above or beside the world.
constexpr CameraKit::View kView{.size = {.width = 160, .height = 144}};

// The values in a map, drawn as the tiles of the same name.
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };

// The values in the garden's canopy map: nothing, or the trees' leaves.
enum class Overhead : std::uint16_t { Nothing = 0, Canopy = 1 };

// Every area's depths: the ground, the marker over it, and the canopy over the marker.
constexpr std::int32_t kGroundZ = 0;
constexpr std::int32_t kActorsZ = 5;
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

// The art: one sheet of four 8×8 tiles (floor, wall, stairs, the marker); a sheet of two for what
// hangs over the garden (nothing, and the canopy), whose index 0 is transparent so the ground shows
// through; and three palettes, one for the halls, one for the cellar and one for the garden.
struct Art {
    retropp::AtlasId tiles{};
    retropp::AtlasId overhead{};
    retropp::PaletteId hall{};
    retropp::PaletteId cellar{};
    retropp::PaletteId garden{};
};

Art uploadArt(retropp::Renderer& renderer) {
    std::array<std::uint8_t, 32 * 8> sheet{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
            sheet[static_cast<std::size_t>(y) * 32 + x] = (x + y) % 4 == 0 ? 2 : 1;            // floor
            sheet[static_cast<std::size_t>(y) * 32 + kCell + x] = rim ? 3 : 2;                  // wall
            sheet[static_cast<std::size_t>(y) * 32 + 2 * kCell + x] = y % 2 == 0 ? 3 : 0;       // stairs
            sheet[static_cast<std::size_t>(y) * 32 + 3 * kCell + x] = rim ? 3 : 0;              // the marker
        }
    }
    // Slot 0 is every pixel index 0, so it draws nothing; slot 1 is leaves with gaps between them.
    std::array<std::uint8_t, 16 * 8> overhead{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            overhead[static_cast<std::size_t>(y) * 16 + kCell + x] = (x * 3 + y * 5) % 7 == 0 ? 0 : 1 + (x + y) % 2;
        }
    }
    const std::array<retropp::Rgba8, 4> hall{{{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    const std::array<retropp::Rgba8, 4> cellar{{{10, 10, 20}, {60, 50, 70}, {120, 100, 130}, {230, 210, 240}}};
    const std::array<retropp::Rgba8, 4> garden{{{10, 30, 10}, {40, 110, 50}, {90, 170, 80}, {220, 240, 180}}};
    return Art{.tiles = renderer.uploadAtlas(sheet.data(), 32, 8).atlasId,
               .overhead = renderer.uploadAtlas(overhead.data(), 16, 8, retropp::TransparentIndices::of({0})).atlasId,
               .hall = renderer.uploadPalette(std::span<const retropp::Rgba8>(hall)),
               .cellar = renderer.uploadPalette(std::span<const retropp::Rgba8>(cellar)),
               .garden = renderer.uploadPalette(std::span<const retropp::Rgba8>(garden))};
}

// How an area's map values are drawn: each value's tile, through one palette.
retropp::TileCatalog CatalogThrough(const Art& art, retropp::PaletteId palette) {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = art.tiles, .slot = 0, .palette = palette},
        {.id = 1, .sheet = art.tiles, .slot = 1, .palette = palette},
        {.id = 2, .sheet = art.tiles, .slot = 2, .palette = palette},
    }};
}

// How the canopy map's values are drawn: nothing through the transparent slot, leaves through the
// garden's palette.
retropp::TileCatalog OverheadArt(const Art& art) {
    return retropp::TileCatalog{.entries = {
        {.id = static_cast<std::uint16_t>(Overhead::Nothing), .sheet = art.overhead, .slot = 0, .palette = art.garden},
        {.id = static_cast<std::uint16_t>(Overhead::Canopy), .sheet = art.overhead, .slot = 1, .palette = art.garden},
    }};
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

// An area's ground: its map through one palette, at the bottom.
WorldKit::MapLayer Ground(const retropp::IndexGrid& map, const Art& art, retropp::PaletteId palette) {
    return WorldKit::MapLayer{.name = "ground", .tiles = &map, .art = CatalogThrough(art, palette), .z = kGroundZ};
}

// The one actor slot every area has: where the marker is drawn.
const std::vector<WorldKit::ActorSlot> kActorSlots{{.name = "actors", .z = kActorsZ}};

// The five maps. Every area's map is its own pixel space; the openings below say how they join. The
// wide hall's north wall and the garden's south wall are open along their whole length but the
// corners, so the two meet along the full width. The garden's canopy is the garden's size, with two
// stands of trees along its north side.
struct Maps {
    retropp::IndexGrid hall = Room(20, 18, Down(19, 7, 10), {});
    retropp::IndexGrid wideHall = Room(30, 18, Both(Down(0, 7, 10), Across(0, 1, 28)), {{20, 5}, {21, 5}});
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
    retropp::IndexGrid garden = Room(30, 12, Across(11, 1, 28), {});
    retropp::IndexGrid gardenCanopy = Canopy(30, 12, {{.x = 4, .y = 1, .width = 6, .height = 4},
                                                      {.x = 18, .y = 1, .width = 7, .height = 3}});
};

WorldKit::Layout CastleLayout(const Maps& maps, const Art& art) {
    using WorldKit::Side;
    using WorldKit::Transition;
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{.layers = {Ground(maps.hall, art, art.hall)}, .actorSlots = kActorSlots,
                           .collision = &maps.hall,
                           .openings = {{.name = "east door", .side = Side::East, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::WideHall, .opening = "west door"}}}},
            WorldKit::Area{.layers = {Ground(maps.wideHall, art, art.hall)}, .actorSlots = kActorSlots,
                           .collision = &maps.wideHall,
                           .openings = {{.name = "west door", .side = Side::West, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Hall, .opening = "east door"}},
                                        {.name = "stairs down", .at = {.x = 20, .y = 5, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::Cellar, .opening = "stairs up"}},
                                        {.name = "north edge", .side = Side::North, .span = {1, 28},
                                         .transition = Transition::Seamless,
                                         .leadsTo = {.area = Area::Garden, .opening = "south edge"}}}},
            WorldKit::Area{.layers = {Ground(maps.cellar, art, art.cellar)}, .actorSlots = kActorSlots,
                           .collision = &maps.cellar,
                           .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::WideHall, .opening = "stairs down"}}}},
            // The canopy is above the actor slot, so the marker walks under the trees.
            WorldKit::Area{.layers = {Ground(maps.garden, art, art.garden),
                                      {.name = "canopy", .tiles = &maps.gardenCanopy, .art = OverheadArt(art),
                                       .z = kCanopyZ}},
                           .actorSlots = kActorSlots,
                           .collision = &maps.garden,
                           .openings = {{.name = "south edge", .side = Side::South, .span = {1, 28},
                                         .transition = Transition::Seamless,
                                         .leadsTo = {.area = Area::WideHall, .opening = "north edge"}}}},
        },
        .cellSize = kCell,
        .open = Cell::Floor,   // stairs are walked on, so only a wall is solid
        .view = kView,
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
    const WorldKit::World world{.layouts = {CastleLayout(maps, art)}};
    WorldKit::validate(world);

    // The marker: an 8×8 box that moves a pixel per tick on each axis held, if the box there is clear.
    retropp::IntRect marker{.x = 76, .y = 68, .width = kCell, .height = kCell};
    WorldKit::Camera camera{Layout::Castle, Area::Hall};
    camera.place(world, marker);

    loop.simTick([&](const retropp::InputState& input) {
        // The stairs are not solid, but the marker's own rules are its own: WorldKit only answers.
        if (!camera.transitioning()) {
            const WorldKit::AreaSolids solids = camera.solids(world);
            const int dx = (input.isHeld(retropp::actionId(Action::Right)) ? 1 : 0) -
                           (input.isHeld(retropp::actionId(Action::Left)) ? 1 : 0);
            const int dy = (input.isHeld(retropp::actionId(Action::Down)) ? 1 : 0) -
                           (input.isHeld(retropp::actionId(Action::Up)) ? 1 : 0);
            retropp::IntRect moved = marker;
            moved.x += dx;
            if (!solids.solidBox(moved)) {
                marker = moved;
            }
            moved = marker;
            moved.y += dy;
            if (!solids.solidBox(moved)) {
                marker = moved;
            }
        }

        // Through a door, WorldKit scrolls and carries the marker; onto the stairs, it fades and jumps
        // it; across the seam into the garden, it only changes the marker's pixels to the garden's.
        const WorldKit::Step step = camera.follow(world, marker);
        marker.x += step.move.x;
        marker.y += step.move.y;
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        // The marker is in the current area's pixels; Actors scrolls it with the camera and draws it at
        // that area's "actors" slot, between the ground and the canopy.
        frame.layers = WorldKit::WorldLayers(camera, world);
        frame.layers.push_back(WorldKit::Actors(camera, world, "actors",
                                                {{retropp::Sprite{.key = "marker",
                                                                  .x = marker.x,
                                                                  .y = marker.y,
                                                                  .size = {.width = kCell, .height = kCell},
                                                                  .atlas = art.tiles,
                                                                  .tile = 3,
                                                                  .palette = art.hall}}}));
        frame.regions = WorldKit::Fade(camera, world);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
