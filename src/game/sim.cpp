#include "game/sim.h"

#include "retropp/geometry.h"

#include "game/player.h"

namespace zorlduh {

Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths) {
    return Config{.player = PlayerConfig(assets), .world = gameWorld(assets, view, depths)};
}

void tick(GameState& state, const Input& input, const Config& config) {
    state.events.clear();

    if (!state.camera.transitioning()) {
        state.player.walk(config.player, input.move, state.camera.solids(config.world));
    }

    const WorldKit::Step step = state.camera.follow(config.world, state.player.collision(config.player).bounds());
    if (step.move != retropp::Vec2i{}) {
        const retropp::Vec2i at = state.player.position();
        state.player.place({.x = at.x + step.move.x, .y = at.y + step.move.y});
    }
    if (step.entered) {
        state.events.push_back(AreaChanged{.layout = static_cast<Layout>(step.entered->layout), .area = step.entered->area});
        state.nameReveal = TextKit::Reveal{};
    }
    state.nameReveal.advance();
}

}  // namespace zorlduh
