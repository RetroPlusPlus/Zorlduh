// The simulation: the game's whole state, what it is configured by, and the tick that advances it. The
// tick changes the state; everything on screen only reads it.
//
//     const zorlduh::Config config = zorlduh::gameConfig(assets, kView, kAreaDepths, kDialogBox);
//     zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
//     state.camera.place(config.world, state.player.collision(config.player).bounds());
//     zorlduh::tick(state, zorlduh::readInput(in), config);   // once per tick
#pragma once

#include <variant>
#include <vector>

#include "retropp/geometry.h"

#include "CameraKit/camera.h"
#include "TextKit/font.h"
#include "TextKit/reveal.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"
#include "WindowKit/border.h"
#include "WindowKit/dialog.h"
#include "WorldKit/area.h"
#include "WorldKit/camera.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/signs.h"

namespace zorlduh {

// What the player asked for this tick.
struct Input {
    TopDownMovement::DirectionalInput move{};  // the directions held and pressed
    bool item = false;                         // Item was pressed this tick
};

// What the tick says happened. Presentation and audio read these; the tick clears them first.
struct AreaChanged {
    Layout layout;  // the layout the player is now in
    int area;       // the area in it: an Area in the overworld, a Room in DungeonOne, a Woods in
                    // SeamlessForest
};
using GameEvent = std::variant<AreaChanged>;

// What the game is: the player's walker config, the world, and what a message is read in. Built once
// from the loaded assets.
struct Config {
    TopDownMovement::WalkerConfig player;
    WorldKit::World world;
    TextKit::Font font;           // a message's text
    Borders borders;              // the border of each material
    retropp::IntRect messageBox;  // where a message's box is, on screen
};

// The game's config over `assets`, with the world shown in `view`, its areas' layers and actor slots
// at `depths`, and a message read in `messageBox`.
[[nodiscard]] Config gameConfig(const Assets& assets, CameraKit::View view, const AreaDepths& depths,
                                retropp::IntRect messageBox);

// The border a message from a thing of `material` is drawn in.
[[nodiscard]] const WindowKit::Border& borderOf(const Borders& borders, Material material) noexcept;

struct GameState {
    TopDownMovement::Walker player;
    WorldKit::Camera camera{kStartLayout, kStartArea};  // which area, where the screen looks, any scroll or fade running
    TextKit::Reveal nameReveal;  // how much of the current area's name has been typed into the HUD strip
    WindowKit::Dialog dialog;    // the message being read, if any
    Material reading = Material::Wood;  // what the thing being read is made of: the border its message is in
    std::vector<GameEvent> events;
};

// One tick. While a message is showing, Item confirms it (shows the page, turns it, closes it after
// the last) and nothing else moves. Otherwise the player walks the current area unless a scroll or fade
// is running, the camera follows the player's collision box and moves the player through any opening
// it goes out by, and entering an area is recorded as an AreaChanged event and starts its name typing
// out again; then Item, with no scroll or fade running, opens the message of the sign or rock the
// player faces. The name's reveal advances every tick.
void tick(GameState& state, const Input& input, const Config& config);

}  // namespace zorlduh
