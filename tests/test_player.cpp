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
#include "game/player.h"
#include "game/rooms.h"
#include "game/sim.h"
#include "game/world.h"
#include "input/input.h"
#include "TopDownMovement/walker.h"
#include "render/player.h"

namespace {

using retropp::Dir;
using retropp::Vec2i;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

zorlduh::World loadWorld() {
    return zorlduh::World{.tiles = retropp::loadMapPng("assets/maps/world/tiles.png"),
                          .collision = retropp::loadMapPng("assets/maps/world/collision.png")};
}

// The player as PlayerConfig makes it, from assets whose walk animations name made-up handles.
TopDownMovement::WalkerConfig headlessPlayer() {
    zorlduh::Assets assets;
    TopDownMovement::WalkerConfig config = zorlduh::PlayerConfig(assets);
    config.timing = kHz60;
    return config;
}

TopDownMovement::DirectionalInput Hold(TopDownMovement::Directions held) {
    return TopDownMovement::DirectionalInput{.held = held, .pressed = held};
}

// ── Where it starts and how it walks the world ───────────────────────────────────────────────

TEST(Player, StartsCenteredInTheStartRoomClearOfEverySolidCell) {
    const zorlduh::World world = loadWorld();
    const TopDownMovement::WalkerConfig player = headlessPlayer();
    EXPECT_EQ(zorlduh::kPlayerStart, (Vec2i{72, 56}));
    EXPECT_EQ(world.roomAt(zorlduh::kPlayerStart.x + 8, zorlduh::kPlayerStart.y + 8),
              std::optional<zorlduh::RoomId>{zorlduh::kStartRoom});
    EXPECT_FALSE(player.collision.hits(world, zorlduh::kPlayerStart));
}

TEST(Player, CollidesWithTheBottomHalfOfItsFrame) {
    const TopDownMovement::WalkerConfig player = headlessPlayer();
    EXPECT_EQ(player.speed, 1);
    EXPECT_EQ(player.collision.bounds(), (retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8}));
}

TEST(Player, TheTickWalksItUntilItsCollisionBoxMeetsTheWall) {
    const TopDownMovement::WalkerConfig player = headlessPlayer();
    zorlduh::GameState state{.world = loadWorld(), .player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    for (int i = 0; i < 100; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.left = true})}, player);
    }
    // Room 0's left wall is column 0, pixels 0–7; the box starts 3 px into the frame.
    EXPECT_EQ(state.player.position(), (Vec2i{5, 56}));
    EXPECT_EQ(state.player.facing(), Dir::Left);
    for (int i = 0; i < 100; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.up = true})}, player);
    }
    // Its top wall is row 0; the box starts 8 px down the frame.
    EXPECT_EQ(state.player.position(), (Vec2i{5, 0}));
}

TEST(Player, WalksOutThroughTheOpeningToTheNextRoom) {
    const TopDownMovement::WalkerConfig player = headlessPlayer();
    zorlduh::GameState state{.world = loadWorld(), .player = TopDownMovement::Walker{zorlduh::kPlayerStart}};
    for (int i = 0; i < 100; ++i) {
        zorlduh::tick(state, zorlduh::Input{.move = Hold({.right = true})}, player);
    }
    // The 0↔1 opening spans rows 6–9 (pixels 48–79), which hold the whole box (pixel rows 64–71).
    EXPECT_EQ(state.player.position(), (Vec2i{172, 56}));
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
    const zorlduh::GameState state{
        .world = zorlduh::World{.tiles = assets.world, .collision = assets.collision},
        .player = TopDownMovement::Walker{zorlduh::kPlayerStart},
    };

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
