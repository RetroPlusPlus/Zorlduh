// Everything a layer carries beyond its key, depth, size and content. SpriteLayer and TileLayer own
// those four and copy every field here onto the layer unchanged, so a caller can scroll, fade, blend,
// transform, confine an effect to part of a layer, or set its advance cadence without the aggregate
// knowing what any of it means. Every default is the engine's own: a layer given no options draws
// exactly as a plain retropp::DrawLayer would.
//
//     SpriteLayer("actors", kActorsZ, {Player(...)},
//                 {.scroll = {cameraX, cameraY - 16}, .regions = RoomClip(...)});
#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/transform.h"

namespace LayerKit {

struct LayerOptions {
    retropp::LayerScroll scroll{};
    float alpha = 1.0f;
    retropp::BlendMode blend = retropp::BlendMode::Normal;
    std::vector<retropp::ScreenSpaceEffect> effects{};
    std::vector<retropp::Region> regions{};
    retropp::Transform transform{};
    retropp::DisplacementEdge transformEdge = retropp::DisplacementEdge::Blank;
    // Ticks between one advance of this layer's state and the next. Unset, the layer takes the frame's
    // retropp::FrameDrawState::advancesEvery.
    std::optional<std::uint32_t> advancesEvery{};
};

// The options of a raster layer (RasterLayer, ImageLayer): every LayerOptions field, in the same order
// with the same defaults, then `fit`. A field added to LayerOptions is added here too; the kit's tests
// fail to compile until it is.
//
//     ImageLayer("title", kTitleZ, titleCard,
//                {.scroll = {-8, -16}, .fit = {.width = 144, .height = 112}});
struct RasterLayerOptions {
    retropp::LayerScroll scroll{};
    float alpha = 1.0f;
    retropp::BlendMode blend = retropp::BlendMode::Normal;
    std::vector<retropp::ScreenSpaceEffect> effects{};
    std::vector<retropp::Region> regions{};
    retropp::Transform transform{};
    retropp::DisplacementEdge transformEdge = retropp::DisplacementEdge::Blank;
    std::optional<std::uint32_t> advancesEvery{};
    // The size the picture is shown at, in viewport pixels. {0, 0} shows it at its own size. Set, the
    // picture fills exactly this size, each viewport pixel taking the texel under it, with no filtering;
    // an axis left at 0 keeps the picture's own size on that axis.
    retropp::PixelSize fit{};
};

}  // namespace LayerKit
