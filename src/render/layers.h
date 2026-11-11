// Every layer in the game: its depth, back to front, and its key. A layer takes both from here, so the
// whole stacking order reads in one place and no two layers share a depth or a key. The gaps leave room
// for a layer between two others.
#pragma once

#include <cstdint>
#include <string_view>

namespace zorlduh {

inline constexpr std::int32_t kWorldZ = 0;
inline constexpr std::int32_t kActorsZ = 10;  // one sprite layer; each actor's Sprite::z is the bottom
                                              // edge of its collision shape, so the actor lower on
                                              // screen draws in front
inline constexpr std::int32_t kEffectsZ = 20;
inline constexpr std::int32_t kHudZ = 100;

// The world map, every room on one tile layer.
inline constexpr std::string_view kWorldLayer = "world";

// Everything that walks the world, on one sprite layer placed like the world layer.
inline constexpr std::string_view kActorsLayer = "actors";

}  // namespace zorlduh
