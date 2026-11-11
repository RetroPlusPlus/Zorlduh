// A complete program built on the WorldKit module: three areas joined by their openings — a hall with
// a door in its east wall that scrolls into a wider hall, and stairs in that hall that fade down to a
// small cellar with stairs back up — and a marker steered with the arrow keys, WASD or a d-pad. The
// marker's own movement is a few lines here; WorldKit tells it what is solid, carries it through each
// opening, and draws whatever is on screen.
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
#include "LayerKit/sprite_layer.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/solids.h"
#include "WorldKit/world_layers.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right };

// The program's areas, by index in the layout.
enum Area : std::uint8_t { Hall, WideHall, Cellar };

constexpr int kCell = 8;

// The whole screen is the view: nothing sits above or beside the world.
constexpr CameraKit::View kView{.size = {.width = 160, .height = 144}};

// The values in a map, drawn as the tiles of the same name.
enum class Cell : std::uint16_t { Floor = 0, Wall = 1, Stairs = 2 };

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

// The art: one sheet of four 8×8 tiles (floor, wall, stairs, the marker) and two palettes, one for
// the halls and one for the cellar.
struct Art {
    retropp::AtlasId tiles{};
    retropp::PaletteId hall{};
    retropp::PaletteId cellar{};
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
    const std::array<retropp::Rgba8, 4> hall{{{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    const std::array<retropp::Rgba8, 4> cellar{{{10, 10, 20}, {60, 50, 70}, {120, 100, 130}, {230, 210, 240}}};
    return Art{.tiles = renderer.uploadAtlas(sheet.data(), 32, 8).atlasId,
               .hall = renderer.uploadPalette(std::span<const retropp::Rgba8>(hall)),
               .cellar = renderer.uploadPalette(std::span<const retropp::Rgba8>(cellar))};
}

// How an area's map values are drawn: each value's tile, through one palette.
retropp::TileCatalog CatalogThrough(const Art& art, retropp::PaletteId palette) {
    return retropp::TileCatalog{.entries = {
        {.id = 0, .sheet = art.tiles, .slot = 0, .palette = palette},
        {.id = 1, .sheet = art.tiles, .slot = 1, .palette = palette},
        {.id = 2, .sheet = art.tiles, .slot = 2, .palette = palette},
    }};
}

// The three maps. Every map is its own pixel space; the openings below say how they join.
struct Maps {
    retropp::IndexGrid hall = Room(20, 18, {{19, 7}, {19, 8}, {19, 9}, {19, 10}}, {});
    retropp::IndexGrid wideHall = Room(30, 18, {{0, 7}, {0, 8}, {0, 9}, {0, 10}}, {{20, 5}, {21, 5}});
    retropp::IndexGrid cellar = Room(12, 10, {}, {{5, 4}, {6, 4}});
};

WorldKit::Layout World(const Maps& maps, const Art& art) {
    using WorldKit::Side;
    using WorldKit::Transition;
    return WorldKit::Layout{
        .areas = {
            WorldKit::Area{.tiles = &maps.hall, .art = CatalogThrough(art, art.hall), .collision = &maps.hall,
                           .openings = {{.name = "east door", .side = Side::East, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::WideHall, .opening = "west door"}}}},
            WorldKit::Area{.tiles = &maps.wideHall, .art = CatalogThrough(art, art.hall), .collision = &maps.wideHall,
                           .openings = {{.name = "west door", .side = Side::West, .span = {7, 10},
                                         .transition = Transition::Scroll,
                                         .leadsTo = {.area = Area::Hall, .opening = "east door"}},
                                        {.name = "stairs down", .at = {.x = 20, .y = 5, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::Cellar, .opening = "stairs up"}}}},
            WorldKit::Area{.tiles = &maps.cellar, .art = CatalogThrough(art, art.cellar), .collision = &maps.cellar,
                           .openings = {{.name = "stairs up", .at = {.x = 5, .y = 4, .width = 2, .height = 1},
                                         .transition = Transition::Fade,
                                         .leadsTo = {.area = Area::WideHall, .opening = "stairs down"}}}},
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
    const WorldKit::Layout layout = World(maps, art);
    WorldKit::validate(layout);

    // The marker: an 8×8 box that moves a pixel per tick on each axis held, if the box there is clear.
    retropp::IntRect marker{.x = 76, .y = 68, .width = kCell, .height = kCell};
    WorldKit::Camera camera{Area::Hall};
    camera.place(layout, marker);

    loop.simTick([&](const retropp::InputState& input) {
        // The stairs are not solid, but the marker's own rules are its own: WorldKit only answers.
        if (!camera.transitioning()) {
            const WorldKit::AreaSolids solids = camera.solids(layout);
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

        // Through a door, WorldKit scrolls and carries the marker; onto the stairs, it fades and jumps it.
        const WorldKit::Step step = camera.follow(layout, marker);
        marker.x += step.move.x;
        marker.y += step.move.y;
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        frame.layers = WorldKit::WorldLayers(camera, layout, 0);
        frame.layers.push_back(LayerKit::SpriteLayer(
            "marker", 10,
            {{retropp::Sprite{.key = "marker",
                              .x = marker.x,
                              .y = marker.y,
                              .size = {.width = kCell, .height = kCell},
                              .atlas = art.tiles,
                              .tile = 3,
                              .palette = art.hall}}},
            {.scroll = camera.scroll(layout)}));
        frame.regions = WorldKit::Fade(camera, layout);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
