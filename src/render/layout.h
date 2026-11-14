// Where things are on the 160×144 screen, in screen pixels: a 16 px HUD strip across the top and the
// play area beneath it, 20×16 cells. The camera is the world pixel shown at the play area's top-left.
#pragma once

#include "retropp/geometry.h"

#include "CameraKit/camera.h"

#include "game/menus.h"

namespace zorlduh {

inline constexpr int kHudHeight = 16;

inline constexpr int kPlayAreaX = 0;
inline constexpr int kPlayAreaY = kHudHeight;
inline constexpr int kPlayAreaWidth = 160;
inline constexpr int kPlayAreaHeight = 128;

// The play area as the camera's view: what the camera shows, placed under the HUD strip.
inline constexpr CameraKit::View kView{
    .size = {.width = kPlayAreaWidth, .height = kPlayAreaHeight},
    .origin = {.x = kPlayAreaX, .y = kPlayAreaY},
};

// The message box: across the bottom of the screen. Inside an 8 px border it leaves 144×32 for text,
// 18 characters by 4 lines in the game's font.
inline constexpr retropp::IntRect kDialogBox{.x = 0, .y = 96, .width = 160, .height = 48};

// The title screen: the game's name across it, 40 px down.
inline constexpr int kTitleNameY = 40;

// Each menu's window. Inside an 8 px border every room is a whole number of 8 px rows, one row an item
// (the styles' two rows hold two items each, the warp's five rows a page), and every label ends a cell
// short of the room's right edge where a menu shows arrows.
inline constexpr MenuBoxes kMenuBoxes{
    .title = {.x = 32, .y = 80, .width = 96, .height = 40},
    .pause = {.x = 8, .y = 24, .width = 88, .height = 48},
    .options = {.x = 24, .y = 40, .width = 88, .height = 32},
    .text = {.x = 40, .y = 56, .width = 80, .height = 40},
    .styles = {.x = 16, .y = 56, .width = 128, .height = 32},
    .warp = {.x = 40, .y = 32, .width = 104, .height = 56},
};

}  // namespace zorlduh
