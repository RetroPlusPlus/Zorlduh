// A complete program built on the TopDownMovement module: a walled room with four pillars, and a walker
// steered with the arrow keys, WASD or a d-pad. It walks in eight directions, keeps moving along a wall
// it walks into diagonally, faces where it walks and steps through its walk animation.
//
// docs/getting-started.md walks through it part by part.

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/palette.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/timing.h"
#include "retropp/windowed_host.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"
#include "TopDownMovement/walker_sprite.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right };

// The room: the whole Game Boy Color screen, 20×18 cells of 8 px.
constexpr int kCell = 8;
constexpr int kRoomWidth = 20;
constexpr int kRoomHeight = 18;

// The values in the room's collision map.
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

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

// The art: one tile sheet (floor, wall), one walker sheet, one palette.
struct Art {
    retropp::AtlasId tiles{};
    retropp::AtlasManifest walker;
    retropp::PaletteId palette{};
};

// The walker sheet: 8×8 frames, two per row, one row per facing in the order Up, Down, Left, Right.
// Each frame is an arrowhead pointing the way the row faces; the second frame sits one pixel further
// along, so walking reads as a bob.
Art uploadArt(retropp::Renderer& renderer) {
    Art art;

    std::array<std::uint8_t, 16 * 8> tiles{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
            tiles[static_cast<std::size_t>(y) * 16 + x] = 1;
            tiles[static_cast<std::size_t>(y) * 16 + kCell + x] = rim ? 3 : 2;
        }
    }
    art.tiles = renderer.uploadAtlas(tiles.data(), 16, 8).atlasId;

    constexpr int kSheetWidth = 16;
    constexpr int kSheetHeight = 32;
    std::array<std::uint8_t, kSheetWidth * kSheetHeight> walker{};
    for (int row = 0; row < 4; ++row) {
        for (int frame = 0; frame < 2; ++frame) {
            for (int y = 0; y < kCell; ++y) {
                for (int x = 0; x < kCell; ++x) {
                    // Distance along the facing (0 at the back, 7 at the tip) and across it; the
                    // second frame sits one pixel further along.
                    const int along = (row == 0 ? 7 - y : row == 1 ? y : row == 2 ? 7 - x : x) - frame;
                    const int across = row < 2 ? x : y;
                    const bool inside = along >= 0 && std::abs(2 * across - 7) <= 7 - along;
                    walker[static_cast<std::size_t>(row * kCell + y) * kSheetWidth +
                           static_cast<std::size_t>(frame * kCell + x)] = inside ? 3 : 0;
                }
            }
        }
    }
    art.walker = renderer.uploadAtlas(walker.data(), kSheetWidth, kSheetHeight,
                                      retropp::AssetDimensions{.width = 8, .height = 8},
                                      retropp::ContentKind::AnimationSeries,
                                      retropp::ReadOrder::LeftRightThenDown, /*count=*/0,
                                      retropp::TransparentIndices::of({0}), /*framesPerAnimation=*/2);

    const std::array<retropp::Rgba8, 4> colors{
        {{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    art.palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(colors));
    return art;
}

// One row of the walker sheet as a walk animation: two frames of eight ticks each.
retropp::Animation WalkRow(const Art& art, std::size_t row) {
    constexpr auto kFrame = 8 * std::chrono::nanoseconds{
                                    static_cast<std::chrono::nanoseconds::rep>(retropp::TickPeriodNs::Hz60)};
    return retropp::Animation{{
        {.sheet = art.walker.atlasId, .tileIndex = row * 2, .palette = art.palette, .duration = kFrame},
        {.sheet = art.walker.atlasId, .tileIndex = row * 2 + 1, .palette = art.palette, .duration = kFrame},
    }};
}

// The room's cells for its tile layer: wall where the collision map is solid, floor elsewhere.
std::vector<retropp::TileCell> RoomCells(const retropp::IndexGrid& room, const Art& art) {
    std::vector<retropp::TileCell> cells;
    cells.reserve(room.values.size());
    for (const std::uint16_t value : room.values) {
        const bool wall = value == static_cast<std::uint16_t>(Cell::Wall);
        cells.push_back(retropp::TileCell{.atlas = art.tiles,
                                          .tile = static_cast<std::uint16_t>(wall ? 1 : 0),
                                          .palette = art.palette});
    }
    return cells;
}

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
    platform.actions(
        retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));

    const Art art = uploadArt(renderer);
    const retropp::IndexGrid room = Room();
    const std::vector<retropp::TileCell> roomCells = RoomCells(room, art);

    // What the walker is: one pixel per tick, a 6×4 collision box across the bottom of its 8×8 art, and
    // a walk animation per facing.
    const TopDownMovement::WalkerConfig hero{
        .speed = 1,
        .collision = retropp::IntRect{.x = 1, .y = 4, .width = 6, .height = 4},
        .walk = {.up = WalkRow(art, 0), .down = WalkRow(art, 1),
                 .left = WalkRow(art, 2), .right = WalkRow(art, 3)},
    };
    const TopDownMovement::TileCollision walls{.map = &room, .cellSize = kCell, .open = Cell::Open};
    const TopDownMovement::DirectionalActions steering{
        .up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right};

    TopDownMovement::Walker walker{{76, 68}};
    loop.simTick([&](const retropp::InputState& input) {
        walker.walk(hero, TopDownMovement::readDirections(input, steering), walls);
    });

    retropp::FrameDrawState frame;
    std::vector<retropp::Sprite> sprites;
    loop.renderLoop([&] {
        sprites = {TopDownMovement::WalkerSprite("hero", walker, hero)};
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
