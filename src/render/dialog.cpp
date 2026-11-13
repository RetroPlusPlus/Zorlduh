#include "render/dialog.h"

#include "WindowKit/animate.h"
#include "WindowKit/dialog_box.h"

#include "render/layout.h"

namespace zorlduh {

namespace {

// The WindowKit style a message box opens and closes in, for the style of what is read. A slide rises
// from the bottom of the screen.
WindowKit::Style styleOf(MessageStyle style) {
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
        return WindowKit::Style::slide({.x = kDialogBox.x, .y = kPlayAreaY + kPlayAreaHeight});
    case MessageStyle::Pop:
        return WindowKit::Style::pop();
    }
    return WindowKit::Style::grow();
}

}  // namespace

retropp::DrawLayer Dialog(const GameState& state, const Assets& assets, std::int32_t z) {
    return WindowKit::DialogBox("dialog", z, kDialogBox, borderOf(assets.borders, state.reading), assets.font,
                                state.dialog, styleOf(state.readingStyle));
}

}  // namespace zorlduh
