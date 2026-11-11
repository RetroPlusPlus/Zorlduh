// Where things are on the 160×144 screen, in screen pixels: a 16 px HUD strip across the top and the
// play area beneath it, 20×16 cells. The camera is the world pixel shown at the play area's top-left.
#pragma once

#include "CameraKit/camera.h"

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

}  // namespace zorlduh
