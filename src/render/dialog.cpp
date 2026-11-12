#include "render/dialog.h"

#include "WindowKit/dialog_box.h"

#include "render/layout.h"

namespace zorlduh {

retropp::DrawLayer Dialog(const GameState& state, const Assets& assets, std::int32_t z) {
    return WindowKit::DialogBox("dialog", z, kDialogBox, borderOf(assets.borders, state.reading), assets.font,
                                state.dialog);
}

}  // namespace zorlduh
