// The world on screen: a tile layer for each map layer of every area in view, the things that move in
// the current area at its actor slots, and the fade over everything.
//
//     frame.layers = WorldKit::WorldLayers(state.camera, world);
//     frame.layers.push_back(WorldKit::Actors(state.camera, world, "actors", {Player(...), Enemies(...)}));
//     frame.regions = WorldKit::Fade(state.camera, world);
//
// Each map layer is keyed "area:<layout>:<area>:<layer>", drawn at its own depth, assembled from its
// map through its art, placed by the camera's scroll for that area, and confined to the layout's view:
// outside that rectangle of the screen — a HUD strip, a border, a panel — the world draws nothing,
// whatever the camera looks at. Beside the current area's layers are those of each area reached through
// seams that is in view, as many seams away as the layout's `seamDepth` allows, and during a scroll
// those of the area scrolling in. A sprite layer from Actors sits at the depth of the current area's
// slot of that name, so it is drawn over the map layers below that depth and under those above it, and
// is confined to the same view. The fade is one frame-wide color fill, returned only while a fade
// runs. Everything placed above the world — effects, a HUD — takes a depth above TopZ. This is the only
// WorldKit header that builds draw types.
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "retropp/draw_state.h"
#include "retropp/renderer.h"
#include "retropp/tilemap.h"

#include "CameraKit/camera.h"
#include "LayerKit/layer_options.h"
#include "LayerKit/sprite_layer.h"
#include "LayerKit/tile_layer.h"
#include "LayerKit/types.h"

#include "WorldKit/area.h"
#include "WorldKit/camera.h"

namespace WorldKit {

// The key of the layer the map layer named `layer` of area `area` of layout `layout` is drawn on.
[[nodiscard]] inline std::string AreaLayerKey(int layout, int area, std::string_view layer) {
    return "area:" + std::to_string(layout) + ":" + std::to_string(area) + ":" + std::string{layer};
}

// The key of the sprite layer Actors draws at the actor slot named `slot`. It names no area, so the
// layer keeps its key as the camera goes from area to area.
[[nodiscard]] inline std::string ActorsKey(std::string_view slot) {
    return "actors:" + std::string{slot};
}

// The highest depth of any map layer or actor slot of any area of any layout: a layer drawn above the
// whole world takes a depth above it. The lowest int32_t for a world with no layers.
[[nodiscard]] inline std::int32_t TopZ(const World& world) {
    std::int32_t top = std::numeric_limits<std::int32_t>::min();
    for (const Layout& layout : world.layouts) {
        for (const Area& area : layout.areas) {
            for (const MapLayer& layer : area.layers) {
                top = std::max(top, layer.z);
            }
            for (const ActorSlot& slot : area.actorSlots) {
                top = std::max(top, slot.z);
            }
        }
    }
    return top;
}

// The regions that keep a layer inside `view`: everything outside it goes see-through.
[[nodiscard]] inline LayerKit::Regions ViewStencil(const CameraKit::View& view) {
    return retropp::stencil(retropp::ShapePoints::rectangle({static_cast<float>(view.origin.x),
                                                             static_cast<float>(view.origin.y)},
                                                            static_cast<float>(view.size.width),
                                                            static_cast<float>(view.size.height)),
                            retropp::StencilMode::TransparentOutside);
}

// A tile layer for every map layer of every area the camera shows, each at its layer's depth, through
// LayerKit::TileLayer, so each layer's cells are kept under its key until the next frame; each confined
// to the current layout's view. The layers come area by area in the order the camera shows them, each
// area's in the order it lists them; the engine draws them by depth. Throws std::out_of_range when a
// map holds a value its layer's art has no entry for.
[[nodiscard]] inline LayerKit::Layers WorldLayers(const Camera& camera, const World& world) {
    LayerKit::Layers layers;
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(camera.layout())].view;
    for (const Shown& shown : camera.shown(world)) {
        const Area& area = world.layouts[static_cast<std::size_t>(shown.layout)].areas[static_cast<std::size_t>(shown.area)];
        for (const MapLayer& layer : area.layers) {
            retropp::AssembledTilemap assembled = retropp::assembleTilemap(*layer.tiles, layer.art);
            layers.push_back(LayerKit::TileLayer(AreaLayerKey(shown.layout, shown.area, layer.name), layer.z,
                                                 std::move(assembled.cells), assembled.widthInTiles,
                                                 assembled.heightInTiles,
                                                 {.scroll = shown.scroll, .regions = ViewStencil(view)}));
        }
    }
    return layers;
}

// The sprites `children` on one sprite layer, through LayerKit::SpriteLayer, keyed ActorsKey(slot), at
// the depth of the current area's actor slot named `slot`, scrolled by the camera so that a sprite
// placed in the current area's pixels lands on its map, and confined to the current layout's view.
// The regions in `options` are kept and the view's stencil added after them; the scroll is the
// camera's, whatever `options` holds; every other option passes onto the layer. Throws
// std::out_of_range when the current area has no slot by that name.
[[nodiscard]] inline retropp::DrawLayer Actors(const Camera& camera, const World& world, std::string_view slot,
                                               std::initializer_list<LayerKit::Sprites> children,
                                               LayerKit::LayerOptions options = {}) {
    const std::int32_t z = ActorZ(world, Place{.layout = camera.layout(), .area = camera.area()}, slot);
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(camera.layout())].view;
    options.scroll = camera.scroll(world);
    for (retropp::Region& region : ViewStencil(view)) {
        options.regions.push_back(std::move(region));
    }
    return LayerKit::SpriteLayer(ActorsKey(slot), z, children, std::move(options));
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
