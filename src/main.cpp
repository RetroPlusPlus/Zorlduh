// Zorlduh's entry point. It activates the game's engine configuration, creates the four engine
// objects, loads the game's art and maps, and runs the loop until the window is closed. Each tick the
// player walks the world; each displayed frame is the world's tile layer and the actors on top of it,
// placed so the start room fills the play area beneath the HUD strip.

#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/input.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/windowed_host.h"

#include "assets/assets.h"
#include "config/config.h"
#include "game/player.h"
#include "game/rooms.h"
#include "game/sim.h"
#include "game/world.h"
#include "input/input.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/tile_layer.h"
#include "TopDownMovement/walker.h"
#include "render/layers.h"
#include "render/layout.h"
#include "render/player.h"
#include "render/world.h"

int main() {
    retropp::EngineConfig::setActive(zorlduh::engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(zorlduh::actionMap());

    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    const TopDownMovement::WalkerConfig player = zorlduh::PlayerConfig(assets);
    zorlduh::GameState state{
        .world = zorlduh::World{.tiles = assets.world, .collision = assets.collision},
        .player = TopDownMovement::Walker{zorlduh::kPlayerStart},
    };

    // The start room is exactly one play area, so the camera rests on its top-left cell.
    const zorlduh::Room& start = zorlduh::kRooms[zorlduh::kStartRoom];
    const retropp::LayerScroll worldScroll{
        .x = start.x * zorlduh::kCellSize - zorlduh::kPlayAreaX,
        .y = start.y * zorlduh::kCellSize - zorlduh::kPlayAreaY,
    };

    loop.simTick([&](const retropp::InputState& in) {
        zorlduh::tick(state, zorlduh::readInput(in), player);
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&]() {
        frame.layers = {
            LayerKit::TileLayer(zorlduh::kWorldLayer, zorlduh::kWorldZ,
                              zorlduh::WorldTiles(state.world, assets), state.world.tiles.width,
                              state.world.tiles.height, {.scroll = worldScroll}),
            LayerKit::SpriteLayer(zorlduh::kActorsLayer, zorlduh::kActorsZ,
                                {zorlduh::Player(state, player)}, {.scroll = worldScroll}),
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
