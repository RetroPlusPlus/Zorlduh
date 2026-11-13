// The message box, over everything else on screen.
//
//     frame.layers.push_back(Dialog(state, assets, DialogZ(world)));
#pragma once

#include <cstdint>

#include "retropp/draw_state.h"

#include "assets/assets.h"
#include "game/sim.h"

namespace zorlduh {

// The message box with the message being read typed into it: one sprite layer keyed "dialog" at `z`,
// the box at kDialogBox in the border of what is being read, opening and closing in its style. No
// sprites while nothing is read. In screen pixels: the layer scrolls only while a sliding box moves.
[[nodiscard]] retropp::DrawLayer Dialog(const GameState& state, const Assets& assets, std::int32_t z);

}  // namespace zorlduh
