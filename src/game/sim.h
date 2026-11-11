// The simulation: the game's whole state, what it is configured by, and the tick that advances it. The
// tick changes the state; everything on screen only reads it.
//
//     const zorlduh::Config config = zorlduh::gameConfig(assets, kView);
//     zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
//     state.camera.place(config.layout, state.player.collision(config.player).bounds());
//     zorlduh::tick(state, zorlduh::readInput(in), config);   // once per tick
#pragma once

#include <variant>
#include <vector>

#include "CameraKit/camera.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"
#include "WorldKit/area.h"
#include "WorldKit/camera.h"

#include "assets/assets.h"
#include "game/areas.h"

namespace zorlduh {

// What the player asked for this tick.
struct Input {
    TopDownMovement::DirectionalInput move{};  // the directions held and pressed
};

// What the tick says happened. Presentation and audio read these; the tick clears them first.
struct AreaChanged {
    Area area;  // the area the player is now in
};
using GameEvent = std::variant<AreaChanged>;

// What the game is: the player's walker config and the world. Built once from the loaded assets.
struct Config {
    TopDownMovement::WalkerConfig player;
    WorldKit::Layout layout;
};

[[nodiscard]] Config gameConfig(const Assets& assets, CameraKit::View view);

struct GameState {
    TopDownMovement::Walker player;
    WorldKit::Camera camera{kStartArea};  // which area, where the screen looks, any scroll or fade running
    std::vector<GameEvent> events;
};

// One tick: the player walks the current area unless a scroll or fade is running, the camera follows
// the player's collision box and moves the player through any opening it goes out by, and entering an
// area is recorded as an AreaChanged event.
void tick(GameState& state, const Input& input, const Config& config);

}  // namespace zorlduh
