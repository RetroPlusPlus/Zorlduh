#include "assets/assets.h"

#include <chrono>
#include <cstddef>

#include "retropp/animation.h"
#include "retropp/asset_policy.h"
#include "retropp/geometry.h"
#include "retropp/renderer.h"
#include "retropp/timing.h"

namespace zorlduh {

Assets loadAssets(retropp::Renderer& renderer) {
    using retropp::AssetDimensions;
    using retropp::AssetPolicy;
    using retropp::ContentKind;
    using retropp::ReadOrder;
    using retropp::TransparentIndices;

    constexpr AssetDimensions kCell{.width = 8, .height = 8};
    constexpr AssetDimensions kCharacter{.width = 16, .height = 16};
    constexpr AssetDimensions kSwordFrame{.width = 8, .height = 16};
    constexpr int kWalkFrames = 2;
    constexpr TransparentIndices kIndexZeroHole = TransparentIndices::of({0});

    Assets assets;

    assets.terrain = renderer.loadAtlas("assets/gfx/tiles/terrain.png", kCell,
                                        ContentKind::Tileset, ReadOrder::LeftRightThenDown,
                                        /*count=*/0, TransparentIndices::None,
                                        /*framesPerAnimation=*/0, AssetPolicy::Embed);
    assets.forest = renderer.loadAtlas("assets/gfx/tiles/forest.png", kCell,
                                       ContentKind::Tileset, ReadOrder::LeftRightThenDown,
                                       /*count=*/0, TransparentIndices::None,
                                       /*framesPerAnimation=*/0, AssetPolicy::Embed);
    assets.overlay = renderer.loadAtlas("assets/gfx/tiles/overlay.png", kCell,
                                        ContentKind::Tileset, ReadOrder::LeftRightThenDown,
                                        /*count=*/0, kIndexZeroHole, /*framesPerAnimation=*/0,
                                        AssetPolicy::Embed);
    assets.player = renderer.loadAtlas("assets/gfx/sprites/player.png", kCharacter,
                                       ContentKind::AnimationSeries, ReadOrder::LeftRightThenDown,
                                       /*count=*/0, kIndexZeroHole, kWalkFrames, AssetPolicy::Embed);
    assets.enemy = renderer.loadAtlas("assets/gfx/sprites/enemy.png", kCharacter,
                                      ContentKind::SingleAnimation, ReadOrder::LeftRightThenDown,
                                      /*count=*/0, kIndexZeroHole, /*framesPerAnimation=*/0,
                                      AssetPolicy::Embed);
    assets.sword = renderer.loadAtlas("assets/gfx/sprites/sword.png", kSwordFrame,
                                      ContentKind::Single, ReadOrder::LeftRightThenDown,
                                      /*count=*/0, kIndexZeroHole, /*framesPerAnimation=*/0,
                                      AssetPolicy::Embed);
    assets.hearts = renderer.loadAtlas("assets/gfx/hud/hearts.png", kCell,
                                       ContentKind::SpriteSeries, ReadOrder::LeftRightThenDown,
                                       /*count=*/0, kIndexZeroHole, /*framesPerAnimation=*/0,
                                       AssetPolicy::Embed);

    // The build bakes the first asset path in each statement, so every loader call is a statement of
    // its own.
    assets.palettes.floor = renderer.loadPaletteImage("assets/palettes/tiles/floor.png");
    assets.palettes.wall = renderer.loadPaletteImage("assets/palettes/tiles/wall.png");
    assets.palettes.water = renderer.loadPaletteImage("assets/palettes/tiles/water.png");
    assets.palettes.grass = renderer.loadPaletteImage("assets/palettes/tiles/grass.png");
    assets.palettes.leaf = renderer.loadPaletteImage("assets/palettes/tiles/leaf.png");
    assets.palettes.canopy = renderer.loadPaletteImage("assets/palettes/tiles/canopy.png");
    assets.palettes.player = renderer.loadPaletteImage("assets/palettes/sprites/player.png");
    assets.palettes.enemy = renderer.loadPaletteImage("assets/palettes/sprites/enemy.png");
    assets.palettes.sword = renderer.loadPaletteImage("assets/palettes/sprites/sword.png");
    assets.palettes.hearts = renderer.loadPaletteImage("assets/palettes/hud/hearts.png");

    OverworldMaps& overworld = assets.areas.overworld;
    overworld.meadow.tiles = retropp::loadMapPng("assets/maps/overworld/meadow/tiles.png");
    overworld.meadow.collision = retropp::loadMapPng("assets/maps/overworld/meadow/collision.png");
    overworld.meadowOverhang = retropp::loadMapPng("assets/maps/overworld/meadow/overhang.png");
    overworld.lake.tiles = retropp::loadMapPng("assets/maps/overworld/lake/tiles.png");
    overworld.lake.collision = retropp::loadMapPng("assets/maps/overworld/lake/collision.png");
    overworld.field.tiles = retropp::loadMapPng("assets/maps/overworld/field/tiles.png");
    overworld.field.collision = retropp::loadMapPng("assets/maps/overworld/field/collision.png");
    overworld.shrine.tiles = retropp::loadMapPng("assets/maps/overworld/shrine/tiles.png");
    overworld.shrine.collision = retropp::loadMapPng("assets/maps/overworld/shrine/collision.png");
    overworld.cave.tiles = retropp::loadMapPng("assets/maps/overworld/cave/tiles.png");
    overworld.cave.collision = retropp::loadMapPng("assets/maps/overworld/cave/collision.png");

    DungeonOneMaps& dungeon = assets.areas.dungeonOne;
    dungeon.entrance.tiles = retropp::loadMapPng("assets/maps/dungeon-one/entrance/tiles.png");
    dungeon.entrance.collision = retropp::loadMapPng("assets/maps/dungeon-one/entrance/collision.png");
    dungeon.hall.tiles = retropp::loadMapPng("assets/maps/dungeon-one/hall/tiles.png");
    dungeon.hall.collision = retropp::loadMapPng("assets/maps/dungeon-one/hall/collision.png");
    dungeon.westWing.tiles = retropp::loadMapPng("assets/maps/dungeon-one/west-wing/tiles.png");
    dungeon.westWing.collision = retropp::loadMapPng("assets/maps/dungeon-one/west-wing/collision.png");
    dungeon.eastWing.tiles = retropp::loadMapPng("assets/maps/dungeon-one/east-wing/tiles.png");
    dungeon.eastWing.collision = retropp::loadMapPng("assets/maps/dungeon-one/east-wing/collision.png");
    dungeon.treasury.tiles = retropp::loadMapPng("assets/maps/dungeon-one/treasury/tiles.png");
    dungeon.treasury.collision = retropp::loadMapPng("assets/maps/dungeon-one/treasury/collision.png");

    SeamlessForestMaps& forest = assets.areas.seamlessForest;
    forest.edge.tiles = retropp::loadMapPng("assets/maps/seamless-forest/edge/tiles.png");
    forest.edge.collision = retropp::loadMapPng("assets/maps/seamless-forest/edge/collision.png");
    forest.edgePath = retropp::loadMapPng("assets/maps/seamless-forest/edge/path.png");
    forest.glade.tiles = retropp::loadMapPng("assets/maps/seamless-forest/glade/tiles.png");
    forest.glade.collision = retropp::loadMapPng("assets/maps/seamless-forest/glade/collision.png");
    forest.gladeCanopy = retropp::loadMapPng("assets/maps/seamless-forest/glade/canopy.png");
    forest.thicket.tiles = retropp::loadMapPng("assets/maps/seamless-forest/thicket/tiles.png");
    forest.thicket.collision = retropp::loadMapPng("assets/maps/seamless-forest/thicket/collision.png");
    forest.bower.tiles = retropp::loadMapPng("assets/maps/seamless-forest/bower/tiles.png");
    forest.bower.collision = retropp::loadMapPng("assets/maps/seamless-forest/bower/collision.png");
    forest.hollow.tiles = retropp::loadMapPng("assets/maps/seamless-forest/hollow/tiles.png");
    forest.hollow.collision = retropp::loadMapPng("assets/maps/seamless-forest/hollow/collision.png");
    forest.brook.tiles = retropp::loadMapPng("assets/maps/seamless-forest/brook/tiles.png");
    forest.brook.collision = retropp::loadMapPng("assets/maps/seamless-forest/brook/collision.png");
    forest.clearing.tiles = retropp::loadMapPng("assets/maps/seamless-forest/clearing/tiles.png");
    forest.clearing.collision = retropp::loadMapPng("assets/maps/seamless-forest/clearing/collision.png");

    // Each facing walks through its own row of the player sheet: two frames, 8 ticks each.
    constexpr auto kWalkFrame =
        8 * std::chrono::nanoseconds{
                static_cast<std::chrono::nanoseconds::rep>(retropp::TickPeriodNs::Hz60)};
    const auto walkRow = [&](PlayerWalk row) {
        const std::size_t first = static_cast<std::size_t>(row) * kWalkFrames;
        return retropp::Animation{{
            {.label = "step0", .sheet = assets.player.atlasId, .tileIndex = first,
             .palette = assets.palettes.player, .duration = kWalkFrame},
            {.label = "step1", .sheet = assets.player.atlasId, .tileIndex = first + 1,
             .palette = assets.palettes.player, .duration = kWalkFrame},
        }};
    };
    assets.playerWalk = TopDownMovement::DirectionalAnimations{
        .up = walkRow(PlayerWalk::Up),
        .down = walkRow(PlayerWalk::Down),
        .left = walkRow(PlayerWalk::Left),
        .right = walkRow(PlayerWalk::Right),
    };

    return assets;
}

}  // namespace zorlduh
