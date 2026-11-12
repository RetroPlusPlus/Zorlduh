// Zorlduh's entry point. It activates the game's engine configuration, creates the four engine
// objects, loads the game's art and maps, and runs the loop until the window is closed. Each tick the
// player walks the world; each displayed frame is the layers of the areas on screen with the actors
// among them at the current area's actor slot, and the fade over everything, placed under the HUD
// strip, and in the strip the current area's name.

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
#include "game/sim.h"
#include "input/input.h"
#include "LayerKit/sprite_layer.h"
#include "TopDownMovement/walker.h"
#include "WorldKit/world_layers.h"
#include "render/area_name.h"
#include "render/layers.h"
#include "render/layout.h"
#include "render/player.h"

int main() {
    retropp::EngineConfig::setActive(zorlduh::engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(zorlduh::actionMap());

    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    const zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView, zorlduh::kAreaDepths);
    zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());

    loop.simTick([&](const retropp::InputState& in) {
        zorlduh::tick(state, zorlduh::readInput(in), config);
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&]() {
        frame.layers = WorldKit::WorldLayers(state.camera, config.world);
        frame.layers.push_back(WorldKit::Actors(state.camera, config.world, zorlduh::kActorSlot,
                                                {zorlduh::Player(state, config.player)}));
        frame.layers.push_back(LayerKit::SpriteLayer("hud:area-name", zorlduh::HudZ(config.world),
                                                     {zorlduh::AreaName(state, assets)}));
        frame.regions = WorldKit::Fade(state.camera, config.world);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
