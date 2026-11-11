// The game's controls: its actions, what each is bound to, and the plain Input the simulation reads
// each tick.
//
//     platform.actions(zorlduh::actionMap());
//     loop.simTick([&](const retropp::InputState& in) { zorlduh::tick(state, zorlduh::readInput(in), player); });
#pragma once

#include <cstdint>

#include "retropp/input.h"
#include "retropp/input_actions.h"

#include "game/sim.h"

namespace zorlduh {

enum class Action : std::uint8_t { Up, Down, Left, Right, Attack, Item, Start };

// Arrows, WASD and the d-pad move; Attack is X or the pad's B, Item is Z or the pad's A, Start is
// Return or the pad's Start.
[[nodiscard]] retropp::ActionMap actionMap();

// This tick's Input, read from the engine's.
[[nodiscard]] Input readInput(const retropp::InputState& input);

}  // namespace zorlduh
