// Every layer in the game: its depth, back to front, and its key. A layer takes both from here, so the
// whole stacking order reads in one place and no two layers share a depth or a key. The gaps leave room
// for a layer between two others.
//
// The world comes first: each area's ground, what lies under the player's feet, the actors, and what
// hangs over them, handed to the world as kAreaDepths. Above the whole world, at depths counted from
// its top, come the effects, then the HUD.
#pragma once

#include <cstdint>

#include "WorldKit/area.h"
#include "WorldKit/world_layers.h"

#include "game/areas.h"

namespace zorlduh {

inline constexpr std::int32_t kGroundZ = 0;      // each area's ground layer, keyed by WorldKit
inline constexpr std::int32_t kUnderfootZ = 5;   // a path or the like, drawn under the actors
inline constexpr std::int32_t kActorsZ = 10;     // each area's actor slot: one sprite layer, through
                                                 // WorldKit::Actors; each actor's Sprite::z is the bottom
                                                 // edge of its collision shape, so the actor lower on
                                                 // screen draws in front
inline constexpr std::int32_t kOverheadZ = 15;   // a canopy, a roof: drawn over the actors

inline constexpr AreaDepths kAreaDepths{
    .ground = kGroundZ,
    .underfoot = kUnderfootZ,
    .actors = kActorsZ,
    .overhead = kOverheadZ,
};

// Effects over the whole world: the first depth above its top layer.
[[nodiscard]] inline std::int32_t EffectsZ(const WorldKit::World& world) {
    return WorldKit::TopZ(world) + 1;
}

// The HUD, over the effects.
[[nodiscard]] inline std::int32_t HudZ(const WorldKit::World& world) {
    return WorldKit::TopZ(world) + 2;
}

}  // namespace zorlduh
