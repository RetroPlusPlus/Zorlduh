// Sprites — what the actors of one place show at one slot, reported as sprites for the game to draw.
//
//     const WorldKit::Place here{.layout = state.camera.layout(), .area = state.camera.area()};
//     frame.layers = WorldKit::WorldLayers(state.camera, world);
//     frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors",
//                                             {ActorKit::Sprites(state.actors, config.actors, here, "actors", state)}));
//
// The layers are the game's. It asks for the sprites of a slot and hands them to whatever draws that
// slot — WorldKit::Actors here — beside any sprites of its own. This is the only ActorKit header that
// builds a draw type.
#pragma once

#include <memory>
#include <optional>
#include <string_view>

#include "retropp/animation.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"

#include "ScriptKit/expression.h"
#include "ScriptKit/value.h"

#include "LayerKit/types.h"

#include "WorldKit/area.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/config.h"
#include "ActorKit/look.h"

namespace ActorKit {

// One sprite for each live actor in `place` whose `slot` is `slot` and that has something to show, in
// spawn order. A sprite is keyed Actors::key(id) — a key of its own among every Actors value's — with
// its top-left at the actor's position in its area's pixels. It shows the first of the actor's looks
// whose condition holds against its values, and while every look's condition is false, its mover's
// frame. Only an actor with one of the two to show is reported. Its `z` is the bottom of the actor's box
// in its area's pixels — the bottom of what it shows when its box is empty — so the actors of one slot
// sort by their feet.
//
// Throws std::out_of_range, as tick does, for an actor reading an unbound value, and std::logic_error
// when the config's bindings are written for another type than State.
template <class State>
[[nodiscard]] LayerKit::Sprites Sprites(const Actors& actors, const Config& config, WorldKit::Place place,
                                        std::string_view slot, const State& state) {
    using detail::Live;
    const Live* followed = detail::ActorsAccess::find(actors, actors.followed());
    LayerKit::Sprites sprites;
    ScriptKit::Values values;
    for (const std::unique_ptr<Live>& live : detail::ActorsAccess::live(actors)) {
        if (live->place != place || live->actor->slot != slot) {
            continue;
        }
        std::optional<Look> shown;
        if (!live->actor->look.empty()) {
            detail::gatherValues(values, *live, followed, config.bindings, state);
            for (const LookRule& rule : live->actor->look.rules()) {
                if (ScriptKit::evaluate(rule.when, values)) {
                    shown = rule.look;
                    break;
                }
            }
        }
        if (!shown && live->mover) {
            const retropp::AnimationFrame& frame = live->mover->frame();
            if (frame.hasArt()) {
                shown = Look{.size = frame.size(), .atlas = frame.atlas(), .tile = frame.tile(), .palette = frame.palette};
            }
        }
        if (!shown) {
            continue;
        }
        const retropp::IntRect box = detail::relativeBox(*live);
        const int bottom = box.width > 0 && box.height > 0 ? box.y + box.height : shown->size.height;
        sprites.push_back(retropp::Sprite{
            .key = detail::keyOf(detail::ActorsAccess::set(actors), *live),
            .x = live->position.x,
            .y = live->position.y,
            .z = live->position.y + bottom,
            .size = shown->size,
            .atlas = shown->atlas,
            .tile = shown->tile,
            .palette = shown->palette,
            .flipX = shown->flipX,
            .flipY = shown->flipY,
        });
    }
    return sprites;
}

}  // namespace ActorKit
