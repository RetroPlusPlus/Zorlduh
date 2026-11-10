// RasterLayer — shows a picture a component paints on the CPU, on one screen-sized raster layer.
//
//     frame.layers.push_back(RasterLayer("scope", kScopeZ, Scope(state.samples),
//                                        {.scroll = {0, -112}, .fit = {.width = 160}}));
//
// The Raster is drawn texel for texel at its own size, or at `options.fit`, with its top-left corner at
// the screen pixel the scroll places; outside it the layers below show through. The layer is the size
// of the active viewport, read from retropp::Renderer::defaultViewport on every call. The renderer
// uploads the pixels when the raster's `generation` is one it has not seen under this key, and on every
// frame while the generation is 0.
//
// The layer's pixels are a span the renderer reads while it draws, so RasterLayer keeps the Raster it
// was handed under `key` until the next call with that key replaces it. Each key is called once per
// frame: a second call with the same key replaces what the first kept, and the layer the first call
// returned can no longer be drawn. Calls with different keys never disturb each other.
#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/raster_content.h"
#include "retropp/renderer.h"

#include "layers/layer_options.h"
#include "layers/types.h"

namespace layers {

namespace detail {

// The raster each raster layer was last handed, by layer key. One table for the whole program.
inline std::map<std::string, Raster, std::less<>>& keptRasters() {
    static std::map<std::string, Raster, std::less<>> kept;
    return kept;
}

}  // namespace detail

[[nodiscard]] inline retropp::DrawLayer RasterLayer(std::string_view key, std::int32_t z,
                                                    Raster raster,
                                                    RasterLayerOptions options = {}) {
    auto& kept = detail::keptRasters();
    auto found = kept.find(key);
    if (found == kept.end()) {
        found = kept.emplace(std::string{key}, Raster{}).first;
    }
    found->second = std::move(raster);
    const Raster& shown = found->second;

    return retropp::DrawLayer{
        .key = key,
        .z = z,
        .size = retropp::Renderer::defaultViewport.size(),
        .scroll = options.scroll,
        .alpha = options.alpha,
        .blend = options.blend,
        .content = retropp::RasterContent{.pixels = std::span<const std::uint8_t>(shown.pixels),
                                          .width = shown.width,
                                          .height = shown.height,
                                          .format = shown.format,
                                          .fit = options.fit,
                                          .generation = shown.generation},
        .effects = std::move(options.effects),
        .regions = std::move(options.regions),
        .transform = options.transform,
        .transformEdge = options.transformEdge,
        .advancesEvery = options.advancesEvery,
    };
}

}  // namespace layers
