// The simulation: the game's whole state, what it is configured by, and the tick that advances it. The
// tick changes the state; everything on screen only reads it.
//
//     const zorlduh::Config config = zorlduh::gameConfig(assets, kView, kAreaDepths);
//     zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
//     state.camera.place(config.world, state.player.collision(config.player).bounds());
//     zorlduh::tick(state, zorlduh::readInput(in), config);   // once per tick
#pragma once

#include <variant>
#include <vector>

#include "CameraKit/camera.h"
#include "TextKit/reveal.h"
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
    Layout layout;  // the layout the player is now in
    int area;       // the area in it: an Area in the overworld, a Room in DungeonOne, a Woods in
                    // SeamlessForest
};
using GameEvent = std::variant<AreaChanged>;

// What the game is: the player's walker config and the world. Built once from the loaded assets.
struct Config {
    TopDownMovement::WalkerConfig player;
    WorldKit::World world;
};

// The game's config over `assets`, with the world shown in `view` and its areas' layers and actor
// slots at `depths`.
[[nodiscard]] Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths);

struct GameState {
    TopDownMovement::Walker player;
    WorldKit::Camera camera{kStartLayout, kStartArea};  // which area, where the screen looks, any scroll or fade running
    TextKit::Reveal nameReveal;  // how much of the current area's name has been typed into the HUD strip
    std::vector<GameEvent> events;
};

// One tick: the player walks the current area unless a scroll or fade is running, the camera follows
// the player's collision box and moves the player through any opening it goes out by, and entering an
// area is recorded as an AreaChanged event and starts its name typing out again. The name's reveal
// advances every tick.
void tick(GameState& state, const Input& input, const Config& config);

}  // namespace zorlduh
