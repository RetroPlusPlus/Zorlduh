// tick — one call a tick that runs every awake actor: their scripts' requests, their walking, their
// actions, the triggers that happen to them and their scripts. A frozen actor holds as it is.
//
//     loop.simTick([&](const retropp::InputState& in) {
//         ActorKit::tick(state.actors, config.actors, state.camera, readInput(in), state);
//     });
//
// The game reads its input once, into its own plain Input value, and hands it over with its state. The
// camera is read and left as it is: which area it is in, and whether a scroll or a fade is running.
// Moving the camera is the game's own call to WorldKit, made with what Actors reports.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

#include "retropp/geometry.h"
#include "retropp/input_actions.h"

#include "ScriptKit/command.h"
#include "ScriptKit/expression.h"
#include "ScriptKit/script.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"
#include "WorldKit/solids.h"

#include "ActorKit/actor.h"
#include "ActorKit/actors.h"
#include "ActorKit/commands.h"
#include "ActorKit/config.h"
#include "ActorKit/mover.h"
#include "ActorKit/triggers.h"

namespace ActorKit {

namespace detail {

// The square of the distance between two boxes: 0 when they overlap or meet.
[[nodiscard]] inline long long gapSquared(retropp::IntRect a, retropp::IntRect b) noexcept {
    const long long dx = std::max({0, a.x - (b.x + b.width), b.x - (a.x + a.width)});
    const long long dy = std::max({0, a.y - (b.y + b.height), b.y - (a.y + a.height)});
    return dx * dx + dy * dy;
}

[[nodiscard]] inline bool contains(retropp::IntRect box, retropp::Vec2i point) noexcept {
    return point.x >= box.x && point.x < box.x + box.width && point.y >= box.y && point.y < box.y + box.height;
}

[[nodiscard]] inline bool listed(const TriggerKinds& kinds, std::uint32_t kind) noexcept {
    for (const TriggerKind& candidate : kinds) {
        if (candidate.kind == kind) {
            return true;
        }
    }
    return false;
}

// Starts the script at `index` of `live`'s list when its `from` is Any or `side`, its `when` holds against
// `values`, and it is off.
inline void startScript(Live& live, std::size_t index, int side, const ScriptKit::Values& values) {
    const ScriptKit::Script& script = *live.actor->scripts[index].get();
    if (script.from != Any && script.from != side) {
        return;
    }
    if (live.processor.running(script) || !ScriptKit::evaluate(script.when, values)) {
        return;
    }
    live.processor.start(script);
}

// Starts every script of `live` whose trigger is the one of ScriptKit kind `kind`, as startScript does.
inline void startScripts(Live& live, std::uint32_t kind, int side, const ScriptKit::Values& values) {
    for (std::size_t n = 0; n < live.actor->scripts.size(); ++n) {
        if (live.actor->scripts[n].get()->on.kind == kind) {
            startScript(live, n, side, values);
        }
    }
}

// Whether a detection of kind `kind` is what the trigger of ScriptKit kind `trigger` is bound to.
[[nodiscard]] inline bool boundTo(const TriggerMap& triggers, std::uint32_t trigger, DetectionKind kind) noexcept {
    const std::optional<Detection> detection = triggers.detectionOf(trigger);
    return detection.has_value() && detection->kind == kind;
}

// Appends to `strips` the rectangle beyond each edge opening of `area` that leads to a layout other than
// `layout`: as wide as the opening's span, reaching out from the wall, in the area's pixels.
inline void otherLayoutStrips(std::vector<retropp::IntRect>& strips, const WorldKit::Area& area, int layout,
                              int cellSize) {
    constexpr int kFar = 1 << 24;   // farther out than any walk goes
    const int width = area.collision->width * cellSize;
    const int height = area.collision->height * cellSize;
    for (const WorldKit::Opening& opening : area.openings) {
        if (opening.side == WorldKit::Side::None || opening.leadsTo.area == WorldKit::kNoArea) {
            continue;
        }
        const int leadsTo = opening.leadsTo.layout == WorldKit::kSameLayout ? layout : opening.leadsTo.layout;
        if (leadsTo == layout) {
            continue;
        }
        const int first = opening.span.first * cellSize;
        const int length = (opening.span.last - opening.span.first + 1) * cellSize;
        switch (opening.side) {
            case WorldKit::Side::East:
                strips.push_back({.x = width, .y = first, .width = kFar, .height = length});
                break;
            case WorldKit::Side::West:
                strips.push_back({.x = -kFar, .y = first, .width = kFar, .height = length});
                break;
            case WorldKit::Side::North:
                strips.push_back({.x = first, .y = -kFar, .width = length, .height = kFar});
                break;
            case WorldKit::Side::South:
                strips.push_back({.x = first, .y = height, .width = length, .height = kFar});
                break;
            case WorldKit::Side::None:
                break;
        }
    }
}

}  // namespace detail

// One tick of the live actors. Every actor spawned before the tick is present; a frozen one
// (Actors::freeze) holds as it is, and the rest are awake and take part. In this order:
//
//   1. Requests. For each actor awake as the tick begins, in spawn order, each thing its scripts asked
//      for on its last tick awake, in the order asked: a `set` of an own value is written to the actor; a
//      `set` of one of ActorKit's six is dropped; a `set` of one of the game's goes through
//      `config.bindings`; ActorKit::spawn and ActorKit::remove are done; any other command goes to
//      `config.handlers` with the actor's id. Removals take effect once every request is written. A
//      frozen actor's requests stay asked until its first tick awake.
//   2. Solidity. Each awake actor's `solid` is worked out against its values. A frozen actor keeps the
//      solidity it last had; one whose `solid` is yet to be worked out has it worked out here, once.
//   3. Walking, while `camera.transitioning()` is false: each awake actor with a mover, in spawn order,
//      is walked by its drive against its area's collision map, the boxes of the other solid actors in
//      its area, frozen ones included, and what its confinement holds it inside: for every actor but
//      the player, everything outside its area's own rectangle under Confinement::Area, and the strip
//      beyond each edge opening that leads to another layout under Confinement::Layout. Under
//      Confinement::None, and for the player, the map and the actors are all that stop it.
//   4. Actions. Each action an awake actor's rows drive this tick, and each asked for with Actors::act,
//      is performed on the awake actor it faces: that actor's scripts on ActorKit::Interact start when
//      the action is `config.interactAction`, and its scripts on any of the game's triggers bound to the
//      action. A frozen actor's actions asked for with act wait for its first tick awake.
//   5. Triggers. Every detection the trigger map binds is worked out for each awake actor, with every
//      present actor as a setter, and the scripts it sets off start. The camera's area is the one
//      `camera` is in when tick is called. A frozen actor's Touched and what it remembers of each
//      trigger hold.
//   6. Scripts. Each awake actor with a running script is handed its values, and its processor steps
//      once.
//
// An actor spawned during a tick, or since the last, takes part from the next. A script started in step
// 5 asks for its first things in step 6, and they are written in step 1 of the next tick. A freeze or a
// wake from a handler holds from step 2 of the same tick. `camera` is read only: the camera and each
// actor's area are the game's to change.
//
// Throws std::invalid_argument when `config.world` is null; std::out_of_range naming the actor for an
// awake actor in a place outside the world or for an actor reading an unbound value — one outside its
// own, ActorKit's six and the game's bindings — and naming the command for one missing from the
// handlers; std::logic_error when a mover's or an action's drive is a member of another type than Input,
// or the config's bindings or handlers are written for another type than State. Whatever a binding, a
// handler or ScriptKit throws passes through.
template <class State, class Input>
void tick(Actors& actors, const Config& config, const WorldKit::Camera& camera, const Input& input, State& state) {
    using detail::ActorsAccess;
    using detail::Live;
    if (config.world == nullptr) {
        throw std::invalid_argument("ActorKit::tick: the config names no world");
    }
    const WorldKit::World& world = *config.world;
    std::vector<std::unique_ptr<Live>>& everyone = ActorsAccess::live(actors);
    ScriptKit::Values& values = ActorsAccess::scratch(actors);

    ActorsAccess::begin(actors);

    // 1. Requests, of the actors awake as the tick begins.
    std::vector<Live*> askers;
    askers.reserve(everyone.size());
    for (const std::unique_ptr<Live>& live : everyone) {
        if (!live->frozen) {
            askers.push_back(live.get());
        }
    }
    ActorsAccess::defer(actors, true);
    try {
        for (Live* const asker : askers) {
            for (const ScriptKit::Request& request : asker->processor.requests()) {
                if (const ScriptKit::Command* command = request.command()) {
                    if (command->id() == detail::SpawnCommand.id()) {
                        const ScriptKit::Arguments& arguments = request.arguments();
                        const Actor& spawned = *detail::spawnTable().at(static_cast<std::size_t>(arguments.number(0)));
                        actors.spawn(spawned, asker->place,
                                     {.x = asker->position.x + arguments.number(1),
                                      .y = asker->position.y + arguments.number(2)});
                    } else if (command->id() == detail::RemoveCommand.id()) {
                        actors.remove(asker->id);
                    } else if (config.handlers) {
                        config.handlers.apply(state, asker->id, request);
                    } else {
                        throw std::out_of_range("ActorKit: no handler for command '" + std::string(command->name()) +
                                                "'");
                    }
                    continue;
                }
                bool owned = false;
                for (const OwnValue& value : asker->actor->ownValues) {
                    if (value.apply(request, asker->own)) {
                        owned = true;
                        break;
                    }
                }
                if (owned || detail::setsProvided(request)) {
                    continue;
                }
                if (!config.bindings) {
                    throw std::out_of_range("ActorKit: actor " + std::to_string(asker->id) +
                                            " sets a value that is neither its own nor bound");
                }
                config.bindings.apply(state, request);
            }
            asker->processor.clearRequests();
        }
    } catch (...) {
        ActorsAccess::defer(actors, false);
        throw;
    }
    ActorsAccess::defer(actors, false);
    ActorsAccess::applyRemovals(actors);

    // Every actor present this tick, in spawn order, and among them the ones awake, which take part.
    std::vector<Live*> present;
    std::vector<Live*> taking;
    for (const std::unique_ptr<Live>& live : everyone) {
        if (live->newlySpawned) {
            continue;
        }
        present.push_back(live.get());
        if (live->frozen) {
            continue;
        }
        const WorldKit::Place place = live->place;
        if (place.layout < 0 || static_cast<std::size_t>(place.layout) >= world.layouts.size() || place.area < 0 ||
            static_cast<std::size_t>(place.area) >= world.layouts[static_cast<std::size_t>(place.layout)].areas.size()) {
            throw std::out_of_range("ActorKit: actor " + std::to_string(live->id) + " is in layout " +
                                    std::to_string(place.layout) + " area " + std::to_string(place.area) +
                                    ", which the world lacks");
        }
        taking.push_back(live.get());
    }
    Live* player = ActorsAccess::find(actors, actors.player());
    if (player != nullptr && player->newlySpawned) {
        player = nullptr;
    }
    const auto gather = [&](const Live& live) { detail::gatherValues(values, live, player, config.bindings, state); };

    // 2. Solidity. A frozen actor keeps the solidity it last had, worked out once if it is yet to have one.
    for (Live* live : present) {
        if (live->frozen && live->solidKnown) {
            continue;
        }
        gather(*live);
        live->solid = ScriptKit::evaluate(live->actor->solid, values);
        live->solidKnown = true;
    }

    // 3. Walking.
    if (!camera.transitioning()) {
        std::vector<retropp::IntRect> others;
        for (Live* live : taking) {
            if (!live->mover) {
                continue;
            }
            others.clear();
            for (const Live* other : present) {
                if (other != live && other->solid && other->place == live->place) {
                    others.push_back(detail::areaBox(*other));
                }
            }
            const WorldKit::Layout& layout = world.layouts[static_cast<std::size_t>(live->place.layout)];
            const WorldKit::Area& area = layout.areas[static_cast<std::size_t>(live->place.area)];
            const retropp::IndexGrid& map = *area.collision;
            const bool held = live != player;
            if (held && live->confinement == Confinement::Layout) {
                detail::otherLayoutStrips(others, area, live->place.layout, layout.cellSize);
            }
            const Solids solids{WorldKit::AreaSolids{.world = &world, .layout = live->place.layout, .area = live->place.area},
                                retropp::IntRect{.x = 0, .y = 0, .width = map.width * layout.cellSize,
                                                 .height = map.height * layout.cellSize},
                                held && live->confinement == Confinement::Area, others};
            if (live->actor->mover.kind()->readsValues()) {
                gather(*live);
            }
            live->mover->walk(detail::DriveInput{.input = &input, .type = &typeid(Input), .values = &values}, solids);
        }
    }

    // 4. Actions.
    const auto perform = [&](Live& actor, retropp::ActionId action) {
        const retropp::IntRect box = detail::areaBox(actor);
        const retropp::Dir facing = actor.mover ? actor.mover->facing() : retropp::Dir::Down;
        retropp::Vec2i point{};
        int side = Any;
        switch (facing) {
            case retropp::Dir::Up: point = {.x = box.x + box.width / 2, .y = box.y - 1}; side = Below; break;
            case retropp::Dir::Left: point = {.x = box.x - 1, .y = box.y + box.height / 2}; side = Right; break;
            case retropp::Dir::Right: point = {.x = box.x + box.width, .y = box.y + box.height / 2}; side = Left; break;
            case retropp::Dir::Down:
            case retropp::Dir::None:
            default: point = {.x = box.x + box.width / 2, .y = box.y + box.height}; side = Above; break;
        }
        for (Live* target : taking) {
            if (target == &actor || target->place != actor.place || !detail::contains(detail::areaBox(*target), point)) {
                continue;
            }
            gather(*target);
            if (config.interactAction && config.interactAction->id == action) {
                detail::startScripts(*target, Interact.kind, side, values);
            }
            for (const TriggerMap::Row& row : config.triggers.rows()) {
                if (row.detection.kind == DetectionKind::ActedOn && row.detection.action == action) {
                    detail::startScripts(*target, row.trigger.kind, side, values);
                }
            }
            return;
        }
    };
    for (Live* live : taking) {
        for (const ActionRow& row : live->actor->actions) {
            if (row.performed(&input, typeid(Input))) {
                perform(*live, row.action());
            }
        }
        std::vector<retropp::ActionId> queued = std::move(live->queued);
        live->queued.clear();
        for (const retropp::ActionId action : queued) {
            perform(*live, action);
        }
    }

    // 5. Triggers. First who touches whom, so Touched holds for every awake actor; then each awake actor's
    // triggers. Every present actor is a setter, frozen ones included.
    std::vector<std::vector<std::uint32_t>> touchedBy(taking.size());
    for (std::size_t t = 0; t < taking.size(); ++t) {
        Live& target = *taking[t];
        std::vector<std::pair<std::uint32_t, Id>> now;
        for (const Live* setter : present) {
            if (setter == &target || setter->place != target.place ||
                !detail::overlap(detail::areaBox(*setter), detail::areaBox(target))) {
                continue;
            }
            for (const TriggerKind& kind : setter->actor->setsOff) {
                if (detail::boundTo(config.triggers, kind.kind, DetectionKind::Touched)) {
                    now.emplace_back(kind.kind, setter->id);
                }
            }
        }
        for (const auto& touch : now) {
            if (std::find(target.touching.begin(), target.touching.end(), touch) == target.touching.end() &&
                std::find(touchedBy[t].begin(), touchedBy[t].end(), touch.first) == touchedBy[t].end()) {
                touchedBy[t].push_back(touch.first);
            }
        }
        target.touched = !now.empty();
        target.touching = std::move(now);
    }
    const WorldKit::Place cameraPlace{.layout = camera.layout(), .area = camera.area()};
    std::optional<WorldKit::Place>& lastCameraPlace = ActorsAccess::lastCameraPlace(actors);
    const bool cameraMoved = lastCameraPlace != cameraPlace;
    for (std::size_t t = 0; t < taking.size(); ++t) {
        Live& target = *taking[t];
        gather(target);
        for (const std::uint32_t kind : touchedBy[t]) {
            detail::startScripts(target, kind, Any, values);
        }
        const retropp::IntRect box = detail::areaBox(target);
        for (std::size_t n = 0; n < target.actor->scripts.size(); ++n) {
            const ScriptKit::Trigger& on = target.actor->scripts[n].get()->on;
            if (!on.set()) {
                continue;
            }
            if (detail::boundTo(config.triggers, on.kind, DetectionKind::Near)) {
                bool within = false;
                const long long reach = static_cast<long long>(on.number) * on.number;
                for (const Live* setter : present) {
                    if (setter != &target && setter->place == target.place &&
                        detail::listed(setter->actor->setsOff, on.kind) &&
                        detail::gapSquared(detail::areaBox(*setter), box) <= reach) {
                        within = true;
                        break;
                    }
                }
                if (within && target.within[n] == 0) {
                    detail::startScript(target, n, Any, values);
                }
                target.within[n] = within ? 1 : 0;
            } else if (detail::boundTo(config.triggers, on.kind, DetectionKind::Becomes) && on.value != 0) {
                const bool now = values.raw(on.value) != 0;
                if (target.started && now && target.wasTrue[n] == 0) {
                    detail::startScript(target, n, Any, values);
                }
                target.wasTrue[n] = now ? 1 : 0;
            }
        }
        if (target.place == cameraPlace && (!target.started || cameraMoved)) {
            for (const TriggerMap::Row& row : config.triggers.rows()) {
                if (row.detection.kind == DetectionKind::Entered) {
                    detail::startScripts(target, row.trigger.kind, Any, values);
                }
            }
        }
        target.started = true;
    }
    lastCameraPlace = cameraPlace;

    // 6. Scripts.
    for (Live* live : taking) {
        if (!live->processor.idle()) {
            gather(*live);
            live->processor.advance(values);
        }
    }
}

}  // namespace ActorKit
