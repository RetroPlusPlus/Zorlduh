#include "render/area_name.h"

#include <string_view>

#include "TextKit/text.h"

#include "game/areas.h"
#include "render/layout.h"

namespace zorlduh {

LayerKit::Sprites AreaName(const GameState& state, const Assets& assets) {
    const std::string_view name = areaName(static_cast<Layout>(state.camera.layout()), state.camera.area());
    return TextKit::Text("area-name", name, {.x = 0, .y = 4}, assets.font,
                         {.shown = state.nameReveal.shown(), .align = TextKit::Align::Right,
                          .width = kPlayAreaWidth - 8});
}

}  // namespace zorlduh
