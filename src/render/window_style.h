// The WindowKit style for each of the game's window styles: how a message's box and the menus' windows
// open and close.
//
//     WindowKit::DialogBox("dialog", z, kDialogBox, border, font, state.dialog,
//                          styleOf(state.readingStyle, {.x = kDialogBox.x, .y = kPlayAreaY + kPlayAreaHeight}));
#pragma once

#include "retropp/geometry.h"

#include "WindowKit/animate.h"

#include "game/signs.h"

namespace zorlduh {

// The WindowKit style for `style`: grow and clip from the box's middle, squash across its middle, fade,
// slide from `slideFrom` (where the box's top-left starts, in screen pixels), or pop.
[[nodiscard]] inline WindowKit::Style styleOf(MessageStyle style, retropp::Vec2i slideFrom) {
    switch (style) {
    case MessageStyle::Grow:
        return WindowKit::Style::grow();
    case MessageStyle::Clip:
        return WindowKit::Style::clip();
    case MessageStyle::Squash:
        return WindowKit::Style::squash();
    case MessageStyle::Fade:
        return WindowKit::Style::fade();
    case MessageStyle::Slide:
        return WindowKit::Style::slide(slideFrom);
    case MessageStyle::Pop:
        return WindowKit::Style::pop();
    }
    return WindowKit::Style::grow();
}

}  // namespace zorlduh
