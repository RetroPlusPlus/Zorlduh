// A complete program built on the TopDownMovement module: a walled room with four pillars and three
// walkers, one for each way a walker is moved.
//
//   - The hero is steered with the arrow keys, WASD or a d-pad (an InputWalker).
//   - The wanderer walks its heading until something stops it, then turns clockwise (a Walker given a
//     heading by this program's own rule).
//   - The guard patrols a route along the bottom of the room, pausing at its east end (a Walker on the
//     engine's SpritePath).
//
// Z or a pad's A button switches every walker between 8-way and 4-way movement; X or B between 4 and 8
// sprite directions. The two cells in the room's top-left corner say which are on: the blue one is the
// movement, the orange one the sprite directions, each a plus for four and a star for eight.
//
// docs/getting-started.md walks through it part by part.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/path_walker.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/sprite_path.h"
#include "retropp/timing.h"
#include "retropp/windowed_host.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/input_walker.h"
#include "TopDownMovement/route.h"
#include "TopDownMovement/walker.h"
#include "TopDownMovement/walker_sprite.h"

namespace {

using namespace std::chrono_literals;

enum class Action : std::uint8_t { Up, Down, Left, Right, Movement, Sprites };

// The room: the whole Game Boy Color screen, 20×18 cells of 8 px.
constexpr int kCell = 8;
constexpr int kRoomWidth = 20;
constexpr int kRoomHeight = 18;

// The values in the room's collision map.
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

// The cells of the tile sheet, in its order.
enum class Tile : std::size_t { Floor, Wall, Four, Eight };

// The room's collision map: wall round the edge and four 2×2 pillars.
retropp::IndexGrid Room() {
    retropp::IndexGrid room{.width = kRoomWidth, .height = kRoomHeight};
    room.values.resize(static_cast<std::size_t>(kRoomWidth) * kRoomHeight);
    for (int y = 0; y < kRoomHeight; ++y) {
        for (int x = 0; x < kRoomWidth; ++x) {
            const bool edge = x == 0 || y == 0 || x == kRoomWidth - 1 || y == kRoomHeight - 1;
            const bool pillar = (x == 5 || x == 6 || x == 13 || x == 14) &&
                                (y == 5 || y == 6 || y == 11 || y == 12);
            room.values[static_cast<std::size_t>(y) * kRoomWidth + x] =
                static_cast<std::uint16_t>(edge || pillar ? Cell::Wall : Cell::Open);
        }
    }
    return room;
}

// The bytes of a file under this example's assets/ directory.
std::vector<std::uint8_t> readAsset(const std::string& name) {
    std::ifstream in(std::string(TOPDOWNMOVEMENT_EXAMPLE_ASSETS) + "/" + name, std::ios::binary);
    if (!in) {
        throw std::runtime_error("cannot open assets/" + name);
    }
    return {std::istreambuf_iterator<char>(in), {}};
}

// A palette image from this example's assets/ directory, uploaded.
retropp::PaletteId loadPalette(retropp::Renderer& renderer, const std::string& name) {
    const std::vector<retropp::Rgba16> colors =
        retropp::slicePaletteImage(retropp::loadPngFromMemory(readAsset(name)));
    return renderer.uploadPalette(std::span<const retropp::Rgba16>(colors));
}

// The art: the tile sheet (floor, wall, the plus and the star), the walker sheet, and a palette for the
// room, for each of the two indicators and for each walker.
struct Art {
    retropp::AtlasManifest tiles;
    retropp::AtlasManifest walker;
    retropp::PaletteId room{};
    retropp::PaletteId movement{};
    retropp::PaletteId sprites{};
    retropp::PaletteId hero{};
    retropp::PaletteId wanderer{};
    retropp::PaletteId guard{};
};

Art loadArt(retropp::Renderer& renderer) {
    return Art{
        .tiles = renderer.loadAtlasFromMemory(readAsset("tiles.png"), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                              retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                              /*count=*/4),
        // Eight rows of 8×8 frames, two to a row, one row per sprite direction in the order of
        // TopDownMovement::Facing: Up, UpRight, Right, DownRight, Down, DownLeft, Left, UpLeft. Index 0 is
        // transparent.
        .walker = renderer.loadAtlasFromMemory(readAsset("walker.png"), retropp::AssetDimensions{.width = kCell, .height = kCell},
                                               retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown,
                                               /*count=*/0, retropp::TransparentIndices::of({0}),
                                               /*framesPerAnimation=*/2),
        .room = loadPalette(renderer, "palettes/room.png"),
        .movement = loadPalette(renderer, "palettes/movement.png"),
        .sprites = loadPalette(renderer, "palettes/sprites.png"),
        .hero = loadPalette(renderer, "palettes/hero.png"),
        .wanderer = loadPalette(renderer, "palettes/wanderer.png"),
        .guard = loadPalette(renderer, "palettes/guard.png"),
    };
}

// The walker sheet's row for `facing` as a walk animation in `palette`: two frames of eight ticks each.
retropp::Animation WalkRow(const Art& art, TopDownMovement::Facing facing, retropp::PaletteId palette) {
    const std::size_t first = static_cast<std::size_t>(facing) * 2;
    return retropp::Animation{{
        {.sheet = art.walker.atlasId, .tileIndex = first, .palette = palette, .duration = 8 * 1s / 60},
        {.sheet = art.walker.atlasId, .tileIndex = first + 1, .palette = palette, .duration = 8 * 1s / 60},
    }};
}

// Every row of the walker sheet, in `palette`.
TopDownMovement::DirectionalAnimations WalkAnimations(const Art& art, retropp::PaletteId palette) {
    using TopDownMovement::Facing;
    return {
        .up = WalkRow(art, Facing::Up, palette),
        .down = WalkRow(art, Facing::Down, palette),
        .left = WalkRow(art, Facing::Left, palette),
        .right = WalkRow(art, Facing::Right, palette),
        .upLeft = WalkRow(art, Facing::UpLeft, palette),
        .upRight = WalkRow(art, Facing::UpRight, palette),
        .downLeft = WalkRow(art, Facing::DownLeft, palette),
        .downRight = WalkRow(art, Facing::DownRight, palette),
    };
}

// A walker of 8×8 art, one pixel a tick, colliding with a 6×4 box across the bottom of its art.
TopDownMovement::WalkerConfig WalkerOf(const Art& art, retropp::PaletteId palette) {
    return TopDownMovement::WalkerConfig{
        .speed = 1,
        .collision = retropp::IntRect{.x = 1, .y = 4, .width = 6, .height = 4},
        .walk = WalkAnimations(art, palette),
    };
}

// The room's cells for its tile layer: wall where the collision map is solid, floor elsewhere; and in
// the top-left corner the two indicators, a plus for Ways::Four and a star for Ways::Eight.
std::vector<retropp::TileCell> RoomCells(const retropp::IndexGrid& room, const Art& art, TopDownMovement::Ways movement,
                                         TopDownMovement::Ways sprites) {
    const auto cell = [&](Tile tile, retropp::PaletteId palette) {
        return retropp::TileCell{.atlas = art.tiles.atlasId,
                                 .tile = art.tiles[static_cast<std::size_t>(tile)].tile,
                                 .palette = palette};
    };
    const auto ways = [](TopDownMovement::Ways w) { return w == TopDownMovement::Ways::Four ? Tile::Four : Tile::Eight; };

    std::vector<retropp::TileCell> cells;
    cells.reserve(room.values.size());
    for (const std::uint16_t value : room.values) {
        cells.push_back(cell(value == static_cast<std::uint16_t>(Cell::Wall) ? Tile::Wall : Tile::Floor, art.room));
    }
    cells[0] = cell(ways(movement), art.movement);
    cells[1] = cell(ways(sprites), art.sprites);
    return cells;
}

// The wanderer's headings, clockwise from right: it turns to the next when it is blocked.
const std::array<TopDownMovement::Directions, 8> kClockwise{{
    {.right = true}, {.down = true, .right = true}, {.down = true}, {.down = true, .left = true},
    {.left = true}, {.up = true, .left = true}, {.up = true}, {.up = true, .right = true},
}};

}  // namespace

int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "TopDownMovement Getting Started"},
        .window = {.title = "TopDownMovement — getting started"},
        .timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    retropp::ActionMap actions{
        {Action::Movement, {SDL_SCANCODE_Z, retropp::PadButton::FaceLabelA}},
        {Action::Sprites, {SDL_SCANCODE_X, retropp::PadButton::FaceLabelB}},
    };
    actions.add(retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));
    platform.actions(actions);

    const Art art = loadArt(renderer);
    const retropp::IndexGrid room = Room();
    const TopDownMovement::TileCollision walls{.map = &room, .cellSize = kCell, .open = Cell::Open};
    const TopDownMovement::DirectionalActions steering{
        .up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right};

    // What each walker is: the same shape and speed, its own colors. The two switches start at their
    // defaults, 8-way movement with 4 sprite directions.
    TopDownMovement::WalkerConfig heroConfig = WalkerOf(art, art.hero);
    TopDownMovement::WalkerConfig wandererConfig = WalkerOf(art, art.wanderer);
    TopDownMovement::WalkerConfig guardConfig = WalkerOf(art, art.guard);

    // Where each walker stands: positions this program keeps, each walker bound to its own.
    retropp::Vec2i heroAt{76, 68};
    retropp::Vec2i wandererAt{76, 24};
    retropp::Vec2i guardAt{16, 120};

    TopDownMovement::InputWalker hero{heroAt};
    TopDownMovement::Walker wanderer{wandererAt};
    TopDownMovement::Walker guard{guardAt};

    // The guard's patrol, along row 15 and back, half a pixel a tick, resting half a second at the east
    // end. The route is the engine's SpritePath, departing from where the guard stands.
    retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
        {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
        {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
        {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    });

    std::size_t heading = 0;   // the wanderer's, in kClockwise
    loop.simTick([&](const retropp::InputState& input) {
        // The two switches, on every walker at once.
        if (input.justPressed(Action::Movement)) {
            const TopDownMovement::Ways movement = heroConfig.movement == TopDownMovement::Ways::Eight
                                                       ? TopDownMovement::Ways::Four
                                                       : TopDownMovement::Ways::Eight;
            heroConfig.movement = wandererConfig.movement = guardConfig.movement = movement;
        }
        if (input.justPressed(Action::Sprites)) {
            const TopDownMovement::Ways sprites = heroConfig.sprites == TopDownMovement::Ways::Four
                                                      ? TopDownMovement::Ways::Eight
                                                      : TopDownMovement::Ways::Four;
            heroConfig.sprites = wandererConfig.sprites = guardConfig.sprites = sprites;
        }

        // The input pathway: the pad.
        hero.walk(heroConfig, TopDownMovement::readDirections(input, steering), walls);

        // The free pathway: a heading from this program's own rule.
        wanderer.walk(wandererConfig, kClockwise[heading], walls);
        if (!wanderer.moving()) {
            heading = (heading + 1) % kClockwise.size();
        }

        // The route pathway: one tick along the patrol.
        TopDownMovement::follow(guard, guardConfig, patrol);
    });

    retropp::FrameDrawState frame;
    std::vector<retropp::TileCell> roomCells;
    std::vector<retropp::Sprite> sprites;
    loop.renderLoop([&] {
        roomCells = RoomCells(room, art, heroConfig.movement, heroConfig.sprites);
        sprites = {
            TopDownMovement::WalkerSprite("hero", hero, heroConfig),
            TopDownMovement::WalkerSprite("wanderer", wanderer, wandererConfig),
            TopDownMovement::WalkerSprite("guard", guard, guardConfig),
        };
        frame.layers = {
            retropp::DrawLayer{.key = "room",
                               .z = 0,
                               .size = retropp::Renderer::defaultViewport.size(),
                               .content = retropp::TileContent{.widthInTiles = kRoomWidth,
                                                               .heightInTiles = kRoomHeight,
                                                               .cells = roomCells,
                                                               .wrap = retropp::TileWrap::Blank}},
            retropp::DrawLayer{.key = "walkers",
                               .z = 10,
                               .size = retropp::Renderer::defaultViewport.size(),
                               .content = retropp::SpriteContent{.sprites = sprites}},
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
