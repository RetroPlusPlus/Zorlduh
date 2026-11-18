// What the world can do to an actor: triggers, each bound to something ActorKit detects.
//
// ActorKit's own triggers are ready to use in a script's `on`:
//
//     .on = ActorKit::Touch            // an actor that sets it off meets this one's box
//     .on = ActorKit::Near(24)         // one comes within 24 pixels
//     .on = ActorKit::Enter            // this actor's area becomes the camera's
//     .on = ActorKit::Becomes(Lit)     // the value goes from false to true
//     .on = ActorKit::Interact         // an actor facing this one performs the game's interact action
//
//     const ActorKit::Config config{.world = &world, .interactAction = Action::Interact};
//
// The first four work in every game as they are. Interact works once the game names its interact action
// in its Config (config.h).
//
// A game declares triggers of its own beside them, as it declares its input actions: it owns the names,
// and each row of the map binds one to a detection.
//
//     enum class On : std::uint8_t { Poke, Brush };
//     constexpr ScriptKit::Trigger Poke{On::Poke};
//
//     const ActorKit::TriggerMap triggers{
//         {On::Poke,  ActorKit::actedOn(Action::Poke)},   // an actor facing this one performs Poke
//         {On::Brush, ActorKit::touched()},               // a second touch trigger, with its own setters
//     };
//
// When a detection happens to an actor, ActorKit starts that actor's scripts whose `on` is a trigger
// bound to it.
#pragma once

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "retropp/input.h"

#include "ScriptKit/trigger.h"
#include "ScriptKit/value.h"

namespace ActorKit {

// The game's trigger enumerator, held as ScriptKit::Trigger holds its kind: the enumerator's value plus
// one.
struct TriggerKind {
    std::uint32_t kind = 0;

    template <class E>
        requires std::is_enum_v<E>
    constexpr TriggerKind(E trigger) noexcept : kind(static_cast<std::uint32_t>(trigger) + 1) {}

    [[nodiscard]] constexpr bool operator==(const TriggerKind&) const noexcept = default;
};

// The triggers an actor's body sets off in other actors: {On::Touch, On::Near}.
using TriggerKinds = std::vector<TriggerKind>;

// One of the game's input actions, written as the game's own enumerator.
struct ActionName {
    retropp::ActionId id = 0;

    template <retropp::ActionLike A>
    constexpr ActionName(A action) noexcept : id(retropp::actionId(action)) {}
};

// The things ActorKit detects.
enum class DetectionKind : std::uint8_t { ActedOn, Touched, Near, Entered, Becomes };

// One thing ActorKit detects, made by the functions below for a TriggerMap row.
struct Detection {
    DetectionKind kind = DetectionKind::Touched;
    retropp::ActionId action = 0;   // ActedOn: the action performed

    [[nodiscard]] constexpr bool operator==(const Detection&) const noexcept = default;
};

// An actor facing this one performs `action`. Its side is where the acting actor stands: facing up it
// acts from ActorKit::Below, down from Above, left from Right, right from Left.
[[nodiscard]] constexpr Detection actedOn(ActionName action) noexcept {
    return Detection{.kind = DetectionKind::ActedOn, .action = action.id};
}

// An actor whose `setsOff` lists the trigger starts overlapping this one's box, in the same area. It
// happens on the first tick of the overlap, and again after the two have parted.
[[nodiscard]] constexpr Detection touched() noexcept { return Detection{.kind = DetectionKind::Touched}; }

// An actor whose `setsOff` lists the trigger comes within the number of pixels the script's trigger
// carries, measured between the two boxes, in the same area. Per script.
[[nodiscard]] constexpr Detection near() noexcept { return Detection{.kind = DetectionKind::Near}; }

// The camera's area becomes this actor's area; or the actor's first tick, spawned into the camera's area.
[[nodiscard]] constexpr Detection entered() noexcept { return Detection{.kind = DetectionKind::Entered}; }

// The value the script's trigger carries is true for this actor, having been false on the tick before.
// Per script.
[[nodiscard]] constexpr Detection becomes() noexcept { return Detection{.kind = DetectionKind::Becomes}; }

// The names of ActorKit's own triggers: what an actor lists in `setsOff` to set off the touch and near
// ones, `.setsOff = {ActorKit::On::Touch, ActorKit::On::Near}`. They are numbered far above the
// enumerators of a game's own trigger enum, so the two sets of names stay apart.
enum class On : std::uint32_t { Interact = 0x40000000, Touch, Near, Enter, Becomes };

// ActorKit's own triggers, for a script's `on`. Touch, Near, Enter and Becomes are bound in every
// TriggerMap; Interact is the input action named in Config::interactAction.
inline constexpr ScriptKit::Trigger Interact{On::Interact};   // an actor facing this one performs the interact action
inline constexpr ScriptKit::Trigger Touch{On::Touch};         // an actor whose setsOff lists On::Touch meets its box
inline constexpr ScriptKit::Trigger Enter{On::Enter};         // its area becomes the camera's
// An actor whose setsOff lists On::Near comes within `distance` pixels.
[[nodiscard]] constexpr ScriptKit::Trigger Near(int distance) noexcept { return {On::Near, distance}; }
// `value` goes from false to true for the actor.
[[nodiscard]] inline ScriptKit::Trigger Becomes(ScriptKit::Value<bool> value) noexcept {
    return {On::Becomes, value};
}

// Triggers, each bound to a detection. Every map binds ActorKit's own Touch, Near, Enter and Becomes;
// the rows it is built from bind the game's own triggers. Two triggers may share a detection; a trigger
// bound twice throws std::invalid_argument when the map is built.
class TriggerMap {
public:
    // One row: {On::Poke, ActorKit::actedOn(Action::Poke)}.
    struct Row {
        TriggerKind trigger;
        Detection detection;
    };

    TriggerMap() : TriggerMap(std::initializer_list<Row>{}) {}
    TriggerMap(std::initializer_list<Row> rows) : rows_(rows) {
        rows_.push_back(Row{On::Touch, touched()});
        rows_.push_back(Row{On::Near, near()});
        rows_.push_back(Row{On::Enter, entered()});
        rows_.push_back(Row{On::Becomes, becomes()});
        for (std::size_t n = 0; n < rows_.size(); ++n) {
            for (std::size_t earlier = 0; earlier < n; ++earlier) {
                if (rows_[earlier].trigger == rows_[n].trigger) {
                    throw std::invalid_argument("ActorKit::TriggerMap: trigger " +
                                                std::to_string(rows_[n].trigger.kind - 1) + " is bound twice");
                }
            }
        }
    }

    // The detection the trigger whose ScriptKit kind is `kind` is bound to; empty for a trigger outside
    // the map.
    [[nodiscard]] std::optional<Detection> detectionOf(std::uint32_t kind) const noexcept {
        for (const Row& row : rows_) {
            if (row.trigger.kind == kind) {
                return row.detection;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] const std::vector<Row>& rows() const noexcept { return rows_; }

private:
    std::vector<Row> rows_;
};

}  // namespace ActorKit
