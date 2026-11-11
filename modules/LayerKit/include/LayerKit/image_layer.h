// ImageLayer — shows a truecolor image the renderer holds, on one screen-sized raster layer.
//
//     const retropp::ImageId titleCard = renderer.loadImage("assets/title.png");  // once, at load
//
//     frame.layers.push_back(ImageLayer("title", kTitleZ, titleCard,
//                                       {.scroll = {-8, -16}, .fit = {.width = 144, .height = 112}}));
//
// The image is registered once with retropp::Renderer::loadImage or uploadImage, and the layer names it
// by its retropp::ImageId every frame. The renderer binds the image's own texture, so nothing is
// uploaded for it on any frame. It is drawn at its own size, or at `options.fit`, with its top-left
// corner at the screen pixel the scroll places — a scroll of (-8, -16) puts it at (8, 16) — and outside
// it the layers below show through. The layer is the size of the active viewport, read from
// retropp::Renderer::defaultViewport on every call. An id the renderer never returned draws nothing.
//
// The layer holds no span, so ImageLayer keeps nothing between calls.
#pragma once

#include <cstdint>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/image.h"
#include "retropp/raster_content.h"
#include "retropp/renderer.h"

#include "LayerKit/layer_options.h"

namespace LayerKit {

[[nodiscard]] inline retropp::DrawLayer ImageLayer(std::string_view key, std::int32_t z,
                                                   retropp::ImageId image,
                                                   RasterLayerOptions options = {}) {
    return retropp::DrawLayer{
        .key = key,
        .z = z,
        .size = retropp::Renderer::defaultViewport.size(),
        .scroll = options.scroll,
        .alpha = options.alpha,
        .blend = options.blend,
        .content = retropp::RasterContent{.fit = options.fit, .image = image},
        .effects = std::move(options.effects),
        .regions = std::move(options.regions),
        .transform = options.transform,
        .transformEdge = options.transformEdge,
        .advancesEvery = options.advancesEvery,
    };
}

}  // namespace LayerKit
