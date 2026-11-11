// The world on screen: one tile layer per area in view, and the fade over everything.
//
//     frame.layers = WorldKit::WorldLayers(state.camera, layout, kWorldZ);
//     frame.regions = WorldKit::Fade(state.camera, layout);
//
// Each area's layer is keyed "area:<index>" and assembled from its tile map through its art, placed by
// the camera's scroll for that area; a second area appears only while a scroll between two runs. The
// fade is one frame-wide color fill, returned only while a fade runs. This is the only WorldKit header
// that builds draw types.
#pragma once

#include <cstdint>
#include <string>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"
#include "retropp/tilemap.h"

#include "LayerKit/tile_layer.h"
#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

namespace WorldKit {

// The key of the layer area `area` is drawn on.
[[nodiscard]] inline std::string AreaLayerKey(int area) {
    return "area:" + std::to_string(area);
}

// A tile layer at depth `z` for every area the camera shows, through LayerKit::TileLayer, so each
// layer's cells are kept under its key until the next frame. Throws std::out_of_range when a map
// holds a value its area's art has no entry for.
[[nodiscard]] inline LayerKit::Layers WorldLayers(const Camera& camera, const Layout& layout, std::int32_t z) {
    LayerKit::Layers layers;
    for (const Shown& shown : camera.shown(layout)) {
        const Area& area = layout.areas[static_cast<std::size_t>(shown.area)];
        retropp::AssembledTilemap assembled = retropp::assembleTilemap(*area.tiles, area.art);
        layers.push_back(LayerKit::TileLayer(AreaLayerKey(shown.area), z, std::move(assembled.cells),
                                             assembled.widthInTiles, assembled.heightInTiles,
                                             {.scroll = shown.scroll}));
    }
    return layers;
}

// The fade over the whole screen, keyed "world:fade": a color fill in the layout's fade color at the
// camera's fade opacity, over the active viewport. Nothing while no fade runs.
[[nodiscard]] inline LayerKit::Regions Fade(const Camera& camera, const Layout& layout) {
    const float opacity = camera.fade(layout);
    if (opacity <= 0.0f) {
        return {};
    }
    const retropp::PixelSize screen = retropp::Renderer::defaultViewport.size();
    return {retropp::Region{
        .key = "world:fade",
        .shape = retropp::ShapePoints::rectangle({0.0f, 0.0f}, static_cast<float>(screen.width),
                                                 static_cast<float>(screen.height)),
        .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::ColorFill,
                                               .fill = layout.fadeColor}},
        .alpha = opacity,
    }};
}

}  // namespace WorldKit
