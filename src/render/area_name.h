// The current area's name in the HUD strip, for the sprite layer over the world.
//
//     LayerKit::SpriteLayer("hud:area-name", HudZ(world), {AreaName(state, assets)});
#pragma once

#include "assets/assets.h"
#include "game/sim.h"
#include "LayerKit/types.h"

namespace zorlduh {

// The current area's name in the HUD strip, typed out as the game's reveal advances: right-aligned
// across the strip with an 8 px margin, 4 px down, in the game's font with its shadow, keyed
// "area-name:<n>" per character. In screen pixels: the layer it is on does not scroll.
[[nodiscard]] LayerKit::Sprites AreaName(const GameState& state, const Assets& assets);

}  // namespace zorlduh
