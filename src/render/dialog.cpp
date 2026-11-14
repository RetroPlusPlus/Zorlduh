#include "render/dialog.h"

#include "WindowKit/dialog_box.h"

#include "render/layout.h"
#include "render/window_style.h"

namespace zorlduh {

retropp::DrawLayer Dialog(const GameState& state, const Assets& assets, std::int32_t z) {
    // A sliding box rises from the bottom of the screen.
    return WindowKit::DialogBox("dialog", z, kDialogBox, borderOf(assets.borders, state.reading), assets.font,
                                state.dialog,
                                styleOf(state.readingStyle, {.x = kDialogBox.x, .y = kPlayAreaY + kPlayAreaHeight}));
}

}  // namespace zorlduh
