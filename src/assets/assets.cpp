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
    constexpr TransparentIndices kSpriteHole = TransparentIndices::of({0});

    Assets assets;

    assets.terrain = renderer.loadAtlas("assets/gfx/tiles/terrain.png", kCell,
                                        ContentKind::Tileset, ReadOrder::LeftRightThenDown,
                                        /*count=*/0, TransparentIndices::None,
                                        /*framesPerAnimation=*/0, AssetPolicy::Embed);
    assets.player = renderer.loadAtlas("assets/gfx/sprites/player.png", kCharacter,
                                       ContentKind::AnimationSeries, ReadOrder::LeftRightThenDown,
                                       /*count=*/0, kSpriteHole, kWalkFrames, AssetPolicy::Embed);
    assets.enemy = renderer.loadAtlas("assets/gfx/sprites/enemy.png", kCharacter,
                                      ContentKind::SingleAnimation, ReadOrder::LeftRightThenDown,
                                      /*count=*/0, kSpriteHole, /*framesPerAnimation=*/0,
                                      AssetPolicy::Embed);
    assets.sword = renderer.loadAtlas("assets/gfx/sprites/sword.png", kSwordFrame,
                                      ContentKind::Single, ReadOrder::LeftRightThenDown,
                                      /*count=*/0, kSpriteHole, /*framesPerAnimation=*/0,
                                      AssetPolicy::Embed);
    assets.hearts = renderer.loadAtlas("assets/gfx/hud/hearts.png", kCell,
                                       ContentKind::SpriteSeries, ReadOrder::LeftRightThenDown,
                                       /*count=*/0, kSpriteHole, /*framesPerAnimation=*/0,
                                       AssetPolicy::Embed);

    // The build bakes the first asset path in each statement, so every loader call is a statement of
    // its own.
    assets.palettes.floor = renderer.loadPaletteImage("assets/palettes/tiles/floor.png");
    assets.palettes.wall = renderer.loadPaletteImage("assets/palettes/tiles/wall.png");
    assets.palettes.water = renderer.loadPaletteImage("assets/palettes/tiles/water.png");
    assets.palettes.voidCells = renderer.loadPaletteImage("assets/palettes/tiles/void.png");
    assets.palettes.player = renderer.loadPaletteImage("assets/palettes/sprites/player.png");
    assets.palettes.enemy = renderer.loadPaletteImage("assets/palettes/sprites/enemy.png");
    assets.palettes.sword = renderer.loadPaletteImage("assets/palettes/sprites/sword.png");
    assets.palettes.hearts = renderer.loadPaletteImage("assets/palettes/hud/hearts.png");

    assets.world = retropp::loadMapPng("assets/maps/world/tiles.png");
    assets.collision = retropp::loadMapPng("assets/maps/world/collision.png");

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
