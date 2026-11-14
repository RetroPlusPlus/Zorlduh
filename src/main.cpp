// Zorlduh's entry point. It activates the game's engine configuration, creates the four engine
// objects, loads the game's art and maps, and runs the loop until the window is closed. The game opens
// on the title screen: the game's name and the title menu. In play, each tick the player walks the
// world; each displayed frame is the layers of the areas on screen with the actors among them at the
// current area's actor slot, and the fade over everything, placed under the HUD strip, in the strip the
// current area's name, over it the message box while a sign or a rock is read, and over it all the
// pause menu while Start holds the world still.

#include <utility>

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
#include "render/dialog.h"
#include "render/layers.h"
#include "render/layout.h"
#include "render/menus.h"
#include "render/player.h"

int main() {
    retropp::EngineConfig::setActive(zorlduh::engineConfig());

    retropp::SteadyClock clock;
    retropp::SdlPlatform platform;
    retropp::Renderer renderer{platform.device(), platform.sdlWindow()};
    retropp::RunLoop loop{clock};
    platform.actions(zorlduh::actionMap());

    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    const zorlduh::Config config =
        zorlduh::gameConfig(assets, zorlduh::kView, zorlduh::kAreaDepths, zorlduh::kDialogBox, zorlduh::kMenuBoxes);
    zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart},
                             .screen = zorlduh::Screen::Title};
    state.camera.place(config.world, state.player.collision(config.player).bounds());

    loop.simTick([&](const retropp::InputState& in) {
        zorlduh::tick(state, zorlduh::readInput(in), config);
    });

    retropp::FrameDrawState frame;
    loop.renderLoop([&]() {
        if (state.screen == zorlduh::Screen::Title) {
            frame.layers = zorlduh::TitleScreen(state, config, assets, zorlduh::HudZ(config.world),
                                                zorlduh::MenuZ(config.world));
            frame.regions.clear();
            renderer.renderFrame(frame);
            return;
        }
        frame.layers = WorldKit::WorldLayers(state.camera, config.world);
        frame.layers.push_back(WorldKit::Actors(state.camera, config.world, zorlduh::kActorSlot,
                                                {zorlduh::Player(state, config.player)}));
        frame.layers.push_back(LayerKit::SpriteLayer("hud:area-name", zorlduh::HudZ(config.world),
                                                     {zorlduh::AreaName(state, assets)}));
        frame.layers.push_back(zorlduh::Dialog(state, assets, zorlduh::DialogZ(config.world)));
        for (retropp::DrawLayer& layer : zorlduh::PauseMenu(state, config, assets, zorlduh::MenuZ(config.world))) {
            frame.layers.push_back(std::move(layer));
        }
        frame.regions = WorldKit::Fade(state.camera, config.world);
        renderer.renderFrame(frame);
    });

    retropp::WindowedHost{loop, platform}.run();
    return 0;
}
