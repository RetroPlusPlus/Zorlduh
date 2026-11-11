// Camera laws: where the camera should be, given the rectangle it may show, the view's size and the
// point it is looking at. A law is a plain function; a game calls one each tick and places the camera
// where it says, or hands one to whatever drives the camera for it.
//
//     camera.place(CameraKit::follow(mapBounds, kView.size, playerCenter));
#pragma once

#include <algorithm>

#include "retropp/geometry.h"

namespace CameraKit {

namespace detail {

// One axis of `follow`: the view's start along an axis where `bounds` runs from `start` for `size`
// pixels and the view is `view` pixels long.
[[nodiscard]] constexpr int followAxis(int start, int size, int view, int focus) noexcept {
    if (size >= view) {
        return std::clamp(focus - view / 2, start, start + size - view);
    }
    return start - (view - size) / 2;
}

}  // namespace detail

// Centers the view on `focus` and stops at the edges of `bounds`, so nothing outside `bounds` comes on
// screen. On an axis where `bounds` is shorter than the view it cannot follow, so it centers `bounds`
// in the view instead and holds still. Each axis is decided on its own.
[[nodiscard]] constexpr retropp::Vec2i follow(retropp::IntRect bounds, retropp::PixelSize view,
                                              retropp::Vec2i focus) noexcept {
    return retropp::Vec2i{
        .x = detail::followAxis(bounds.x, bounds.width, view.width, focus.x),
        .y = detail::followAxis(bounds.y, bounds.height, view.height, focus.y),
    };
}

}  // namespace CameraKit
