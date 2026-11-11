// The player: its walker config, where it starts, walking the real world map through the tick, the
// controls read into Input, and the Player component. The config and the component use the game's real
// assets, loaded through a renderer on a GPU device.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/timing.h"

#include "assets/assets.h"
#include "game/areas.h"
#include "game/player.h"
#include "game/sim.h"
#include "input/input.h"
#include "TopDownMovement/walker.h"
#include "render/layout.h"
#include "render/player.h"

namespace {

using retropp::Dir;
using retropp::Vec2i;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

// The real maps, with the sheets and palettes as handles the test makes up. The config's world points
// at the maps, so the assets outlive it.
zorlduh::Assets headlessAssets() {
    zorlduh::Assets assets;
    assets.terrain = retropp::AtlasManifest{
        .atlasId = retropp::AtlasId{7},
        .slots = retropp::sliceLayout({.width = 40, .height = 8}, {.width = 8, .height = 8},
                                      retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown),
        .kind = retropp::ContentKind::Tileset,
    };
    zorlduh::OverworldMaps& overworld = assets.areas.overworld;
    overworld.meadow.tiles = retropp::loadMapPng("assets/maps/overworld/meadow/tiles.png");
    overworld.meadow.collision = retropp::loadMapPng("assets/maps/overworld/meadow/collision.png");
    overworld.lake.tiles = retropp::loadMapPng("assets/maps/overworld/lake/tiles.png");
    overworld.lake.collision = retropp::loadMapPng("assets/maps/overworld/lake/collision.png");
    overworld.field.tiles = retropp::loadMapPng("assets/maps/overworld/field/tiles.png");
    overworld.field.collision = retropp::loadMapPng("assets/maps/overworld/field/collision.png");
    overworld.shrine.tiles = retropp::loadMapPng("assets/maps/overworld/shrine/tiles.png");
    overworld.shrine.collision = retropp::loadMapPng("assets/maps/overworld/shrine/collision.png");
    overworld.cave.tiles = retropp::loadMapPng("assets/maps/overworld/cave/tiles.png");
    overworld.cave.collision = retropp::loadMapPng("assets/maps/overworld/cave/collision.png");
    zorlduh::DungeonOneMaps& dungeon = assets.areas.dungeonOne;
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
    return assets;
}

// The game as gameConfig makes it.
zorlduh::Config headlessConfig(const zorlduh::Assets& assets) {
    zorlduh::Config config = zorlduh::gameConfig(assets, zorlduh::kView);
    config.player.timing = kHz60;
    return config;
}

zorlduh::GameState start(const zorlduh::Config& config) {
    zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    state.camera.place(config.world, state.player.collision(config.player).bounds());
    return state;
}

TopDownMovement::DirectionalInput Hold(TopDownMovement::Directions held) {
    return TopDownMovement::DirectionalInput{.held = held, .pressed = held};
}

// ── Where it starts and how it walks the world ───────────────────────────────────────────────

TEST(Player, StartsCenteredInTheMeadowClearOfEverySolidCell) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    const zorlduh::GameState state = start(config);
    EXPECT_EQ(zorlduh::kPlayerStart, (Vec2i{72, 56}));
    EXPECT_EQ(state.camera.layout(), zorlduh::Layout::Overworld);
    EXPECT_EQ(state.camera.area(), zorlduh::Area::Meadow);
    EXPECT_FALSE(config.player.collision.hits(state.camera.solids(config.world), zorlduh::kPlayerStart));
}

TEST(Player, CollidesWithTheBottomHalfOfItsFrame) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    EXPECT_EQ(config.player.speed, 1);
    EXPECT_EQ(config.player.collision.bounds(), (retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8}));
}

TEST(Player, TheTickWalksItUntilItsCollisionBoxMeetsTheWall) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    for (int i = 0; i < 100; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.left = true})}, config);
    }
    // The meadow's west wall is column 0, pixels 0–7; the box starts 3 px into the frame.
    EXPECT_EQ(state.player.position(), (Vec2i{5, 56}));
    EXPECT_EQ(state.player.facing(), Dir::Left);
    for (int i = 0; i < 100; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.up = true})}, config);
    }
    // Its north wall is row 0; the box starts 8 px down the frame.
    EXPECT_EQ(state.player.position(), (Vec2i{5, 0}));
    EXPECT_TRUE(state.events.empty());
}

TEST(Player, KeepsWalkingAndAnimatingThroughTicksThatMoveItNowhereElse) {
    const zorlduh::Assets assets = headlessAssets();
    const zorlduh::Config config = headlessConfig(assets);
    zorlduh::GameState state = start(config);
    for (int i = 0; i < 12; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.down = true})}, config);
    }
    // The stride is not reset by the camera's empty move: twelve ticks in, the second walk frame shows.
    EXPECT_TRUE(state.player.moving());
    EXPECT_EQ(state.player.position(), (Vec2i{72, 68}));
}

// ── The controls ─────────────────────────────────────────────────────────────────────────────

class ManualClock final : public retropp::Clock {
public:
    [[nodiscard]] std::chrono::nanoseconds now() const noexcept override { return now_; }
    void advance(std::chrono::nanoseconds by) noexcept { now_ += by; }

private:
    std::chrono::nanoseconds now_{};
};

TEST(Controls, TheDirectionActionsMoveThePlayer) {
    ManualClock clock;
    retropp::RunLoop loop{clock, kHz60};
    std::optional<zorlduh::Input> read;
    loop.simTick([&](const retropp::InputState& input) { read = zorlduh::readInput(input); });
    loop.renderLoop([] {});
    loop.advance();

    retropp::InputSample sample;
    sample.players[0].held.set(retropp::actionId(zorlduh::Action::Down), true);
    sample.players[0].held.set(retropp::actionId(zorlduh::Action::Right), true);
    sample.players[0].held.set(retropp::actionId(zorlduh::Action::Attack), true);
    loop.setRawInput(sample);
    clock.advance(loop.tickPeriod());
    loop.advance();

    ASSERT_TRUE(read.has_value());
    EXPECT_EQ(read->move, (TopDownMovement::DirectionalInput{.held = {.down = true, .right = true},
                                                      .pressed = {.down = true, .right = true}}));
}

TEST(Controls, EveryActionIsBound) {
    const retropp::ActionMap map = zorlduh::actionMap();
    for (const zorlduh::Action action :
         {zorlduh::Action::Up, zorlduh::Action::Down, zorlduh::Action::Left, zorlduh::Action::Right,
          zorlduh::Action::Attack, zorlduh::Action::Item, zorlduh::Action::Start}) {
        bool bound = false;
        for (const retropp::ActionBinding& row : map.rows()) {
            bound = bound || row.action == retropp::actionId(action);
        }
        EXPECT_TRUE(bound) << static_cast<int>(action);
    }
}

// ── The player's art and component (through a renderer on a GPU device) ──────────────────────

class PlayerOnDevice : public ::testing::Test {
protected:
    static inline SDL_GPUDevice* device_ = nullptr;
    static inline std::string error_;

    static void SetUpTestSuite() {
        if (!SDL_Init(SDL_INIT_VIDEO)) {
            error_ = std::string("SDL_Init failed: ") + SDL_GetError();
            return;
        }
        device_ = SDL_CreateGPUDevice(
            SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_METALLIB,
            /*debug_mode=*/false, /*name=*/nullptr);
        if (device_ == nullptr) {
            error_ = std::string("SDL_CreateGPUDevice failed: ") + SDL_GetError();
        }
    }

    static void TearDownTestSuite() {
        if (device_ != nullptr) {
            SDL_DestroyGPUDevice(device_);
            device_ = nullptr;
        }
        SDL_Quit();
    }

    void SetUp() override {
        if (device_ == nullptr) {
            FAIL() << "no GPU device: " << error_
                   << ". The player's art resolves through the renderer, which needs one.";
        }
    }
};

TEST_F(PlayerOnDevice, EachFacingWalksThroughItsOwnRowForEightTicksAFrame) {
    retropp::Renderer renderer{device_, nullptr};
    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    const auto expectRow = [&](const retropp::Animation& walk, zorlduh::PlayerWalk row) {
        ASSERT_EQ(walk.count(), 2u);
        const std::size_t first = static_cast<std::size_t>(row) * 2;
        for (std::size_t i = 0; i < 2; ++i) {
            EXPECT_EQ(walk[i].sheet, assets.player.atlasId);
            EXPECT_EQ(walk[i].tileIndex, std::optional<std::size_t>{first + i});
            EXPECT_EQ(walk[i].palette, assets.palettes.player);
            EXPECT_EQ(kHz60.ticksForDuration(walk[i].duration), 8u);
        }
    };
    expectRow(assets.playerWalk.up, zorlduh::PlayerWalk::Up);
    expectRow(assets.playerWalk.down, zorlduh::PlayerWalk::Down);
    expectRow(assets.playerWalk.left, zorlduh::PlayerWalk::Left);
    expectRow(assets.playerWalk.right, zorlduh::PlayerWalk::Right);
}

TEST_F(PlayerOnDevice, ThePlayerComponentIsOneSpriteKeyedPlayer) {
    retropp::Renderer renderer{device_, nullptr};
    const zorlduh::Assets assets = zorlduh::loadAssets(renderer);
    TopDownMovement::WalkerConfig player = zorlduh::PlayerConfig(assets);
    player.timing = kHz60;
    const zorlduh::GameState state{.player = TopDownMovement::Walker{zorlduh::kPlayerStart}};

    const LayerKit::Sprites sprites = zorlduh::Player(state, player);
    ASSERT_EQ(sprites.size(), 1u);
    const retropp::Sprite& sprite = sprites[0];
    EXPECT_EQ(sprite.key.value, "player");
    EXPECT_EQ(sprite.x, 72);
    EXPECT_EQ(sprite.y, 56);
    EXPECT_EQ(sprite.z, 56 + 16);  // the bottom of its collision box: its feet
    EXPECT_EQ(sprite.atlas, assets.player.atlasId);
    const std::size_t standingDown = static_cast<std::size_t>(zorlduh::PlayerWalk::Down) * 2;
    EXPECT_EQ(sprite.tile, assets.player[standingDown].tile);
    EXPECT_EQ(sprite.size, (retropp::AssetDimensions{.width = 16, .height = 16}));
    EXPECT_EQ(sprite.palette, assets.palettes.player);
}

}  // namespace
