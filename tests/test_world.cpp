// The world: which pixels are solid, which room holds a point, that the room table matches the map,
// that the world's tiles cover the map and draw each kind of cell through its own palette, and that the
// screen layout fills the viewport. No renderer is needed: the terrain sheet is carved from its size.

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>

#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"
#include "retropp/tilemap.h"

#include "assets/assets.h"
#include "config/config.h"
#include "game/rooms.h"
#include "game/world.h"
#include "render/layout.h"
#include "render/world.h"

namespace {

using zorlduh::kCellSize;
using zorlduh::RoomId;
using zorlduh::TerrainTile;
using zorlduh::World;
using zorlduh::WorldCell;

World loadWorld() {
    return World{.tiles = retropp::loadMapPng("assets/maps/world/tiles.png"),
                 .collision = retropp::loadMapPng("assets/maps/world/collision.png")};
}

// The pixel at the top-left of cell (cx, cy).
int px(int cell) {
    return cell * kCellSize;
}

// Assets as the game loads them, with handles made up instead of minted by a renderer: the terrain
// sheet is carved from its 40×8 size, and every palette has a handle of its own.
zorlduh::Assets fakeAssets() {
    zorlduh::Assets assets;
    assets.terrain = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{7},
        .slots = retropp::sliceLayout({.width = 40, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset,
                                      retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    assets.palettes = zorlduh::Palettes{
        .floor = retropp::PaletteId{4},
        .wall = retropp::PaletteId{8},
        .water = retropp::PaletteId{12},
        .voidCells = retropp::PaletteId{16},
    };
    return assets;
}

std::uint16_t slot(TerrainTile tile) {
    return static_cast<std::uint16_t>(tile);
}

// ── Solid cells ──────────────────────────────────────────────────────────────────────────────

TEST(World, WallWaterAndVoidAreSolid) {
    const World world = loadWorld();
    EXPECT_TRUE(world.solidAt(px(0), px(0)));              // room 0's top-left wall
    EXPECT_TRUE(world.solidAt(px(0) + 7, px(0) + 7));      // the same cell's last pixel
    EXPECT_TRUE(world.solidAt(px(40), px(7)));             // the pond in room 1
    EXPECT_TRUE(world.solidAt(px(45) + 3, px(20) + 3));    // void between rooms
}

TEST(World, FloorAndOpeningsAreOpen) {
    const World world = loadWorld();
    EXPECT_FALSE(world.solidAt(px(5), px(5)));             // floor in room 0
    EXPECT_FALSE(world.solidAt(px(19), px(6)));            // the 0↔1 opening, room 0's side
    EXPECT_FALSE(world.solidAt(px(20) + 7, px(9) + 7));    // the 0↔1 opening, room 1's side
    EXPECT_FALSE(world.solidAt(px(8), px(16)));            // the 0↔2 opening, room 2's side
}

TEST(World, EveryPixelOffTheMapIsSolid) {
    const World world = loadWorld();
    EXPECT_TRUE(world.solidAt(-1, px(5)));
    EXPECT_TRUE(world.solidAt(px(5), -1));
    EXPECT_TRUE(world.solidAt(px(60), px(5)));
    EXPECT_TRUE(world.solidAt(px(5), px(32)));
}

TEST(World, ABoxIsSolidWhenAnyPixelOfItIs) {
    const World world = loadWorld();
    // Room 0's right wall is column 19 (pixels 152–159); row 5 has no opening.
    EXPECT_FALSE(world.solidBox({.x = 142, .y = px(5), .width = 10, .height = 8}));  // ends at 151
    EXPECT_TRUE(world.solidBox({.x = 143, .y = px(5), .width = 10, .height = 8}));   // reaches 152
    // Its left wall is column 0 (pixels 0–7).
    EXPECT_FALSE(world.solidBox({.x = 8, .y = px(5), .width = 10, .height = 8}));
    EXPECT_TRUE(world.solidBox({.x = 7, .y = px(5), .width = 10, .height = 8}));
    // A box partly off the map.
    EXPECT_TRUE(world.solidBox({.x = -4, .y = px(5), .width = 10, .height = 8}));
}

TEST(World, AnEmptyBoxIsNeverSolid) {
    const World world = loadWorld();
    EXPECT_FALSE(world.solidBox({.x = 0, .y = 0, .width = 0, .height = 8}));
    EXPECT_FALSE(world.solidBox({.x = 0, .y = 0, .width = 8, .height = 0}));
}

// ── Rooms ────────────────────────────────────────────────────────────────────────────────────

TEST(World, EachRoomHoldsItsOwnPoints) {
    const World world = loadWorld();
    EXPECT_EQ(world.roomAt(px(10), px(8)), std::optional<RoomId>{0});
    EXPECT_EQ(world.roomAt(px(40), px(8)), std::optional<RoomId>{1});
    EXPECT_EQ(world.roomAt(px(15), px(24)), std::optional<RoomId>{2});
    EXPECT_EQ(world.roomAt(px(36), px(21)), std::optional<RoomId>{3});
    // An opening's two sides belong to the two rooms it joins.
    EXPECT_EQ(world.roomAt(px(19) + 7, px(6)), std::optional<RoomId>{0});
    EXPECT_EQ(world.roomAt(px(20), px(6)), std::optional<RoomId>{1});
}

TEST(World, VoidAndOffTheMapAreInNoRoom) {
    const World world = loadWorld();
    EXPECT_EQ(world.roomAt(px(45), px(20)), std::nullopt);
    EXPECT_EQ(world.roomAt(px(59), px(31)), std::nullopt);
    EXPECT_EQ(world.roomAt(-1, 0), std::nullopt);
    EXPECT_EQ(world.roomAt(px(60), 0), std::nullopt);
}

TEST(World, RoomsLieOnTheMapAndNeverOverlap) {
    const World world = loadWorld();
    for (const zorlduh::Room& room : zorlduh::kRooms) {
        EXPECT_GE(room.x, 0);
        EXPECT_GE(room.y, 0);
        EXPECT_LE(room.x + room.width, world.tiles.width);
        EXPECT_LE(room.y + room.height, world.tiles.height);
    }
    for (std::size_t a = 0; a < zorlduh::kRooms.size(); ++a) {
        for (std::size_t b = a + 1; b < zorlduh::kRooms.size(); ++b) {
            const zorlduh::Room& p = zorlduh::kRooms[a];
            const zorlduh::Room& q = zorlduh::kRooms[b];
            const bool apart = p.x + p.width <= q.x || q.x + q.width <= p.x ||
                               p.y + p.height <= q.y || q.y + q.height <= p.y;
            EXPECT_TRUE(apart) << "rooms " << a << " and " << b;
        }
    }
}

// The room table and the map agree: a cell is in a room exactly when the map does not make it void.
TEST(World, EveryCellThatIsNotVoidIsInARoom) {
    const World world = loadWorld();
    for (int cy = 0; cy < world.tiles.height; ++cy) {
        for (int cx = 0; cx < world.tiles.width; ++cx) {
            const bool isVoid = world.tiles.at(cx, cy) == static_cast<std::uint16_t>(WorldCell::Void);
            EXPECT_EQ(world.roomAt(px(cx), px(cy)).has_value(), !isVoid) << cx << "," << cy;
        }
    }
}

// ── The world's tiles ────────────────────────────────────────────────────────────────────────

TEST(WorldTiles, TheCatalogHasOneEntryForEveryValueTheMapHolds) {
    const World world = loadWorld();
    const retropp::TileCatalog catalog = zorlduh::TerrainCatalog(fakeAssets());
    std::set<std::uint16_t> ids;
    for (const retropp::TileCatalogEntry& entry : catalog.entries) {
        EXPECT_TRUE(ids.insert(entry.id).second) << "duplicate id " << entry.id;
    }
    for (const std::uint16_t value : world.tiles.values) {
        EXPECT_TRUE(ids.contains(value)) << value;
    }
}

TEST(WorldTiles, IsOneCellPerMapCell) {
    const World world = loadWorld();
    EXPECT_EQ(zorlduh::WorldTiles(world, fakeAssets()).size(),
              static_cast<std::size_t>(world.tiles.width) * static_cast<std::size_t>(world.tiles.height));
}

TEST(WorldTiles, DrawsEachKindOfCellFromItsTileThroughItsPalette) {
    const World world = loadWorld();
    const zorlduh::Assets assets = fakeAssets();
    const layers::Cells cells = zorlduh::WorldTiles(world, assets);
    const auto at = [&](int cx, int cy) {
        return cells[static_cast<std::size_t>(cy) * static_cast<std::size_t>(world.tiles.width) +
                     static_cast<std::size_t>(cx)];
    };
    const auto expectCell = [&](int cx, int cy, TerrainTile tile, retropp::PaletteId palette) {
        const retropp::TileCell cell = at(cx, cy);
        EXPECT_EQ(cell.atlas, assets.terrain.atlasId) << cx << "," << cy;
        EXPECT_EQ(cell.tile, slot(tile)) << cx << "," << cy;
        EXPECT_EQ(cell.palette, palette) << cx << "," << cy;
        EXPECT_FALSE(cell.flipX);
        EXPECT_FALSE(cell.flipY);
    };
    expectCell(5, 5, TerrainTile::Floor, assets.palettes.floor);
    expectCell(0, 0, TerrainTile::Wall, assets.palettes.wall);
    expectCell(40, 7, TerrainTile::Water, assets.palettes.water);
    expectCell(19, 6, TerrainTile::Doorway, assets.palettes.floor);
    expectCell(45, 20, TerrainTile::Void, assets.palettes.voidCells);
}

// ── Screen layout ────────────────────────────────────────────────────────────────────────────

TEST(Layout, TheHudStripAndThePlayAreaFillTheViewport) {
    const retropp::EngineConfig config = zorlduh::engineConfig();
    EXPECT_EQ(zorlduh::kPlayAreaX, 0);
    EXPECT_EQ(zorlduh::kPlayAreaY, zorlduh::kHudHeight);
    EXPECT_EQ(zorlduh::kPlayAreaWidth, static_cast<int>(config.viewport.width));
    EXPECT_EQ(zorlduh::kHudHeight + zorlduh::kPlayAreaHeight, static_cast<int>(config.viewport.height));
    EXPECT_EQ(zorlduh::kPlayAreaWidth / kCellSize, 20);
    EXPECT_EQ(zorlduh::kPlayAreaHeight / kCellSize, 16);
}

TEST(Layout, TheStartRoomIsExactlyOnePlayArea) {
    const zorlduh::Room& start = zorlduh::kRooms[zorlduh::kStartRoom];
    EXPECT_EQ(start.width * kCellSize, zorlduh::kPlayAreaWidth);
    EXPECT_EQ(start.height * kCellSize, zorlduh::kPlayAreaHeight);
}

}  // namespace
