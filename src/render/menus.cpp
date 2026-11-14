#include "render/menus.h"

#include <utility>

#include "LayerKit/sprite_layer.h"
#include "MenuKit/menu_windows.h"
#include "TextKit/text.h"

#include "render/layout.h"
#include "render/window_style.h"

namespace zorlduh {

namespace {

// How the game's menus are drawn.
MenuKit::Look look(const GameState& state, const Assets& assets) {
    return MenuKit::Look{
        .border = assets.borders.plain,
        .font = assets.font,
        .selected = assets.selectedFont,
        .disabled = assets.dimFont,
        .style = styleOf(state.menuStyle, {.x = kPlayAreaX, .y = kPlayAreaY + kPlayAreaHeight}),
    };
}

}  // namespace

LayerKit::Layers PauseMenu(const GameState& state, const Config& config, const Assets& assets, std::int32_t z) {
    return MenuKit::MenuWindows("pause", z, config.pause, state.pause, look(state, assets));
}

LayerKit::Layers TitleScreen(const GameState& state, const Config& config, const Assets& assets, std::int32_t nameZ,
                             std::int32_t menuZ) {
    LayerKit::Layers layers{LayerKit::SpriteLayer(
        "title:name", nameZ,
        {TextKit::Text("title:name", "ZORLDUH", {.x = 0, .y = kTitleNameY}, assets.font,
                       {.align = TextKit::Align::Center, .width = kPlayAreaWidth})})};
    for (retropp::DrawLayer& layer : MenuKit::MenuWindows("title", menuZ, config.title, state.title, look(state, assets))) {
        layers.push_back(std::move(layer));
    }
    return layers;
}

}  // namespace zorlduh
