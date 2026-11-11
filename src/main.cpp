// Zorlduh's entry point. It activates the game's engine configuration, creates the four engine
// objects, loads the game's art and maps, and runs the loop until the window is closed. Each displayed
// frame is the world's tile layer, placed so the start room fills the play area beneath the HUD strip.

#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/engine_config.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sdl_platform.h"
#include "retropp/windowed_host.h"

#include "assets/assets.h"
#include "config/config.h"
#include "game/rooms.h"
#include "game/world.h"
#include "layers/tile_layer.h"
#include "render/layers.h"
#include "render/layout.h"
#include "render/world.h"

int main() {
    retropp::EngineConfig::setActive(zorlduh::engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};

    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    const zorlduh::World world{.tiles = assets.world, .collision = assets.collision};

    // The start room is exactly one play area, so the camera rests on its top-left cell.
    const zorlduh::Room& start = zorlduh::kRooms[zorlduh::kStartRoom];
    const retropp::LayerScroll worldScroll{
        .x = start.x * zorlduh::kCellSize - zorlduh::kPlayAreaX,
        .y = start.y * zorlduh::kCellSize - zorlduh::kPlayAreaY,
    };

    retropp::FrameDrawState frame;
    loop.renderLoop([&]() {
        frame.layers = {
            layers::TileLayer(zorlduh::kWorldLayer, zorlduh::kWorldZ,
                              zorlduh::WorldTiles(world, assets), world.tiles.width,
                              world.tiles.height, {.scroll = worldScroll}),
        };
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
