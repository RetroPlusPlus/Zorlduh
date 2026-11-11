// The files under assets/: every one is baked into any binary that links the game's library; each
// sheet decodes to the size and carve the game loads it with and holds palette indices only; each
// palette image holds four colors; each area's maps are the same size, hold only world and collision
// ids, are ringed by wall except their doorways, and are solid exactly where the area is wall or water.
// No renderer is needed.

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

// Every area's maps, from the bytes baked into this binary, with the size each is drawn at.
struct AreaFiles {
    const char* name;
    retropp::IndexGrid tiles;
    retropp::IndexGrid collision;
    int width;
    int height;
};

std::vector<AreaFiles> areas() {
    std::vector<AreaFiles> all;
    all.push_back({"meadow", retropp::loadMapPng("assets/maps/overworld/meadow/tiles.png"),
                   retropp::loadMapPng("assets/maps/overworld/meadow/collision.png"), 20, 16});
    all.push_back({"lake", retropp::loadMapPng("assets/maps/overworld/lake/tiles.png"),
                   retropp::loadMapPng("assets/maps/overworld/lake/collision.png"), 40, 16});
    all.push_back({"field", retropp::loadMapPng("assets/maps/overworld/field/tiles.png"),
                   retropp::loadMapPng("assets/maps/overworld/field/collision.png"), 30, 16});
    all.push_back({"shrine", retropp::loadMapPng("assets/maps/overworld/shrine/tiles.png"),
                   retropp::loadMapPng("assets/maps/overworld/shrine/collision.png"), 12, 10});
    all.push_back({"cave", retropp::loadMapPng("assets/maps/overworld/cave/tiles.png"),
                   retropp::loadMapPng("assets/maps/overworld/cave/collision.png"), 12, 10});
    all.push_back({"entrance", retropp::loadMapPng("assets/maps/dungeon-one/entrance/tiles.png"),
                   retropp::loadMapPng("assets/maps/dungeon-one/entrance/collision.png"), 20, 16});
    all.push_back({"hall", retropp::loadMapPng("assets/maps/dungeon-one/hall/tiles.png"),
                   retropp::loadMapPng("assets/maps/dungeon-one/hall/collision.png"), 20, 16});
    all.push_back({"west-wing", retropp::loadMapPng("assets/maps/dungeon-one/west-wing/tiles.png"),
                   retropp::loadMapPng("assets/maps/dungeon-one/west-wing/collision.png"), 12, 10});
    all.push_back({"east-wing", retropp::loadMapPng("assets/maps/dungeon-one/east-wing/tiles.png"),
                   retropp::loadMapPng("assets/maps/dungeon-one/east-wing/collision.png"), 12, 10});
    all.push_back({"treasury", retropp::loadMapPng("assets/maps/dungeon-one/treasury/tiles.png"),
                   retropp::loadMapPng("assets/maps/dungeon-one/treasury/collision.png"), 30, 16});
    all.push_back({"edge", retropp::loadMapPng("assets/maps/seamless-forest/edge/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/edge/collision.png"), 20, 16});
    all.push_back({"glade", retropp::loadMapPng("assets/maps/seamless-forest/glade/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/glade/collision.png"), 30, 20});
    all.push_back({"thicket", retropp::loadMapPng("assets/maps/seamless-forest/thicket/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/thicket/collision.png"), 12, 10});
    all.push_back({"bower", retropp::loadMapPng("assets/maps/seamless-forest/bower/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/bower/collision.png"), 16, 10});
    all.push_back({"hollow", retropp::loadMapPng("assets/maps/seamless-forest/hollow/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/hollow/collision.png"), 20, 8});
    all.push_back({"brook", retropp::loadMapPng("assets/maps/seamless-forest/brook/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/brook/collision.png"), 20, 8});
    all.push_back({"clearing", retropp::loadMapPng("assets/maps/seamless-forest/clearing/tiles.png"),
                   retropp::loadMapPng("assets/maps/seamless-forest/clearing/collision.png"), 20, 16});
    return all;
}

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
    EXPECT_EQ(files, 49);
}

// ── Sheets ───────────────────────────────────────────────────────────────────────────────────

TEST(Assets, TerrainSheetIsOneTilePerWorldCell) {
    const LoadedImage sheet = decode("gfx/tiles/terrain.png");
    EXPECT_EQ(sheet.width, 40);
    EXPECT_EQ(sheet.height, 8);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 8, .height = 8}, ContentKind::Tileset),
              static_cast<std::size_t>(zorlduh::TerrainTile::Stairs) + 1);
}

TEST(Assets, ForestSheetIsOneTilePerForestTile) {
    const LoadedImage sheet = decode("gfx/tiles/forest.png");
    EXPECT_EQ(sheet.width, 32);
    EXPECT_EQ(sheet.height, 8);
    expectIndicesOnly(sheet);
    EXPECT_EQ(slotCount(sheet, {.width = 8, .height = 8}, ContentKind::Tileset),
              static_cast<std::size_t>(zorlduh::ForestTile::Trodden) + 1);
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
    for (const char* path : {"palettes/tiles/floor.png", "palettes/tiles/wall.png", "palettes/tiles/water.png",
                             "palettes/tiles/grass.png", "palettes/tiles/leaf.png"}) {
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

TEST(Assets, EachAreasMapsAreItsSizeAndHoldOnlyWorldCells) {
    const std::vector<std::uint16_t> ids = {value(WorldCell::Stairs), value(WorldCell::Wall),
                                            value(WorldCell::Water), value(WorldCell::Doorway),
                                            value(WorldCell::Floor)};
    for (const AreaFiles& area : areas()) {
        EXPECT_EQ(area.tiles.width, area.width) << area.name;
        EXPECT_EQ(area.tiles.height, area.height) << area.name;
        EXPECT_EQ(area.collision.width, area.width) << area.name;
        EXPECT_EQ(area.collision.height, area.height) << area.name;
        for (const std::uint16_t cell : area.tiles.values) {
            EXPECT_NE(std::find(ids.begin(), ids.end(), cell), ids.end()) << area.name << " " << cell;
        }
    }
}

TEST(Assets, EachAreaIsRingedByWallExceptItsDoorways) {
    for (const AreaFiles& area : areas()) {
        for (int y = 0; y < area.height; ++y) {
            for (int x = 0; x < area.width; ++x) {
                const bool border = x == 0 || x == area.width - 1 || y == 0 || y == area.height - 1;
                const std::uint16_t cell = area.tiles.at(x, y);
                if (border) {
                    EXPECT_TRUE(cell == value(WorldCell::Wall) || cell == value(WorldCell::Doorway))
                        << area.name << " " << x << "," << y;
                } else {
                    EXPECT_NE(cell, value(WorldCell::Doorway)) << area.name << " " << x << "," << y;
                }
            }
        }
    }
}

TEST(Assets, TheMeadowHasItsDoorsRoadAndCaveMouthAndTheLakeItsPondAndDungeonDoor) {
    const std::vector<AreaFiles> all = areas();
    const retropp::IndexGrid& meadow = all[0].tiles;
    for (int y = 6; y <= 9; ++y) {
        EXPECT_EQ(meadow.at(19, y), value(WorldCell::Doorway));   // the east door
        EXPECT_EQ(all[1].tiles.at(0, y), value(WorldCell::Doorway));   // the lake's west door
    }
    EXPECT_EQ(meadow.at(19, 5), value(WorldCell::Wall));
    EXPECT_EQ(meadow.at(19, 10), value(WorldCell::Wall));
    for (int x = 8; x <= 11; ++x) {
        EXPECT_EQ(meadow.at(x, 15), value(WorldCell::Doorway));   // the south road
        EXPECT_EQ(all[2].tiles.at(x, 0), value(WorldCell::Doorway));   // the field's north road
    }
    EXPECT_EQ(meadow.at(12, 4), value(WorldCell::Stairs));   // the cave mouth
    EXPECT_EQ(meadow.at(13, 4), value(WorldCell::Stairs));
    EXPECT_EQ(meadow.at(5, 5), value(WorldCell::Floor));
    EXPECT_EQ(all[1].tiles.at(20, 7), value(WorldCell::Water));   // the pond
    EXPECT_EQ(all[1].tiles.at(30, 2), value(WorldCell::Stairs));  // the dungeon door
    EXPECT_EQ(all[1].tiles.at(31, 2), value(WorldCell::Stairs));
    EXPECT_EQ(all[1].tiles.at(29, 2), value(WorldCell::Floor));
    EXPECT_EQ(all[4].tiles.at(5, 4), value(WorldCell::Stairs));   // the cave's way back up
}

TEST(Assets, TheDungeonsRoomsHaveTheirDoorsAndOpenWalls) {
    const std::vector<AreaFiles> all = areas();
    const retropp::IndexGrid& entrance = all[5].tiles;
    const retropp::IndexGrid& hall = all[6].tiles;
    const retropp::IndexGrid& treasury = all[9].tiles;
    EXPECT_EQ(entrance.at(9, 13), value(WorldCell::Stairs));    // the door back up to the lake
    EXPECT_EQ(entrance.at(10, 13), value(WorldCell::Stairs));
    for (int x = 8; x <= 11; ++x) {
        EXPECT_EQ(entrance.at(x, 0), value(WorldCell::Doorway));   // the entrance's north door
        EXPECT_EQ(hall.at(x, 15), value(WorldCell::Doorway));      // the hall's south door
    }
    for (int y = 6; y <= 9; ++y) {
        EXPECT_EQ(hall.at(0, y), value(WorldCell::Doorway));       // the hall's west door
        EXPECT_EQ(hall.at(19, y), value(WorldCell::Doorway));      // and east door
    }
    for (int y = 3; y <= 6; ++y) {
        EXPECT_EQ(all[7].tiles.at(11, y), value(WorldCell::Doorway));   // the west wing's east door
        EXPECT_EQ(all[8].tiles.at(0, y), value(WorldCell::Doorway));    // the east wing's west door
    }
    for (int x = 1; x <= 18; ++x) {
        EXPECT_EQ(hall.at(x, 0), value(WorldCell::Doorway));           // the hall's open north wall
        EXPECT_EQ(treasury.at(x + 5, 15), value(WorldCell::Doorway));  // the treasury's open south wall, centered
    }
    EXPECT_EQ(hall.at(0, 0), value(WorldCell::Wall));
    EXPECT_EQ(hall.at(19, 0), value(WorldCell::Wall));
    EXPECT_EQ(treasury.at(5, 15), value(WorldCell::Wall));
    EXPECT_EQ(treasury.at(24, 15), value(WorldCell::Wall));
}

// The field's forest path, and each forest room's open walls: whole edges where the hollow and the brook
// meet, a span short of the corners everywhere else.
TEST(Assets, TheForestsRoomsAreOpenAlongTheirSeamsAndTheFieldHasItsForestPath) {
    const std::vector<AreaFiles> all = areas();
    const retropp::IndexGrid& field = all[2].tiles;
    for (int x = 13; x <= 16; ++x) {
        EXPECT_EQ(field.at(x, 15), value(WorldCell::Doorway));   // the forest path
    }
    EXPECT_EQ(field.at(12, 15), value(WorldCell::Wall));
    EXPECT_EQ(field.at(17, 15), value(WorldCell::Wall));

    const retropp::IndexGrid& edge = all[10].tiles;
    const retropp::IndexGrid& glade = all[11].tiles;
    const retropp::IndexGrid& thicket = all[12].tiles;
    const retropp::IndexGrid& bower = all[13].tiles;
    const retropp::IndexGrid& hollow = all[14].tiles;
    const retropp::IndexGrid& brook = all[15].tiles;
    const retropp::IndexGrid& clearing = all[16].tiles;
    const auto open = [](const retropp::IndexGrid& map, int x, int y) { return map.at(x, y) == value(WorldCell::Doorway); };
    for (int x = 8; x <= 11; ++x) {
        EXPECT_TRUE(open(edge, x, 0)) << x;   // the path back up to the field
    }
    for (int y = 1; y <= 14; ++y) {
        EXPECT_TRUE(open(edge, 19, y)) << y;
    }
    for (int y = 3; y <= 16; ++y) {
        EXPECT_TRUE(open(glade, 0, y)) << y;
    }
    for (int x = 12; x <= 21; ++x) {
        EXPECT_TRUE(open(glade, x, 0)) << x;
    }
    for (int y = 8; y <= 15; ++y) {
        EXPECT_TRUE(open(glade, 29, y)) << y;
    }
    for (int x = 5; x <= 22; ++x) {
        EXPECT_TRUE(open(glade, x, 19)) << x;
    }
    for (int x = 1; x <= 10; ++x) {
        EXPECT_TRUE(open(thicket, x, 9)) << x;
    }
    for (int y = 1; y <= 8; ++y) {
        EXPECT_TRUE(open(thicket, 11, y)) << y;
        EXPECT_TRUE(open(bower, 0, y)) << y;
    }
    for (int y = 0; y <= 7; ++y) {
        EXPECT_TRUE(open(hollow, 0, y)) << y;   // whole edges, corners included
        EXPECT_TRUE(open(hollow, 19, y)) << y;
        EXPECT_TRUE(open(brook, 0, y)) << y;
    }
    for (int x = 1; x <= 18; ++x) {
        EXPECT_TRUE(open(clearing, x, 0)) << x;
    }
    EXPECT_EQ(edge.at(19, 0), value(WorldCell::Wall));
    EXPECT_EQ(edge.at(19, 15), value(WorldCell::Wall));
    EXPECT_EQ(glade.at(0, 0), value(WorldCell::Wall));
    EXPECT_EQ(glade.at(29, 19), value(WorldCell::Wall));
    EXPECT_EQ(thicket.at(11, 9), value(WorldCell::Wall));
    EXPECT_EQ(thicket.at(0, 9), value(WorldCell::Wall));
    EXPECT_EQ(bower.at(0, 9), value(WorldCell::Wall));
    EXPECT_EQ(clearing.at(0, 0), value(WorldCell::Wall));
    EXPECT_EQ(clearing.at(19, 0), value(WorldCell::Wall));
    EXPECT_EQ(brook.at(19, 0), value(WorldCell::Wall));
}

TEST(Assets, CollisionIsSolidExactlyWhereTheAreaIsWallOrWater) {
    for (const AreaFiles& area : areas()) {
        for (int y = 0; y < area.height; ++y) {
            for (int x = 0; x < area.width; ++x) {
                const std::uint16_t cell = area.tiles.at(x, y);
                const bool solid = cell == value(WorldCell::Wall) || cell == value(WorldCell::Water);
                EXPECT_EQ(area.collision.at(x, y), value(solid ? CollisionCell::Solid : CollisionCell::Open))
                    << area.name << " " << x << "," << y;
            }
        }
    }
}

}  // namespace
