// The TopDownMovement module: walking and sliding against a solid query, the facing law, the walk animation,
// every form of collision shape, the tile collision query, reading the directions from the engine's
// input, anchors, and the sprite a walker is. Everything but the sprite runs with no window or GPU;
// the sprite resolves its frame's art through a renderer on a real GPU device.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/timing.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/walker.h"
#include "TopDownMovement/walker_sprite.h"

namespace {

using TopDownMovement::CollisionShape;
using TopDownMovement::DirectionalInput;
using TopDownMovement::Directions;
using TopDownMovement::TileCollision;
using TopDownMovement::Walker;
using TopDownMovement::WalkerConfig;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
constexpr auto kEightTicks =
    8 * std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(retropp::TickPeriodNs::Hz60)};

enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

// A 10×10-cell room of 8 px cells: wall round the edge, open inside.
retropp::IndexGrid Room() {
    retropp::IndexGrid room{.width = 10, .height = 10};
    room.values.resize(100);
    for (int y = 0; y < 10; ++y) {
        for (int x = 0; x < 10; ++x) {
            const bool edge = x == 0 || y == 0 || x == 9 || y == 9;
            room.values[static_cast<std::size_t>(y) * 10 + x] =
                static_cast<std::uint16_t>(edge ? Cell::Wall : Cell::Open);
        }
    }
    return room;
}

const retropp::IndexGrid kRoom = Room();
const TileCollision kWalls{.map = &kRoom, .cellSize = 8, .open = Cell::Open};

// A two-frame animation whose frames name slots `first` and `first + 1` of sheet 7, 8 ticks each.
retropp::Animation Clip(std::size_t first) {
    return retropp::Animation{{
        {.sheet = retropp::AtlasId{7}, .tileIndex = first, .palette = retropp::PaletteId{3},
         .duration = kEightTicks},
        {.sheet = retropp::AtlasId{7}, .tileIndex = first + 1, .palette = retropp::PaletteId{3},
         .duration = kEightTicks},
    }};
}

// An 8×8 walker colliding with all of itself, one clip per facing in slots Up 0–1, Down 2–3, Left 4–5,
// Right 6–7.
WalkerConfig Config(int speed = 1) {
    return WalkerConfig{
        .speed = speed,
        .collision = IntRect{.x = 0, .y = 0, .width = 8, .height = 8},
        .walk = {.up = Clip(0), .down = Clip(2), .left = Clip(4), .right = Clip(6)},
        .timing = kHz60,
    };
}

// Directions held this tick, none newly pressed.
DirectionalInput Hold(Directions held) {
    return DirectionalInput{.held = held};
}

// Directions held and pressed this tick.
DirectionalInput Press(Directions directions) {
    return DirectionalInput{.held = directions, .pressed = directions};
}

void walkTicks(Walker& walker, const WalkerConfig& config, const DirectionalInput& input, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        walker.walk(config, input, kWalls);
    }
}

// ── Walking ──────────────────────────────────────────────────────────────────────────────────

TEST(Walker, AStepIntoOpenSpaceMovesOnePixel) {
    Walker walker{{16, 16}};
    walker.walk(Config(), Hold({.right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{17, 16}));
    EXPECT_TRUE(walker.moving());
}

TEST(Walker, AStepIntoAWallDoesNotMove) {
    Walker walker{{8, 16}};  // flush against the left wall, which covers pixels 0–7
    walker.walk(Config(), Hold({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    EXPECT_FALSE(walker.moving());
}

TEST(Walker, ADiagonalMovesOnePixelOnBothAxes) {
    Walker walker{{16, 16}};
    walker.walk(Config(), Hold({.down = true, .right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{17, 17}));
}

TEST(Walker, OppositeDirectionsCancel) {
    Walker walker{{16, 16}};
    walker.walk(Config(), Hold({.left = true, .right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{16, 16}));
    EXPECT_FALSE(walker.moving());
    walker.walk(Config(), Hold({.up = true, .down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{15, 16}));
}

TEST(Walker, ADiagonalIntoAWallSlidesAlongIt) {
    Walker walker{{8, 16}};
    walker.walk(Config(), Hold({.down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 17}));
    EXPECT_TRUE(walker.moving());
}

TEST(Walker, ItMovesUpToItsSpeedAndStopsFlushAgainstAWall) {
    Walker walker{{10, 16}};
    walker.walk(Config(3), Hold({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    walker.walk(Config(3), Hold({.right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{11, 16}));
}

TEST(Walker, PlacingMovesItWithoutWalking) {
    Walker walker{{16, 16}};
    walkTicks(walker, Config(), Hold({.right = true}), 3);
    walker.place({4, 4});  // inside the wall: placing never collides
    EXPECT_EQ(walker.position(), (Vec2i{4, 4}));
    EXPECT_FALSE(walker.moving());
}

// ── Facing ───────────────────────────────────────────────────────────────────────────────────

TEST(Facing, WalkingAlongOneAxisFacesTheWayItWalks) {
    Walker walker{{32, 32}};
    walker.walk(Config(), Press({.right = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), Press({.up = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Up);
}

TEST(Facing, ADiagonalKeepsAFacingThatIsOneOfItsDirections) {
    Walker walker{{32, 32}, Dir::Right};
    walker.walk(Config(), Press({.down = true, .right = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), DirectionalInput{.held = {.down = true, .left = true}, .pressed = {.left = true}},
                kWalls);
    EXPECT_EQ(walker.facing(), Dir::Left);  // Right is no longer held; Left was pressed after Down
}

TEST(Facing, ADiagonalOtherwiseFacesTheMoreRecentPress) {
    Walker walker{{32, 32}, Dir::Up};
    walker.walk(Config(), DirectionalInput{.pressed = {.right = true}}, kWalls);  // a tap: no step
    walker.walk(Config(), DirectionalInput{.held = {.down = true, .right = true}, .pressed = {.down = true}},
                kWalls);
    EXPECT_EQ(walker.facing(), Dir::Down);

    Walker other{{32, 32}, Dir::Up};
    other.walk(Config(), DirectionalInput{.pressed = {.down = true}}, kWalls);
    other.walk(Config(), DirectionalInput{.held = {.down = true, .right = true}, .pressed = {.right = true}},
               kWalls);
    EXPECT_EQ(other.facing(), Dir::Right);
}

TEST(Facing, PressedOnTheSameTickTheHorizontalDirectionWins) {
    Walker walker{{32, 32}, Dir::Up};
    walker.walk(Config(), Press({.down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Left);
}

TEST(Facing, ABlockedWalkerTurnsToFaceTheWayItPushesAndStandsStill) {
    Walker walker{{8, 16}, Dir::Down};
    walker.walk(Config(), Press({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    EXPECT_EQ(walker.facing(), Dir::Left);
    EXPECT_FALSE(walker.moving());
}

TEST(Facing, HoldingNothingOrOppositeDirectionsKeepsTheFacing) {
    Walker walker{{32, 32}, Dir::Right};
    walker.walk(Config(), DirectionalInput{}, kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), Press({.up = true, .down = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
}

TEST(Facing, ADiagonalSlideStillFollowsTheDiagonalRule) {
    Walker walker{{8, 16}, Dir::Up};
    walker.walk(Config(), Press({.down = true, .left = true}), kWalls);  // left is blocked, down moves
    EXPECT_EQ(walker.position(), (Vec2i{8, 17}));
    EXPECT_EQ(walker.facing(), Dir::Left);
}

// ── The walk animation ───────────────────────────────────────────────────────────────────────

TEST(WalkAnimation, AdvancesAFrameEveryEightTicksOfWalking) {
    const WalkerConfig config = Config();
    Walker walker{{16, 16}};
    walkTicks(walker, config, Hold({.right = true}), 7);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
    walkTicks(walker, config, Hold({.right = true}), 1);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{7});
    walkTicks(walker, config, Hold({.right = true}), 8);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(WalkAnimation, StandingShowsFrameZeroOfTheFacing) {
    const WalkerConfig config = Config();
    Walker walker{{16, 16}};
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{2});  // Down, never walked
    walkTicks(walker, config, Hold({.right = true}), 8);
    walker.walk(config, DirectionalInput{}, kWalls);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(WalkAnimation, ATurnKeepsThePlaceInTheStride) {
    const WalkerConfig config = Config();
    Walker walker{{32, 32}};
    walkTicks(walker, config, Press({.right = true}), 8);
    walker.walk(config, Press({.down = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Down);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{3});
}

TEST(WalkAnimation, AFacingWithNoFramesThrows) {
    WalkerConfig config = Config();
    config.walk.down = {};
    const Walker walker{{16, 16}};
    EXPECT_THROW((void)walker.frame(config), std::logic_error);
}

// ── Collision shapes ─────────────────────────────────────────────────────────────────────────

TEST(CollisionShape, ARectangleCoversExactlyItsPixels) {
    const CollisionShape box = IntRect{.x = 3, .y = 8, .width = 10, .height = 8};
    EXPECT_EQ(box.bounds(), (IntRect{.x = 3, .y = 8, .width = 10, .height = 8}));
    EXPECT_TRUE(box.covers({3, 8}));
    EXPECT_TRUE(box.covers({12, 15}));
    EXPECT_FALSE(box.covers({13, 8}));
    EXPECT_FALSE(box.covers({3, 16}));
    EXPECT_TRUE((CollisionShape{IntRect{.width = 0, .height = 4}}.empty()));
}

TEST(CollisionShape, AShapeRectangleCoversWhatTheBoxCovers) {
    const CollisionShape box = IntRect{.x = 3, .y = 8, .width = 10, .height = 8};
    const CollisionShape shape = retropp::ShapePoints::rectangle({3, 8}, 10, 8);
    EXPECT_EQ(shape.bounds(), box.bounds());
    for (int y = 6; y < 18; ++y) {
        for (int x = 1; x < 15; ++x) {
            EXPECT_EQ(shape.covers({x, y}), box.covers({x, y})) << x << "," << y;
        }
    }
}

TEST(CollisionShape, ACircleCoversThePixelsWhoseCentersAreInside) {
    const CollisionShape circle = retropp::ShapePoints::circle({8, 8}, 3);
    EXPECT_TRUE(circle.covers({7, 7}));
    EXPECT_TRUE(circle.covers({8, 5}));     // center (8.5, 5.5): 2.55 from (8, 8)
    EXPECT_FALSE(circle.covers({5, 5}));    // center (5.5, 5.5): 3.54 away
    EXPECT_FALSE(circle.covers({8, 4}));    // center (8.5, 4.5): 3.54 away
    EXPECT_EQ(circle.bounds(), (IntRect{.x = 5, .y = 5, .width = 6, .height = 6}));
}

TEST(CollisionShape, AnchorsMakeThePolygonThroughThemInOrder) {
    constexpr std::array<retropp::Anchor, 5> kArt{{
        {.label = "hand", .x = 14, .y = 6},
        {.label = "heel-l", .x = 4, .y = 12},
        {.label = "toe-l", .x = 4, .y = 16},
        {.label = "toe-r", .x = 12, .y = 16},
        {.label = "heel-r", .x = 12, .y = 12},
    }};
    const CollisionShape feet =
        CollisionShape::fromAnchors(kArt, {"heel-l", "toe-l", "toe-r", "heel-r"});
    EXPECT_EQ(feet.bounds(), (IntRect{.x = 4, .y = 12, .width = 8, .height = 4}));
    EXPECT_THROW((void)CollisionShape::fromAnchors(kArt, {"heel-l", "toe-l", "nose"}), std::out_of_range);
    EXPECT_THROW((void)CollisionShape::fromAnchors(kArt, {"heel-l", "toe-l"}), std::invalid_argument);
}

TEST(CollisionShape, AShapeWithNoRegionAnInvertedShapeOrATransformedShapeIsRejected) {
    EXPECT_THROW(CollisionShape{retropp::ShapePoints{}}, std::invalid_argument);
    EXPECT_THROW(CollisionShape{retropp::ShapePoints::circle({8, 8}, 3).inverted()}, std::invalid_argument);
    retropp::ShapePoints turned = retropp::ShapePoints::rectangle({0, 0}, 8, 8);
    turned.transform = retropp::Transform::rotation(0.5f);
    EXPECT_THROW(CollisionShape{turned}, std::invalid_argument);
}

TEST(CollisionShape, AWalkerWithACircleStopsWhereTheCircleMeetsTheWall) {
    WalkerConfig config = Config();
    config.collision = retropp::ShapePoints::circle({4, 4}, 3);  // covers pixels 1–6 of the 8×8 art
    Walker walker{{16, 16}};
    walkTicks(walker, config, Hold({.left = true}), 20);
    EXPECT_EQ(walker.position(), (Vec2i{7, 16}));  // its leftmost covered pixel stops at 8
}

TEST(CollisionShape, MovingAShapeMovesItsPixelsAndBounds) {
    const CollisionShape box = IntRect{.x = 1, .y = 2, .width = 3, .height = 4};
    const CollisionShape moved = box.at({10, 20});
    EXPECT_EQ(moved.bounds(), (IntRect{.x = 11, .y = 22, .width = 3, .height = 4}));
    EXPECT_TRUE(moved.covers({11, 22}));
    EXPECT_FALSE(moved.covers({1, 2}));
}

TEST(CollisionShape, AnyCallableIsASolidQuery) {
    const auto rightHalf = [](IntRect box) { return box.x + box.width > 40; };
    const CollisionShape box = IntRect{.x = 0, .y = 0, .width = 8, .height = 8};
    EXPECT_FALSE(box.hits(rightHalf, {32, 0}));
    EXPECT_TRUE(box.hits(rightHalf, {33, 0}));
}

// ── Tile collision ───────────────────────────────────────────────────────────────────────────

TEST(TileCollision, OnlyTheOpenValueIsOpenAndEverythingOffTheMapIsSolid) {
    EXPECT_TRUE(kWalls.solidAt({7, 20}));
    EXPECT_FALSE(kWalls.solidAt({8, 20}));
    EXPECT_TRUE(kWalls.solidAt({-1, 20}));
    EXPECT_TRUE(kWalls.solidAt({20, 80}));
    EXPECT_FALSE(kWalls.solidBox({.x = 8, .y = 8, .width = 64, .height = 64}));
    EXPECT_TRUE(kWalls.solidBox({.x = 8, .y = 8, .width = 65, .height = 64}));
    EXPECT_FALSE(kWalls.solidBox({.x = 0, .y = 0, .width = 0, .height = 8}));
}

TEST(TileCollision, WithNoMapEverythingIsSolid) {
    const TileCollision none{};
    EXPECT_TRUE(none.solidAt({20, 20}));
}

// ── Reading the directions ───────────────────────────────────────────────────────────────────

enum class Action : std::uint8_t { Up, Down, Left, Right, Jump };

class ManualClock final : public retropp::Clock {
public:
    [[nodiscard]] std::chrono::nanoseconds now() const noexcept override { return now_; }
    void advance(std::chrono::nanoseconds by) noexcept { now_ += by; }

private:
    std::chrono::nanoseconds now_{};
};

// The DirectionalInput read on each of a run loop's ticks, one tick per sample.
std::vector<DirectionalInput> ReadEachTick(std::initializer_list<std::initializer_list<Action>> held) {
    ManualClock clock;
    retropp::RunLoop loop{clock, kHz60};
    std::vector<DirectionalInput> read;
    loop.simTick([&](const retropp::InputState& input) {
        read.push_back(TopDownMovement::readDirections(
            input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right}));
    });
    loop.renderLoop([] {});
    loop.advance();  // the first call starts the clock
    for (const auto& actions : held) {
        retropp::InputSample sample;
        for (const Action action : actions) {
            sample.players[0].held.set(retropp::actionId(action), true);
        }
        loop.setRawInput(sample);
        clock.advance(loop.tickPeriod());
        loop.advance();
    }
    return read;
}

TEST(ReadDirections, HeldAndPressedComeFromTheGamesOwnActions) {
    const std::vector<DirectionalInput> read =
        ReadEachTick({{Action::Left}, {Action::Left, Action::Up, Action::Jump}, {}});
    ASSERT_EQ(read.size(), 3u);
    EXPECT_EQ(read[0], (DirectionalInput{.held = {.left = true}, .pressed = {.left = true}}));
    EXPECT_EQ(read[1], (DirectionalInput{.held = {.up = true, .left = true}, .pressed = {.up = true}}));
    EXPECT_EQ(read[2], DirectionalInput{});
}

// ── Anchors ──────────────────────────────────────────────────────────────────────────────────

TEST(Walker, AnAnchorIsReadInWorldPixels) {
    constexpr std::array<retropp::Anchor, 1> kArt{{{.label = "hand", .x = 14, .y = 6}}};
    WalkerConfig config = Config();
    config.anchors = kArt;
    const Walker walker{{40, 30}};
    EXPECT_EQ(walker.anchor(config, "hand"), (retropp::Point{54, 36}));
    EXPECT_THROW((void)walker.anchor(config, "foot"), std::out_of_range);
    EXPECT_EQ(walker.collision(config).bounds(), (IntRect{.x = 40, .y = 30, .width = 8, .height = 8}));
}

// ── The sprite (resolved through a renderer on a GPU device) ─────────────────────────────────

class WalkerSpriteOnDevice : public ::testing::Test {
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
                   << ". A frame's tile and size resolve through the renderer, which needs one.";
        }
    }
};

TEST_F(WalkerSpriteOnDevice, IsTheWalkersFrameAtItsPositionSortedByTheBottomOfItsCollision) {
    retropp::Renderer renderer{device_, nullptr};
    // 4 rows × 2 frames of 16×16, every pixel index 1.
    const std::vector<std::uint8_t> pixels(32 * 64, 1);
    const retropp::AtlasManifest sheet = renderer.uploadAtlas(
        pixels.data(), 32, 64, retropp::AssetDimensions{.width = 16, .height = 16},
        retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown, 0,
        retropp::TransparentIndices::of({0}), 2);
    const auto clip = [&](std::size_t row) {
        return retropp::Animation{{
            {.sheet = sheet.atlasId, .tileIndex = row * 2, .palette = retropp::PaletteId{5},
             .duration = kEightTicks},
            {.sheet = sheet.atlasId, .tileIndex = row * 2 + 1, .palette = retropp::PaletteId{5},
             .duration = kEightTicks},
        }};
    };
    constexpr std::array<retropp::Anchor, 1> kArt{{{.label = "hand", .x = 14, .y = 6}}};
    const WalkerConfig config{
        .collision = IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
        .anchors = kArt,
        .walk = {.up = clip(1), .down = clip(0), .left = clip(2), .right = clip(3)},
        .timing = kHz60,
    };
    const Walker walker{{40, 30}, Dir::Left};

    const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", walker, config);
    EXPECT_EQ(sprite.key.value, "hero");
    EXPECT_EQ(sprite.x, 40);
    EXPECT_EQ(sprite.y, 30);
    EXPECT_EQ(sprite.z, 30 + 16);
    EXPECT_EQ(sprite.atlas, sheet.atlasId);
    EXPECT_EQ(sprite.tile, sheet[4].tile);
    EXPECT_EQ(sprite.size, (retropp::AssetDimensions{.width = 16, .height = 16}));
    EXPECT_EQ(sprite.palette, retropp::PaletteId{5});
    ASSERT_EQ(sprite.anchors.size(), 1u);
    EXPECT_EQ(sprite.anchors[0].label, "hand");
}

}  // namespace
