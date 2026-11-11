// What a rendering component returns. A component is a function of the state it shows, and it returns
// the engine primitives it is: a leaf or a group of leaves returns its Sprites, Cells, Regions or
// Raster; a function that gathers components onto a layer (SpriteLayer, TileLayer, RasterLayer,
// ImageLayer) returns a retropp::DrawLayer; a screen returns its Layers. The frame is the list of what
// these return.
#pragma once

#include <cstdint>
#include <vector>

#include "retropp/draw_state.h"
#include "retropp/raster_content.h"

namespace LayerKit {

using Sprites = std::vector<retropp::Sprite>;
using Cells = std::vector<retropp::TileCell>;
using Regions = std::vector<retropp::Region>;
using Layers = std::vector<retropp::DrawLayer>;

// A picture a component paints on the CPU, owning its pixels. RasterLayer shows it.
struct Raster {
    // Row-major, width × height × retropp::bytesPerPixel(format) bytes. A raster holding fewer bytes
    // than that is not drawn.
    std::vector<std::uint8_t> pixels;
    int width = 0;
    int height = 0;
    retropp::RasterPixelFormat format = retropp::RasterPixelFormat::Rgba8888;
    // How many pictures the component has finished. Two submissions under the same non-zero generation
    // carry the same picture, so the renderer uploads it once; generation 0 is uploaded every frame.
    std::uint64_t generation = 0;
};

}  // namespace LayerKit
