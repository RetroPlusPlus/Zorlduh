// The files under assets/: every one is baked into any binary that links the game's library; each
// sheet decodes to the size and carve the game loads it with and holds palette indices only; each
// palette image holds four colors; the maps hold the world and collision ids, rooms ringed by wall with
// their openings, and solid exactly where the world is wall, water or void. No renderer is needed.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include "retropp/asset_registry.h"
#include "retropp/atlas_manifest.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/palette.h"

#include "assets/assets.h"

namespace {

using retropp::AssetDimensions;
using retropp::ContentKind;
using retropp::ImageColorKind;
using retropp::LoadedImage;
using retropp::ReadOrder;
using zorlduh::CollisionCell;
using zorlduh::WorldCell;

LoadedImage decode(const std::string& relative) {
    return retropp::loadPng(std::string(ZORLDUH_ASSET_DIR) + "/" + relative);
}

std::size_t slotCount(const LoadedImage& sheet, AssetDimensions asset, ContentKind kind) {
    return retropp::sliceLayout({.width = sheet.width, .height = sheet.height}, asset, kind,
                                ReadOrder::LeftRightThenDown)
        .size();
}

// A sheet is palette indices 0-3 with no color of its own.
void expectIndicesOnly(const LoadedImage& sheet) {
    EXPECT_EQ(sheet.kind, ImageColorKind::Indexed);
    EXPECT_TRUE(sheet.palette.empty());
    ASSERT_FALSE(sheet.indices.empty());
    EXPECT_LE(*std::max_element(sheet.indices.begin(), sheet.indices.end()), 3);
}

std::uint16_t value(WorldCell cell) {
    return static_cast<std::uint16_t>(cell);
}

std::uint16_t value(CollisionCell cell) {
    return static_cast<std::uint16_t>(cell);
}

struct Room {
    int x;
    int y;
    int width;
    int height;
};

constexpr Room kRooms[] = {
    {.x = 0, .y = 0, .width = 20, .height = 16},
    {.x = 20, .y = 0, .width = 40, .height = 16},
    {.x = 0, .y = 16, .width = 30, .height = 16},
    {.x = 30, .y = 16, .width = 12, .height = 10},
};

// ── Baked into the binary ────────────────────────────────────────────────────────────────────

// loadAssets loads every file with the Embed policy, so each must be in the build's embedded set under
// its logical path, with the bytes of the file on disk.
TEST(Assets, EveryFileIsBakedIntoTheBinary) {
    const std::filesystem::path root(ZORLDUH_ASSET_DIR);
    int files = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root)) {
        if (!entry.is_regular_file() || entry.path().filename().string().starts_with(".")) {
            continue;
        }
        ++files;
        const std::string logical = "assets/" + entry.path().lexically_relative(root).generic_string();
        std::ifstream in(entry.path(), std::ios::binary);
        const std::vector<std::uint8_t> onDisk{std::istreambuf_iterator<char>(in), {}};
        const std::span<const std::uint8_t> baked = retropp::detail::findEmbeddedAsset(logical);
        EXPECT_TRUE(std::equal(baked.begin(), baked.end(), onDisk.begin(), onDisk.end())) << logical;
    }
    EXPECT_EQ(files, 15);
}

// ── Sheets ───────────────────────────────────────────────────────────────────────────────────

TEST(Assets, TerrainSheetIsOneTilePerWorldCell) {
    const LoadedImage sheet = decode("gfx/tiles/terrain.png");
    EXPECT_EQ(sheet.width, 40);
    EXPECT_EQ(sheet.height, 8);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 8, .height = 8}, ContentKind::Tileset),
              static_cast<std::size_t>(zorlduh::TerrainTile::Void) + 1);
}

TEST(Assets, PlayerSheetIsFourFacingsOfTwoWalkFrames) {
    const LoadedImage sheet = decode("gfx/sprites/player.png");
    EXPECT_EQ(sheet.width, 32);
    EXPECT_EQ(sheet.height, 64);
    expectIndicesOnly(sheet);
    const retropp::AtlasManifest manifest{
        .slots = retropp::sliceLayout({.width = sheet.width, .height = sheet.height},
                                      {.width = 16, .height = 16}, ContentKind::AnimationSeries,
                                      ReadOrder::LeftRightThenDown),
        .framesPerAnimation = 2,
        .kind = ContentKind::AnimationSeries,
    };
    EXPECT_EQ(manifest.animationCount(), static_cast<std::size_t>(zorlduh::PlayerWalk::Right) + 1);
}

TEST(Assets, EnemySheetIsTwoFrames) {
    const LoadedImage sheet = decode("gfx/sprites/enemy.png");
    EXPECT_EQ(sheet.width, 32);
    EXPECT_EQ(sheet.height, 16);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 16, .height = 16}, ContentKind::SingleAnimation), 2u);
}

TEST(Assets, SwordSheetIsOneUprightFrame) {
    const LoadedImage sheet = decode("gfx/sprites/sword.png");
    EXPECT_EQ(sheet.width, 8);
    EXPECT_EQ(sheet.height, 16);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 8, .height = 16}, ContentKind::Single), 1u);
}

TEST(Assets, HeartsSheetIsFullHalfAndEmpty) {
    const LoadedImage sheet = decode("gfx/hud/hearts.png");
    EXPECT_EQ(sheet.width, 24);
    EXPECT_EQ(sheet.height, 8);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 8, .height = 8}, ContentKind::SpriteSeries),
              static_cast<std::size_t>(zorlduh::Heart::Empty) + 1);
}

TEST(Assets, SpriteSheetsAreTransparentAtTheirCorners) {
    for (const char* path : {"gfx/sprites/player.png", "gfx/sprites/enemy.png", "gfx/sprites/sword.png",
                             "gfx/hud/hearts.png"}) {
        const LoadedImage sheet = decode(path);
        EXPECT_EQ(sheet.indices.front(), 0) << path;
        EXPECT_EQ(sheet.indices.back(), 0) << path;
    }
}

// ── Palettes ─────────────────────────────────────────────────────────────────────────────────

TEST(Assets, TerrainPalettesAreFourOpaqueColors) {
    for (const char* path : {"palettes/tiles/floor.png", "palettes/tiles/wall.png",
                             "palettes/tiles/water.png", "palettes/tiles/void.png"}) {
        const std::vector<retropp::Rgba16> entries = retropp::slicePaletteImage(decode(path));
        ASSERT_EQ(entries.size(), 4u) << path;
        for (const retropp::Rgba16& entry : entries) {
            EXPECT_EQ(entry.a, 65535) << path;
        }
    }
}

TEST(Assets, SpritePalettesLeaveEntryZeroUndrawn) {
    for (const char* path : {"palettes/sprites/player.png", "palettes/sprites/enemy.png",
                             "palettes/sprites/sword.png", "palettes/hud/hearts.png"}) {
        const std::vector<retropp::Rgba16> entries = retropp::slicePaletteImage(decode(path));
        ASSERT_EQ(entries.size(), 4u) << path;
        EXPECT_EQ(entries[0].a, 0) << path;
        for (std::size_t i = 1; i < entries.size(); ++i) {
            EXPECT_EQ(entries[i].a, 65535) << path;
        }
    }
}

// ── Maps ─────────────────────────────────────────────────────────────────────────────────────

TEST(Assets, WorldMapHoldsOnlyWorldCells) {
    const retropp::IndexGrid world = retropp::loadMapPng("assets/maps/world/tiles.png");
    EXPECT_EQ(world.width, 60);
    EXPECT_EQ(world.height, 32);
    const std::vector<std::uint16_t> ids = {value(WorldCell::Void), value(WorldCell::Wall),
                                            value(WorldCell::Water), value(WorldCell::Doorway),
                                            value(WorldCell::Floor)};
    for (const std::uint16_t cell : world.values) {
        EXPECT_NE(std::find(ids.begin(), ids.end(), cell), ids.end()) << cell;
    }
}

TEST(Assets, RoomsAreRingedByWallExceptTheirOpenings) {
    const retropp::IndexGrid world = retropp::loadMapPng("assets/maps/world/tiles.png");
    for (const Room& room : kRooms) {
        for (int y = room.y; y < room.y + room.height; ++y) {
            for (int x = room.x; x < room.x + room.width; ++x) {
                const bool border = x == room.x || x == room.x + room.width - 1 || y == room.y ||
                                    y == room.y + room.height - 1;
                const std::uint16_t cell = world.at(x, y);
                if (border) {
                    EXPECT_TRUE(cell == value(WorldCell::Wall) || cell == value(WorldCell::Doorway))
                        << x << "," << y;
                } else {
                    EXPECT_NE(cell, value(WorldCell::Void)) << x << "," << y;
                }
            }
        }
    }
}

TEST(Assets, OpeningsAreDoorwaysOnBothSidesOfTheSharedEdge) {
    const retropp::IndexGrid world = retropp::loadMapPng("assets/maps/world/tiles.png");
    for (int y = 6; y <= 9; ++y) {  // room 0 ↔ room 1
        EXPECT_EQ(world.at(19, y), value(WorldCell::Doorway));
        EXPECT_EQ(world.at(20, y), value(WorldCell::Doorway));
    }
    for (int x = 8; x <= 11; ++x) {  // room 0 ↔ room 2
        EXPECT_EQ(world.at(x, 15), value(WorldCell::Doorway));
        EXPECT_EQ(world.at(x, 16), value(WorldCell::Doorway));
    }
    for (int y = 19; y <= 22; ++y) {  // room 2 ↔ room 3
        EXPECT_EQ(world.at(29, y), value(WorldCell::Doorway));
        EXPECT_EQ(world.at(30, y), value(WorldCell::Doorway));
    }
    EXPECT_EQ(world.at(19, 5), value(WorldCell::Wall));
    EXPECT_EQ(world.at(19, 10), value(WorldCell::Wall));
}

TEST(Assets, CellsOutsideEveryRoomAreVoidAndRoomOneHasAPond) {
    const retropp::IndexGrid world = retropp::loadMapPng("assets/maps/world/tiles.png");
    EXPECT_EQ(world.at(59, 31), value(WorldCell::Void));
    EXPECT_EQ(world.at(45, 20), value(WorldCell::Void));
    EXPECT_EQ(world.at(35, 28), value(WorldCell::Void));
    EXPECT_EQ(world.at(40, 7), value(WorldCell::Water));
    EXPECT_EQ(world.at(5, 5), value(WorldCell::Floor));
}

TEST(Assets, CollisionIsSolidExactlyWhereTheWorldIsWallWaterOrVoid) {
    const retropp::IndexGrid world = retropp::loadMapPng("assets/maps/world/tiles.png");
    const retropp::IndexGrid collision = retropp::loadMapPng("assets/maps/world/collision.png");
    ASSERT_EQ(collision.width, world.width);
    ASSERT_EQ(collision.height, world.height);
    for (int y = 0; y < world.height; ++y) {
        for (int x = 0; x < world.width; ++x) {
            const std::uint16_t cell = world.at(x, y);
            const bool solid = cell == value(WorldCell::Wall) || cell == value(WorldCell::Water) ||
                               cell == value(WorldCell::Void);
            EXPECT_EQ(collision.at(x, y), value(solid ? CollisionCell::Solid : CollisionCell::Open))
                << x << "," << y;
        }
    }
}

}  // namespace
