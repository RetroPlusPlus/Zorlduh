// The menus on screen: the pause menu over the world, and the title screen.
//
//     for (retropp::DrawLayer& layer : PauseMenu(state, config, assets, MenuZ(world))) {
//         frame.layers.push_back(std::move(layer));
//     }
#pragma once

#include <cstdint>

#include "LayerKit/types.h"

#include "assets/assets.h"
#include "game/sim.h"

namespace zorlduh {

// The pause menu and every menu opened from it: one sprite layer each, keyed "pause:<n>", the first at
// `z` and each one over it a depth higher; none while it is closed. Drawn in the plain border, labels in
// the game's font, the one under the cursor in the selected font and a disabled one in the dim font,
// each window opening and closing in the style WINDOWS set — a slide rises from the bottom of the
// screen. In screen pixels.
[[nodiscard]] LayerKit::Layers PauseMenu(const GameState& state, const Config& config, const Assets& assets,
                                         std::int32_t z);

// The title screen: the game's name centered across the screen kTitleNameY down, on a sprite layer keyed
// "title:name" at `nameZ`, and the title menu with every menu opened from it from `menuZ` up, drawn as
// the pause menu is and keyed "title:<n>". In screen pixels.
[[nodiscard]] LayerKit::Layers TitleScreen(const GameState& state, const Config& config, const Assets& assets,
                                           std::int32_t nameZ, std::int32_t menuZ);

}  // namespace zorlduh
