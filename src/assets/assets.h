// The game's art and maps: every sheet uploaded as an atlas, every palette uploaded from its palette
// image, and the world and collision maps, loaded once at startup and handed to whatever draws or
// reads them.
//
//     const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
//
//     const TopDownMovement::WalkerConfig player{.walk = assets.playerWalk, ...};
//
// Every file under assets/ is written by tools/placeholder_art.py, and every one is baked into the
// executable, so the game ships as one file.
#pragma once

#include <cstddef>
#include <cstdint>

#include "retropp/atlas_manifest.h"
#include "retropp/image.h"
#include "retropp/palette.h"

#include "TopDownMovement/walker.h"

namespace retropp {
class Renderer;
}

namespace zorlduh {

// The value of a cell in the world's tile map, assets/maps/world/tiles.png. The values are spread across
// the 8-bit grayscale range so the map reads by eye: void is black, floor is white.
enum class WorldCell : std::uint16_t {
    Void = 0,       // outside every room
    Wall = 64,
    Water = 128,
    Doorway = 192,  // floor in a room's opening, on both sides of the edge two rooms share
    Floor = 255,
};

// The value of a cell in the world's collision map, assets/maps/world/collision.png: solid is black,
// open is white. Wall, water and void are solid.
enum class CollisionCell : std::uint16_t {
    Solid = 0,
    Open = 255,
};

// The slots of the terrain sheet: one 8×8 tile per WorldCell, in the same order.
enum class TerrainTile : std::size_t {
    Floor = 0,
    Wall = 1,
    Water = 2,
    Doorway = 3,
    Void = 4,
};

// The animations of the player sheet: one row of two walk frames per facing. A standing player
// shows frame 0 of its facing.
enum class PlayerWalk : std::size_t {
    Down = 0,
    Up = 1,
    Left = 2,
    Right = 3,
};

// The slots of the hearts sheet.
enum class Heart : std::size_t {
    Full = 0,
    Half = 1,
    Empty = 2,
};

// Each palette is four colors, dark at entry 0 to light at entry 3, in the order of the gray levels a
// sheet shows in an image viewer. A sprite palette's entry 0 is its sheet's transparent index and is
// never drawn.
struct Palettes {
    retropp::PaletteId floor{};  // Floor and Doorway tiles
    retropp::PaletteId wall{};
    retropp::PaletteId water{};
    retropp::PaletteId voidCells{};
    retropp::PaletteId player{};
    retropp::PaletteId enemy{};
    retropp::PaletteId sword{};
    retropp::PaletteId hearts{};
};

struct Assets {
    retropp::AtlasManifest terrain;  // Tileset of 8×8 tiles, slots in TerrainTile order
    retropp::AtlasManifest player;   // AnimationSeries of 16×16 frames, animations in PlayerWalk order
    TopDownMovement::DirectionalAnimations playerWalk;  // each facing's row of the player sheet, two
                                                        // frames of 8 ticks each, through the player palette
    retropp::AtlasManifest enemy;    // SingleAnimation: two 16×16 frames
    retropp::AtlasManifest sword;    // Single: one 8×16 frame pointing up
    retropp::AtlasManifest hearts;   // SpriteSeries of 8×8 cells, slots in Heart order
    Palettes palettes;
    retropp::IndexGrid world;      // 60×32 cells, one WorldCell value each
    retropp::IndexGrid collision;  // the same size, one CollisionCell value each
};

// Uploads every sheet and palette to `renderer` and decodes both maps. Index 0 is transparent on the
// player, enemy, sword and hearts sheets; the terrain sheet has no transparent index. Throws
// std::runtime_error when a file cannot be decoded or uploaded.
[[nodiscard]] Assets loadAssets(retropp::Renderer& renderer);

}  // namespace zorlduh
