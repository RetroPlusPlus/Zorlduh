// The world on screen: one tile layer per area in view, and the fade over everything.
//
//     frame.layers = WorldKit::WorldLayers(state.camera, world, kWorldZ);
//     frame.regions = WorldKit::Fade(state.camera, world);
//
// Each area's layer is keyed "area:<layout>:<area>" and assembled from its tile map through its art,
// placed by the camera's scroll for that area, and confined to the layout's view: outside that
// rectangle of the screen — a HUD strip, a border, a panel — the world draws nothing, whatever the
// camera looks at. Beside the current area there is a layer for each area reached through seams that
// is in view, as many seams away as the layout's `seamDepth` allows, and during a scroll one for the
// area scrolling in. The fade is one frame-wide
// color fill, returned only while a fade runs. This is the only WorldKit header that builds draw
// types.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "LayerKit/tile_layer.h"
#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

namespace WorldKit {

// The key of the layer area `area` of layout `layout` is drawn on.
[[nodiscard]] inline std::string AreaLayerKey(int layout, int area) {
    return "area:" + std::to_string(layout) + ":" + std::to_string(area);
}

// The regions that keep a layer inside `view`: everything outside it goes see-through.
[[nodiscard]] inline LayerKit::Regions ViewStencil(const CameraKit::View& view) {
    return retropp::stencil(retropp::ShapePoints::rectangle({static_cast<float>(view.origin.x),
                                                             static_cast<float>(view.origin.y)},
                                                            static_cast<float>(view.size.width),
                                                            static_cast<float>(view.size.height)),
                            retropp::StencilMode::TransparentOutside);
}

// A tile layer at depth `z` for every area the camera shows, through LayerKit::TileLayer, so each
// layer's cells are kept under its key until the next frame; each confined to the current layout's
// view. Throws std::out_of_range when a map holds a value its area's art has no entry for.
[[nodiscard]] inline LayerKit::Layers WorldLayers(const Camera& camera, const World& world, std::int32_t z) {
    LayerKit::Layers layers;
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(camera.layout())].view;
    for (const Shown& shown : camera.shown(world)) {
        const Area& area = world.layouts[static_cast<std::size_t>(shown.layout)].areas[static_cast<std::size_t>(shown.area)];
        retropp::AssembledTilemap assembled = retropp::assembleTilemap(*area.tiles, area.art);
        layers.push_back(LayerKit::TileLayer(AreaLayerKey(shown.layout, shown.area), z, std::move(assembled.cells),
                                             assembled.widthInTiles, assembled.heightInTiles,
                                             {.scroll = shown.scroll, .regions = ViewStencil(view)}));
    }
    return layers;
}

// The fade over the whole screen, keyed "world:fade": a color fill in the current layout's fade color
// at the camera's fade opacity, over the active viewport. Nothing while no fade runs.
[[nodiscard]] inline LayerKit::Regions Fade(const Camera& camera, const World& world) {
    const float opacity = camera.fade(world);
    if (opacity <= 0.0f) {
        return {};
    }
    const retropp::PixelSize screen = retropp::Renderer::defaultViewport.size();
    return {retropp::Region{
        .key = "world:fade",
        .shape = retropp::ShapePoints::rectangle({0.0f, 0.0f}, static_cast<float>(screen.width),
                                                 static_cast<float>(screen.height)),
        .effects = {retropp::ScreenSpaceEffect{.kind = retropp::ScreenSpaceEffectKind::ColorFill,
                                               .fill = world.layouts[static_cast<std::size_t>(camera.layout())].fadeColor}},
        .alpha = opacity,
    }};
}

}  // namespace WorldKit
