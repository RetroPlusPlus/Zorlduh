#include "game/sim.h"

#include "retropp/geometry.h"

#include "game/player.h"

namespace zorlduh {

Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths, retropp::IntRect messageBox) {
    WindowKit::Animation messageAnimation;
    messageAnimation.duration = kMessageAnimationTime;
    return Config{
        .player = PlayerConfig(assets),
        .world = gameWorld(assets, view, depths),
        .font = assets.font,
        .borders = assets.borders,
        .messageBox = messageBox,
        .messageAnimation = messageAnimation,
    };
}

const WindowKit::Border& borderOf(const Borders& borders, Material material) noexcept {
    return material == Material::Stone ? borders.stone : borders.wood;
}

void tick(GameState& state, const Input& input, const Config& config) {
    state.events.clear();

    // A message holds the world still. The press that opens one is never also its first confirm: the
    // tick that opens it does not advance it.
    if (state.dialog.showing()) {
        state.dialog.advance(input.item);
        state.nameReveal.advance();
        return;
    }

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

    if (input.item && !state.camera.transitioning()) {
        const Sign* sign = signFaced(static_cast<Layout>(state.camera.layout()), state.camera.area(),
                                     state.player.collision(config.player).bounds(), state.player.facing());
        if (sign != nullptr) {
            const retropp::IntRect room = WindowKit::inside(config.messageBox, borderOf(config.borders, sign->material));
            state.reading = sign->material;
            state.readingStyle = sign->style;
            state.dialog.animation = config.messageAnimation;
            state.dialog.open(sign->text, {.width = room.width, .height = room.height}, config.font);
        }
    }

    state.nameReveal.advance();
}

}  // namespace zorlduh
