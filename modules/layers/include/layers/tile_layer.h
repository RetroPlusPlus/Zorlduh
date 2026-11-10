// TileLayer — gathers tile cells onto one screen-sized, finite tile layer.
//
//     frame.layers.push_back(TileLayer("world", kWorldZ, WorldTiles(...), mapWidth, mapHeight,
//                                      {.scroll = {cameraX, cameraY - 16}}));
//
// The cells are a row-major grid `widthInTiles` across and `heightInTiles` down. The layer is the size
// of the active viewport, read from retropp::Renderer::defaultViewport on every call, and the grid is
// finite (retropp::TileWrap::Blank): outside it the layer shows nothing and the layers below show
// through. `options.scroll` places the grid — screen pixel (x, y) shows grid pixel
// (x + scroll.x, y + scroll.y), so a scroll of (-8, -16) puts the grid's top-left corner at (8, 16).
// Everything in `options` passes straight onto the layer.
//
// The layer's cells are a span the renderer reads while it draws, so TileLayer keeps its own copy
// under `key` until the next call with that key replaces it. Each key is called once per frame: a
// second call with the same key replaces what the first kept, and the layer the first call returned
// can no longer be drawn. Calls with different keys never disturb each other.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"

#include "layers/layer_options.h"
#include "layers/types.h"

namespace layers {

namespace detail {

// The cells each tile layer was last handed, by layer key. One table for the whole program.
inline std::map<std::string, Cells, std::less<>>& keptCells() {
    static std::map<std::string, Cells, std::less<>> kept;
    return kept;
}

}  // namespace detail

[[nodiscard]] inline retropp::DrawLayer TileLayer(std::string_view key, std::int32_t z, Cells cells,
                                                  int widthInTiles, int heightInTiles,
                                                  LayerOptions options = {}) {
    auto& kept = detail::keptCells();
    auto found = kept.find(key);
    if (found == kept.end()) {
        found = kept.emplace(std::string{key}, Cells{}).first;
    }
    found->second = std::move(cells);

    return retropp::DrawLayer{
        .key = key,
        .z = z,
        .size = retropp::Renderer::defaultViewport.size(),
        .scroll = options.scroll,
        .alpha = options.alpha,
        .blend = options.blend,
        .content = retropp::TileContent{.widthInTiles = widthInTiles,
                                        .heightInTiles = heightInTiles,
                                        .cells = std::span<const retropp::TileCell>(found->second),
                                        .wrap = retropp::TileWrap::Blank},
        .effects = std::move(options.effects),
        .regions = std::move(options.regions),
        .transform = options.transform,
        .transformEdge = options.transformEdge,
        .advancesEvery = options.advancesEvery,
    };
}

}  // namespace layers
