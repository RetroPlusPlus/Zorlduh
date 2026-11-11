#include "game/sim.h"

#include "retropp/geometry.h"

#include "game/player.h"

namespace zorlduh {

Config gameConfig(const Assets& assets, CameraKit::View view) {
    return Config{.player = PlayerConfig(assets), .layout = worldLayout(assets, view)};
}

void tick(GameState& state, const Input& input, const Config& config) {
    state.events.clear();

    if (!state.camera.transitioning()) {
        state.player.walk(config.player, input.move, state.camera.solids(config.layout));
    }

    const WorldKit::Step step = state.camera.follow(config.layout, state.player.collision(config.player).bounds());
    if (step.move != retropp::Vec2i{}) {
        const retropp::Vec2i at = state.player.position();
        state.player.place({.x = at.x + step.move.x, .y = at.y + step.move.y});
    }
    if (step.entered) {
        state.events.push_back(AreaChanged{.area = static_cast<Area>(*step.entered)});
    }
}

}  // namespace zorlduh
