// A complete program built on the layer kit: a walled floor of tiles, a player sprite steered with the
// arrow keys or a d-pad, a full-color badge in the corner, and a meter painted on the CPU that shows how
// far across the floor the player is. Each frame is the list of layers Scene() returns.
//
// docs/getting-started.md walks through it line by line.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>

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
#include "retropp/windowed_host.h"

#include "layers/image_layer.h"
#include "layers/raster_layer.h"
#include "layers/sprite_layer.h"
#include "layers/tile_layer.h"
#include "layers/types.h"

namespace {

enum class Action : std::uint8_t { Up, Down, Left, Right };

// Every depth in the program, back to front.
constexpr std::int32_t kFloorZ = 0;
constexpr std::int32_t kActorsZ = 10;
constexpr std::int32_t kBadgeZ = 20;
constexpr std::int32_t kMeterZ = 30;

// The floor is the whole Game Boy Color screen: 20×18 cells of 8 px, walls round the edge.
constexpr int kCell = 8;
constexpr int kFloorWidth = 20;
constexpr int kFloorHeight = 18;

// The meter is painted at 64×4 and shown at 64×8.
constexpr int kMeterWidth = 64;
constexpr int kMeterHeight = 4;

// The art the components draw with: the handles the uploads returned, made once at startup.
struct Art {
    retropp::AtlasId tiles{};
    retropp::AtlasId sprites{};
    retropp::PaletteId palette{};
    retropp::ImageId badge{};
};

// The game's state. The tick changes it; the components only read it.
struct State {
    int playerX = 76;
    int playerY = 68;
    std::uint64_t meterGeneration = 1;  // a new number for each new meter picture
};

// ── Components: each takes the state it shows and returns what it is ─────────────────────────

// The floor's cells: wall round the edge, floor inside.
layers::Cells Floor(const Art& art) {
    layers::Cells cells;
    cells.reserve(static_cast<std::size_t>(kFloorWidth) * kFloorHeight);
    for (int y = 0; y < kFloorHeight; ++y) {
        for (int x = 0; x < kFloorWidth; ++x) {
            const bool wall = x == 0 || y == 0 || x == kFloorWidth - 1 || y == kFloorHeight - 1;
            cells.push_back(retropp::TileCell{.atlas = art.tiles,
                                              .tile = static_cast<std::uint16_t>(wall ? 1 : 0),
                                              .palette = art.palette});
        }
    }
    return cells;
}

// The player: one 8×8 sprite, keyed by what it is.
layers::Sprites Player(const State& state, const Art& art) {
    return {retropp::Sprite{.key = "player",
                            .x = state.playerX,
                            .y = state.playerY,
                            .atlas = art.sprites,
                            .tile = 0,
                            .palette = art.palette}};
}

// The meter: a bar filled as far as the player is across the floor, painted pixel by pixel.
layers::Raster Meter(const State& state) {
    layers::Raster meter{.width = kMeterWidth,
                         .height = kMeterHeight,
                         .generation = state.meterGeneration};
    meter.pixels.resize(static_cast<std::size_t>(kMeterWidth) * kMeterHeight * 4);
    const int filled = state.playerX * kMeterWidth / (kFloorWidth * kCell);
    for (int y = 0; y < kMeterHeight; ++y) {
        for (int x = 0; x < kMeterWidth; ++x) {
            const std::array<std::uint8_t, 4> rgba =
                x < filled ? std::array<std::uint8_t, 4>{255, 200, 40, 255}
                           : std::array<std::uint8_t, 4>{40, 40, 60, 255};
            const std::size_t at = (static_cast<std::size_t>(y) * kMeterWidth + x) * 4;
            for (std::size_t c = 0; c < 4; ++c) {
                meter.pixels[at + c] = rgba[c];
            }
        }
    }
    return meter;
}

// The screen: its layers, declared as a list.
layers::Layers Scene(const State& state, const Art& art) {
    return {
        layers::TileLayer("floor", kFloorZ, Floor(art), kFloorWidth, kFloorHeight),
        layers::SpriteLayer("actors", kActorsZ, {Player(state, art)}),
        layers::ImageLayer("badge", kBadgeZ, art.badge, {.scroll = {-136, -8}}),
        layers::RasterLayer("meter", kMeterZ, Meter(state),
                            {.scroll = {-8, -10}, .fit = {.width = kMeterWidth, .height = 8}}),
    };
}

// ── Logic: no draw types ─────────────────────────────────────────────────────────────────────

// One tick: move the player inside the walls, and give the meter a new generation when it changes.
void step(State& state, const retropp::InputState& input) {
    const int dx = (input.isHeld(Action::Right) ? 1 : 0) - (input.isHeld(Action::Left) ? 1 : 0);
    const int dy = (input.isHeld(Action::Down) ? 1 : 0) - (input.isHeld(Action::Up) ? 1 : 0);
    const int minX = kCell;
    const int maxX = (kFloorWidth - 2) * kCell;
    const int minY = kCell;
    const int maxY = (kFloorHeight - 2) * kCell;

    const int x = std::clamp(state.playerX + dx, minX, maxX);
    if (x != state.playerX) {
        ++state.meterGeneration;
    }
    state.playerX = x;
    state.playerY = std::clamp(state.playerY + dy, minY, maxY);
}

// ── Art, uploaded once ───────────────────────────────────────────────────────────────────────

Art uploadArt(retropp::Renderer& renderer) {
    Art art;

    // Two 8×8 tiles side by side: floor (index 1 with an index-2 fleck), wall (index 2, index-3 rim).
    std::array<std::uint8_t, 16 * 8> tiles{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const bool rim = x == 0 || y == 0 || x == kCell - 1 || y == kCell - 1;
            tiles[static_cast<std::size_t>(y) * 16 + x] = (x == 3 && y == 3) ? 2 : 1;
            tiles[static_cast<std::size_t>(y) * 16 + kCell + x] = rim ? 3 : 2;
        }
    }
    art.tiles = renderer.uploadAtlas(tiles.data(), 16, 8).atlasId;

    // One 8×8 sprite: a diamond of index 3 on index 0, which is a hole.
    std::array<std::uint8_t, 8 * 8> sprite{};
    for (int y = 0; y < kCell; ++y) {
        for (int x = 0; x < kCell; ++x) {
            const int distance = std::abs(2 * x - 7) + std::abs(2 * y - 7);
            sprite[static_cast<std::size_t>(y) * kCell + x] = distance <= 8 ? 3 : 0;
        }
    }
    art.sprites =
        renderer.uploadAtlas(sprite.data(), 8, 8, retropp::TransparentIndices::of({0})).atlasId;

    const std::array<retropp::Rgba8, 4> colors{
        {{20, 24, 40}, {60, 90, 70}, {110, 140, 120}, {240, 230, 200}}};
    art.palette = renderer.uploadPalette(std::span<const retropp::Rgba8>(colors));

    // A 16×16 full-color badge: a red-to-blue gradient with a transparent corner cut.
    std::array<retropp::Rgba8, 16 * 16> badge{};
    for (int y = 0; y < 16; ++y) {
        for (int x = 0; x < 16; ++x) {
            badge[static_cast<std::size_t>(y) * 16 + x] = retropp::Rgba8{
                .r = static_cast<std::uint8_t>(255 - x * 15),
                .g = 80,
                .b = static_cast<std::uint8_t>(x * 15),
                .a = static_cast<std::uint8_t>(x + y < 4 ? 0 : 255)};
        }
    }
    art.badge = renderer.uploadImage(std::span<const retropp::Rgba8>(badge), 16, 16);

    return art;
}

}  // namespace

int main() {
    retropp::EngineConfig::setActive(retropp::EngineConfig{
        .identity = {.organization = "Retro++", .application = "Layer Kit Getting Started"},
        .window = {.title = "layers — getting started"},
        .enhancements = {.windowScale = 4},
    });

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(
        retropp::presets::directional(Action::Up, Action::Down, Action::Left, Action::Right));

    const Art art = uploadArt(renderer);
    State state;

    loop.simTick([&](const retropp::InputState& input) { step(state, input); });

    retropp::FrameDrawState frame;
    loop.renderLoop([&] {
        frame.layers = Scene(state, art);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
