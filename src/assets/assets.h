// The game's art and maps: every sheet uploaded as an atlas, every palette uploaded from its palette
// image, and each area's tile and collision maps, loaded once at startup and handed to whatever draws
// or reads them.
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

// The value of a cell in an area's tile map, assets/maps/<layout>/<area>/tiles.png. The values are
// spread across the 8-bit grayscale range so the map reads by eye: stairs are near black, floor is
// white.
enum class WorldCell : std::uint16_t {
    Stairs = 32,    // a way down or up: a spot opening
    Wall = 64,
    Water = 128,
    Doorway = 192,  // floor in a wall where an opening is
    Floor = 255,
};

// The value of a cell in an area's collision map, assets/maps/<layout>/<area>/collision.png: solid is
// black, open is white. Wall and water are solid.
enum class CollisionCell : std::uint16_t {
    Solid = 0,
    Open = 255,
};

// The slots of the terrain sheet: one 8×8 tile per WorldCell.
enum class TerrainTile : std::size_t {
    Floor = 0,
    Wall = 1,
    Water = 2,
    Doorway = 3,
    Stairs = 4,
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
    retropp::PaletteId player{};
    retropp::PaletteId enemy{};
    retropp::PaletteId sword{};
    retropp::PaletteId hearts{};
};

// One area's maps: the same size, one WorldCell value per cell in `tiles` and one CollisionCell value
// per cell in `collision`.
struct AreaMaps {
    retropp::IndexGrid tiles;
    retropp::IndexGrid collision;
};

// The overworld's areas' maps, from assets/maps/overworld/<area>/.
struct OverworldMaps {
    AreaMaps meadow;  // 20×16 cells: the start
    AreaMaps lake;    // 40×16, with a pond and the dungeon door
    AreaMaps field;   // 30×16
    AreaMaps shrine;  // 12×10
    AreaMaps cave;    // 12×10
};

// DungeonOne's rooms' maps, from assets/maps/dungeon-one/<room>/.
struct DungeonOneMaps {
    AreaMaps entrance;  // 20×16: the door back to the lake
    AreaMaps hall;      // 20×16: doors to the wings, open along its north wall to the treasury
    AreaMaps westWing;  // 12×10
    AreaMaps eastWing;  // 12×10
    AreaMaps treasury;  // 30×16: open along its south wall to the hall
};

// Every area's maps, by layout.
struct AreaMapSet {
    OverworldMaps overworld;
    DungeonOneMaps dungeonOne;
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
    AreaMapSet areas;
};

// Uploads every sheet and palette to `renderer` and decodes every area's maps. Index 0 is transparent
// on the player, enemy, sword and hearts sheets; the terrain sheet has no transparent index. Throws
// std::runtime_error when a file cannot be decoded or uploaded.
[[nodiscard]] Assets loadAssets(retropp::Renderer& renderer);

}  // namespace zorlduh
