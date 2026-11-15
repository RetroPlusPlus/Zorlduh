// The game's controls: its actions, what each is bound to, and the plain Input the simulation reads
// each tick. In a menu the directions move the cursor, Interact confirms and Attack cancels.
//
//     platform.actions(zorlduh::actionMap());
//     loop.simTick([&](const retropp::InputState& in) { zorlduh::tick(state, zorlduh::readInput(in), player); });
#pragma once

#include <cstdint>

#include "retropp/input.h"
#include "retropp/input_actions.h"

#include "game/sim.h"

namespace zorlduh {

enum class Action : std::uint8_t { Up, Down, Left, Right, Attack, Interact, Start };

// Arrows, WASD and the d-pad move; Interact is X or the pad's A, Attack is Z or the pad's B, Start is
// Return or the pad's Start.
[[nodiscard]] retropp::ActionMap actionMap();

// This tick's Input, read from the engine's: the directions, Interact and Start, and the same buttons as
// a menu reads them.
[[nodiscard]] Input readInput(const retropp::InputState& input);

}  // namespace zorlduh
