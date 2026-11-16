// The TopDownMovement module: a walker bound to a position it does not own; walking by a heading and by
// input, under 8-way and 4-way movement, and sliding against a solid query; the two facings and the 4 or
// 8 sprite directions; the walk animation; being moved by something else (moveTo) and along the engine's
// SpritePath (routeFrom, follow), interrupts included; every form of collision shape, the tile collision
// query, reading the directions from the engine's input, anchors, and the sprite a walker is.
// Everything but the sprite runs with no window or GPU; the sprite resolves its frame's art through a
// renderer on a real GPU device.

#include <gtest/gtest.h>

#include <SDL3/SDL.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "retropp/animation.h"
#include "retropp/clock.h"
#include "retropp/draw_state.h"
#include "retropp/geometry.h"
#include "retropp/image.h"
#include "retropp/input.h"
#include "retropp/input_actions.h"
#include "retropp/path_walker.h"
#include "retropp/renderer.h"
#include "retropp/run_loop.h"
#include "retropp/sprite_path.h"
#include "retropp/timing.h"

#include "TopDownMovement/collision.h"
#include "TopDownMovement/directions.h"
#include "TopDownMovement/input_walker.h"
#include "TopDownMovement/route.h"
#include "TopDownMovement/walker.h"
#include "TopDownMovement/walker_sprite.h"

namespace {

using TopDownMovement::CollisionShape;
using TopDownMovement::DirectionalInput;
using TopDownMovement::Directions;
using TopDownMovement::Facing;
using TopDownMovement::InputWalker;
using TopDownMovement::TileCollision;
using TopDownMovement::Walker;
using TopDownMovement::WalkerConfig;
using TopDownMovement::Ways;
using retropp::Dir;
using retropp::IntRect;
using retropp::Vec2i;

constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
constexpr auto kOneTick = std::chrono::nanoseconds{static_cast<std::chrono::nanoseconds::rep>(retropp::TickPeriodNs::Hz60)};
constexpr auto kEightTicks = 8 * kOneTick;

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

// An 8×8 walker colliding with all of itself, one clip per sprite direction in slots Up 0–1, Down 2–3,
// Left 4–5, Right 6–7, UpLeft 8–9, UpRight 10–11, DownLeft 12–13, DownRight 14–15.
WalkerConfig Config(int speed = 1) {
    return WalkerConfig{
        .speed = speed,
        .collision = IntRect{.x = 0, .y = 0, .width = 8, .height = 8},
        .walk = {.up = Clip(0), .down = Clip(2), .left = Clip(4), .right = Clip(6),
                 .upLeft = Clip(8), .upRight = Clip(10), .downLeft = Clip(12), .downRight = Clip(14)},
        .timing = kHz60,
    };
}

// The same walker under other switches.
WalkerConfig Config(Ways movement, Ways sprites) {
    WalkerConfig config = Config();
    config.movement = movement;
    config.sprites = sprites;
    return config;
}

// Directions held this tick, none newly pressed.
DirectionalInput Hold(Directions held) {
    return DirectionalInput{.held = held};
}

// Directions held and pressed this tick.
DirectionalInput Press(Directions directions) {
    return DirectionalInput{.held = directions, .pressed = directions};
}

void walkTicks(InputWalker& walker, const WalkerConfig& config, const DirectionalInput& input, int ticks) {
    for (int i = 0; i < ticks; ++i) {
        walker.walk(config, input, kWalls);
    }
}

// ── Walking ──────────────────────────────────────────────────────────────────────────────────

TEST(Walker, AStepIntoOpenSpaceMovesOnePixel) {
    Vec2i at{16, 16};
    InputWalker walker{at};
    walker.walk(Config(), Hold({.right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{17, 16}));
    EXPECT_TRUE(walker.moving());
}

TEST(Walker, AStepIntoAWallDoesNotMove) {
    Vec2i at{8, 16};  // flush against the left wall, which covers pixels 0–7
    InputWalker walker{at};
    walker.walk(Config(), Hold({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    EXPECT_FALSE(walker.moving());
}

TEST(Walker, ADiagonalMovesOnePixelOnBothAxes) {
    Vec2i at{16, 16};
    InputWalker walker{at};
    walker.walk(Config(), Hold({.down = true, .right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{17, 17}));
}

TEST(Walker, OppositeDirectionsCancel) {
    Vec2i at{16, 16};
    InputWalker walker{at};
    walker.walk(Config(), Hold({.left = true, .right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{16, 16}));
    EXPECT_FALSE(walker.moving());
    walker.walk(Config(), Hold({.up = true, .down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{15, 16}));
}

TEST(Walker, ADiagonalIntoAWallSlidesAlongIt) {
    Vec2i at{8, 16};
    InputWalker walker{at};
    walker.walk(Config(), Hold({.down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 17}));
    EXPECT_TRUE(walker.moving());
}

TEST(Walker, ItMovesUpToItsSpeedAndStopsFlushAgainstAWall) {
    Vec2i at{10, 16};
    InputWalker walker{at};
    walker.walk(Config(3), Hold({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    walker.walk(Config(3), Hold({.right = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{11, 16}));
}

TEST(Walker, PlacingMovesItWithoutWalking) {
    Vec2i at{16, 16};
    InputWalker walker{at};
    walkTicks(walker, Config(), Hold({.right = true}), 3);
    walker.place({4, 4});  // inside the wall: placing never collides
    EXPECT_EQ(walker.position(), (Vec2i{4, 4}));
    EXPECT_FALSE(walker.moving());
}

// ── Facing ───────────────────────────────────────────────────────────────────────────────────

TEST(Facing, WalkingAlongOneAxisFacesTheWayItWalks) {
    Vec2i at{32, 32};
    InputWalker walker{at};
    walker.walk(Config(), Press({.right = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), Press({.up = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Up);
}

TEST(Facing, ADiagonalKeepsAFacingThatIsOneOfItsDirections) {
    Vec2i at{32, 32};
    InputWalker walker{at, Dir::Right};
    walker.walk(Config(), Press({.down = true, .right = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), DirectionalInput{.held = {.down = true, .left = true}, .pressed = {.left = true}},
                kWalls);
    EXPECT_EQ(walker.facing(), Dir::Left);  // Right is no longer held; Left was pressed after Down
}

TEST(Facing, ADiagonalOtherwiseFacesTheMoreRecentPress) {
    Vec2i at{32, 32};
    InputWalker walker{at, Dir::Up};
    walker.walk(Config(), DirectionalInput{.pressed = {.right = true}}, kWalls);  // a tap: no step
    walker.walk(Config(), DirectionalInput{.held = {.down = true, .right = true}, .pressed = {.down = true}},
                kWalls);
    EXPECT_EQ(walker.facing(), Dir::Down);

    Vec2i otherAt{32, 32};
    InputWalker other{otherAt, Dir::Up};
    other.walk(Config(), DirectionalInput{.pressed = {.down = true}}, kWalls);
    other.walk(Config(), DirectionalInput{.held = {.down = true, .right = true}, .pressed = {.right = true}},
               kWalls);
    EXPECT_EQ(other.facing(), Dir::Right);
}

TEST(Facing, PressedOnTheSameTickTheHorizontalDirectionWins) {
    Vec2i at{32, 32};
    InputWalker walker{at, Dir::Up};
    walker.walk(Config(), Press({.down = true, .left = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Left);
}

TEST(Facing, ABlockedWalkerTurnsToFaceTheWayItPushesAndStandsStill) {
    Vec2i at{8, 16};
    InputWalker walker{at, Dir::Down};
    walker.walk(Config(), Press({.left = true}), kWalls);
    EXPECT_EQ(walker.position(), (Vec2i{8, 16}));
    EXPECT_EQ(walker.facing(), Dir::Left);
    EXPECT_FALSE(walker.moving());
}

TEST(Facing, HoldingNothingOrOppositeDirectionsKeepsTheFacing) {
    Vec2i at{32, 32};
    InputWalker walker{at, Dir::Right};
    walker.walk(Config(), DirectionalInput{}, kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(Config(), Press({.up = true, .down = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
}

TEST(Facing, ADiagonalSlideStillFollowsTheDiagonalRule) {
    Vec2i at{8, 16};
    InputWalker walker{at, Dir::Up};
    walker.walk(Config(), Press({.down = true, .left = true}), kWalls);  // left is blocked, down moves
    EXPECT_EQ(walker.position(), (Vec2i{8, 17}));
    EXPECT_EQ(walker.facing(), Dir::Left);
}

// ── The walk animation ───────────────────────────────────────────────────────────────────────

TEST(WalkAnimation, AdvancesAFrameEveryEightTicksOfWalking) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    InputWalker walker{at};
    walkTicks(walker, config, Hold({.right = true}), 7);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
    walkTicks(walker, config, Hold({.right = true}), 1);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{7});
    walkTicks(walker, config, Hold({.right = true}), 8);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(WalkAnimation, StandingShowsFrameZeroOfTheFacing) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    InputWalker walker{at};
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{2});  // Down, never walked
    walkTicks(walker, config, Hold({.right = true}), 8);
    walker.walk(config, DirectionalInput{}, kWalls);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(WalkAnimation, ATurnKeepsThePlaceInTheStride) {
    const WalkerConfig config = Config();
    Vec2i at{32, 32};
    InputWalker walker{at};
    walkTicks(walker, config, Press({.right = true}), 8);
    walker.walk(config, Press({.down = true}), kWalls);
    EXPECT_EQ(walker.facing(), Dir::Down);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{3});
}

TEST(WalkAnimation, AFacingWithNoFramesThrows) {
    WalkerConfig config = Config();
    config.walk.down = {};
    Vec2i at{16, 16};
    const InputWalker walker{at};
    EXPECT_THROW((void)walker.frame(config), std::logic_error);
}

// ── A walker walks a position it does not own ────────────────────────────────────────────────

TEST(Binding, AWalkerMovesThePositionItIsBoundTo) {
    Vec2i at{16, 16};
    Walker walker{at};
    walker.walk(Config(), Directions{.right = true}, kWalls);
    EXPECT_EQ(at, (Vec2i{17, 16}));
    walker.place({20, 20});
    EXPECT_EQ(at, (Vec2i{20, 20}));

    Vec2i other{40, 40};
    walker.bind(other);
    walker.walk(Config(), Directions{.right = true}, kWalls);
    EXPECT_EQ(other, (Vec2i{41, 40}));
    EXPECT_EQ(at, (Vec2i{20, 20}));
    EXPECT_EQ(walker.position(), (Vec2i{41, 40}));
}

// ── Walking by a heading ─────────────────────────────────────────────────────────────────────

TEST(Heading, AWalkerMovesByItsHeadingAndSlidesAlongWalls) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    Walker walker{at};
    walker.walk(config, Directions{.right = true}, kWalls);
    EXPECT_EQ(at, (Vec2i{17, 16}));
    walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_EQ(at, (Vec2i{18, 17}));
    walker.walk(config, Directions{.left = true, .right = true}, kWalls);
    EXPECT_EQ(at, (Vec2i{18, 17}));
    EXPECT_FALSE(walker.moving());

    Vec2i wallAt{8, 16};
    Walker atTheWall{wallAt};
    atTheWall.walk(config, Directions{.left = true}, kWalls);
    EXPECT_EQ(wallAt, (Vec2i{8, 16}));
    EXPECT_FALSE(atTheWall.moving());
    atTheWall.walk(config, Directions{.down = true, .left = true}, kWalls);
    EXPECT_EQ(wallAt, (Vec2i{8, 17}));
    EXPECT_TRUE(atTheWall.moving());
}

TEST(Heading, TheFacingFollowsTheHeadingWithoutThePressOrder) {
    const WalkerConfig config = Config();
    Vec2i at{32, 32};
    Walker walker{at};
    walker.walk(config, Directions{.right = true}, kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_EQ(walker.facing(), Dir::Right);  // one of the two: kept

    Vec2i otherAt{32, 32};
    Walker other{otherAt, Dir::Up};
    other.walk(config, Directions{.down = true, .left = true}, kWalls);
    EXPECT_EQ(other.facing(), Dir::Left);  // neither: the horizontal one

    Vec2i wallAt{8, 16};
    Walker blocked{wallAt, Dir::Down};
    blocked.walk(config, Directions{.left = true}, kWalls);
    EXPECT_EQ(blocked.facing(), Dir::Left);
    EXPECT_FALSE(blocked.moving());
}

// ── 8-way movement by input, tick for tick ───────────────────────────────────────────────────

// A scripted run of 200 ticks over the room: one axis, diagonals held in both press orders, two
// directions pressed on one tick, a tap, opposite directions, walls on three sides. The input that
// spans of ticks hold; what a span presses is pressed on its first tick.
DirectionalInput Scripted(int tick) {
    struct Span {
        int from;
        Directions held;
        Directions pressed;
    };
    static const std::array<Span, 12> spans{{
        {0, {.right = true}, {.right = true}},
        {20, {.down = true, .right = true}, {.down = true}},
        {40, {.down = true}, {}},
        {50, {.down = true, .left = true}, {.left = true}},
        {70, {}, {}},
        {80, {.up = true, .left = true}, {.up = true, .left = true}},
        {100, {.left = true}, {}},
        {130, {.up = true}, {.up = true}},
        {135, {.up = true, .right = true}, {.right = true}},
        {150, {.up = true}, {}},
        {170, {.down = true, .left = true, .right = true}, {.down = true}},
        {190, {.right = true}, {.right = true}},
    }};
    const Span* span = &spans[0];
    for (const Span& s : spans) {
        if (s.from <= tick) {
            span = &s;
        }
    }
    return DirectionalInput{.held = span->held, .pressed = span->from == tick ? span->pressed : Directions{}};
}

// Where the walker stands after each tick of the scripted run, the way it faces, the slot of the frame
// it shows and whether it moved.
struct Sample {
    int x;
    int y;
    Dir facing;
    std::size_t slot;
    bool moving;
};

// The scripted run under the default switches, one sample per tick.
constexpr std::array<Sample, 200> kScriptedRun{{
    {33, 32, Dir::Right, 6, true}, {34, 32, Dir::Right, 6, true}, {35, 32, Dir::Right, 6, true}, {36, 32, Dir::Right, 6, true},
    {37, 32, Dir::Right, 6, true}, {38, 32, Dir::Right, 6, true}, {39, 32, Dir::Right, 6, true}, {40, 32, Dir::Right, 7, true},
    {41, 32, Dir::Right, 7, true}, {42, 32, Dir::Right, 7, true}, {43, 32, Dir::Right, 7, true}, {44, 32, Dir::Right, 7, true},
    {45, 32, Dir::Right, 7, true}, {46, 32, Dir::Right, 7, true}, {47, 32, Dir::Right, 7, true}, {48, 32, Dir::Right, 6, true},
    {49, 32, Dir::Right, 6, true}, {50, 32, Dir::Right, 6, true}, {51, 32, Dir::Right, 6, true}, {52, 32, Dir::Right, 6, true},
    {53, 33, Dir::Right, 6, true}, {54, 34, Dir::Right, 6, true}, {55, 35, Dir::Right, 6, true}, {56, 36, Dir::Right, 7, true},
    {57, 37, Dir::Right, 7, true}, {58, 38, Dir::Right, 7, true}, {59, 39, Dir::Right, 7, true}, {60, 40, Dir::Right, 7, true},
    {61, 41, Dir::Right, 7, true}, {62, 42, Dir::Right, 7, true}, {63, 43, Dir::Right, 7, true}, {64, 44, Dir::Right, 6, true},
    {64, 45, Dir::Right, 6, true}, {64, 46, Dir::Right, 6, true}, {64, 47, Dir::Right, 6, true}, {64, 48, Dir::Right, 6, true},
    {64, 49, Dir::Right, 6, true}, {64, 50, Dir::Right, 6, true}, {64, 51, Dir::Right, 6, true}, {64, 52, Dir::Right, 7, true},
    {64, 53, Dir::Down, 3, true}, {64, 54, Dir::Down, 3, true}, {64, 55, Dir::Down, 3, true}, {64, 56, Dir::Down, 3, true},
    {64, 57, Dir::Down, 3, true}, {64, 58, Dir::Down, 3, true}, {64, 59, Dir::Down, 3, true}, {64, 60, Dir::Down, 2, true},
    {64, 61, Dir::Down, 2, true}, {64, 62, Dir::Down, 2, true}, {63, 63, Dir::Down, 2, true}, {62, 64, Dir::Down, 2, true},
    {61, 64, Dir::Down, 2, true}, {60, 64, Dir::Down, 2, true}, {59, 64, Dir::Down, 2, true}, {58, 64, Dir::Down, 3, true},
    {57, 64, Dir::Down, 3, true}, {56, 64, Dir::Down, 3, true}, {55, 64, Dir::Down, 3, true}, {54, 64, Dir::Down, 3, true},
    {53, 64, Dir::Down, 3, true}, {52, 64, Dir::Down, 3, true}, {51, 64, Dir::Down, 3, true}, {50, 64, Dir::Down, 2, true},
    {49, 64, Dir::Down, 2, true}, {48, 64, Dir::Down, 2, true}, {47, 64, Dir::Down, 2, true}, {46, 64, Dir::Down, 2, true},
    {45, 64, Dir::Down, 2, true}, {44, 64, Dir::Down, 2, true}, {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false},
    {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false},
    {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false}, {44, 64, Dir::Down, 2, false},
    {43, 63, Dir::Left, 4, true}, {42, 62, Dir::Left, 4, true}, {41, 61, Dir::Left, 4, true}, {40, 60, Dir::Left, 4, true},
    {39, 59, Dir::Left, 4, true}, {38, 58, Dir::Left, 4, true}, {37, 57, Dir::Left, 4, true}, {36, 56, Dir::Left, 5, true},
    {35, 55, Dir::Left, 5, true}, {34, 54, Dir::Left, 5, true}, {33, 53, Dir::Left, 5, true}, {32, 52, Dir::Left, 5, true},
    {31, 51, Dir::Left, 5, true}, {30, 50, Dir::Left, 5, true}, {29, 49, Dir::Left, 5, true}, {28, 48, Dir::Left, 4, true},
    {27, 47, Dir::Left, 4, true}, {26, 46, Dir::Left, 4, true}, {25, 45, Dir::Left, 4, true}, {24, 44, Dir::Left, 4, true},
    {23, 44, Dir::Left, 4, true}, {22, 44, Dir::Left, 4, true}, {21, 44, Dir::Left, 4, true}, {20, 44, Dir::Left, 5, true},
    {19, 44, Dir::Left, 5, true}, {18, 44, Dir::Left, 5, true}, {17, 44, Dir::Left, 5, true}, {16, 44, Dir::Left, 5, true},
    {15, 44, Dir::Left, 5, true}, {14, 44, Dir::Left, 5, true}, {13, 44, Dir::Left, 5, true}, {12, 44, Dir::Left, 4, true},
    {11, 44, Dir::Left, 4, true}, {10, 44, Dir::Left, 4, true}, {9, 44, Dir::Left, 4, true}, {8, 44, Dir::Left, 4, true},
    {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false},
    {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false},
    {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false},
    {8, 44, Dir::Left, 4, false}, {8, 44, Dir::Left, 4, false}, {8, 43, Dir::Up, 0, true}, {8, 42, Dir::Up, 0, true},
    {8, 41, Dir::Up, 0, true}, {8, 40, Dir::Up, 0, true}, {8, 39, Dir::Up, 0, true}, {9, 38, Dir::Up, 0, true},
    {10, 37, Dir::Up, 0, true}, {11, 36, Dir::Up, 1, true}, {12, 35, Dir::Up, 1, true}, {13, 34, Dir::Up, 1, true},
    {14, 33, Dir::Up, 1, true}, {15, 32, Dir::Up, 1, true}, {16, 31, Dir::Up, 1, true}, {17, 30, Dir::Up, 1, true},
    {18, 29, Dir::Up, 1, true}, {19, 28, Dir::Up, 0, true}, {20, 27, Dir::Up, 0, true}, {21, 26, Dir::Up, 0, true},
    {22, 25, Dir::Up, 0, true}, {23, 24, Dir::Up, 0, true}, {23, 23, Dir::Up, 0, true}, {23, 22, Dir::Up, 0, true},
    {23, 21, Dir::Up, 0, true}, {23, 20, Dir::Up, 1, true}, {23, 19, Dir::Up, 1, true}, {23, 18, Dir::Up, 1, true},
    {23, 17, Dir::Up, 1, true}, {23, 16, Dir::Up, 1, true}, {23, 15, Dir::Up, 1, true}, {23, 14, Dir::Up, 1, true},
    {23, 13, Dir::Up, 1, true}, {23, 12, Dir::Up, 0, true}, {23, 11, Dir::Up, 0, true}, {23, 10, Dir::Up, 0, true},
    {23, 9, Dir::Up, 0, true}, {23, 8, Dir::Up, 0, true}, {23, 8, Dir::Up, 0, false}, {23, 8, Dir::Up, 0, false},
    {23, 8, Dir::Up, 0, false}, {23, 8, Dir::Up, 0, false}, {23, 9, Dir::Down, 2, true}, {23, 10, Dir::Down, 2, true},
    {23, 11, Dir::Down, 2, true}, {23, 12, Dir::Down, 2, true}, {23, 13, Dir::Down, 2, true}, {23, 14, Dir::Down, 2, true},
    {23, 15, Dir::Down, 2, true}, {23, 16, Dir::Down, 3, true}, {23, 17, Dir::Down, 3, true}, {23, 18, Dir::Down, 3, true},
    {23, 19, Dir::Down, 3, true}, {23, 20, Dir::Down, 3, true}, {23, 21, Dir::Down, 3, true}, {23, 22, Dir::Down, 3, true},
    {23, 23, Dir::Down, 3, true}, {23, 24, Dir::Down, 2, true}, {23, 25, Dir::Down, 2, true}, {23, 26, Dir::Down, 2, true},
    {23, 27, Dir::Down, 2, true}, {23, 28, Dir::Down, 2, true}, {24, 28, Dir::Right, 6, true}, {25, 28, Dir::Right, 6, true},
    {26, 28, Dir::Right, 6, true}, {27, 28, Dir::Right, 7, true}, {28, 28, Dir::Right, 7, true}, {29, 28, Dir::Right, 7, true},
    {30, 28, Dir::Right, 7, true}, {31, 28, Dir::Right, 7, true}, {32, 28, Dir::Right, 7, true}, {33, 28, Dir::Right, 7, true},
}};

TEST(InputWalker, UnderTheDefaultsItWalksTheScriptedRunTickForTick) {
    const WalkerConfig config = Config();
    Vec2i at{32, 32};
    InputWalker walker{at};
    for (int tick = 0; tick < 200; ++tick) {
        walker.walk(config, Scripted(tick), kWalls);
        const Sample& expected = kScriptedRun[static_cast<std::size_t>(tick)];
        EXPECT_EQ(at, (Vec2i{expected.x, expected.y})) << "tick " << tick;
        EXPECT_EQ(walker.facing(), expected.facing) << "tick " << tick;
        EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{expected.slot}) << "tick " << tick;
        EXPECT_EQ(walker.moving(), expected.moving) << "tick " << tick;
    }
}

// ── 4-way movement ───────────────────────────────────────────────────────────────────────────

TEST(FourWay, AHeadingOnBothAxesMovesAlongTheFacedAxisElseTheHorizontal) {
    const WalkerConfig config = Config(Ways::Four, Ways::Four);
    Vec2i at{32, 32};
    Walker walker{at};  // facing down
    walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_EQ(at, (Vec2i{32, 33}));
    EXPECT_EQ(walker.facing(), Dir::Down);

    Vec2i otherAt{32, 32};
    Walker other{otherAt, Dir::Up};
    other.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_EQ(otherAt, (Vec2i{33, 32}));
    EXPECT_EQ(other.facing(), Dir::Right);
    for (int tick = 0; tick < 5; ++tick) {
        const Vec2i before = otherAt;
        other.walk(config, Directions{.down = true, .right = true}, kWalls);
        EXPECT_TRUE(otherAt.x == before.x || otherAt.y == before.y) << "tick " << tick;
    }
}

TEST(FourWay, ByInputTheMoreRecentPressWinsAndReleasingItFallsBack) {
    const WalkerConfig config = Config(Ways::Four, Ways::Four);
    Vec2i at{32, 32};
    InputWalker walker{at};
    walker.walk(config, Press({.right = true}), kWalls);
    EXPECT_EQ(at, (Vec2i{33, 32}));
    walker.walk(config, DirectionalInput{.held = {.down = true, .right = true}, .pressed = {.down = true}}, kWalls);
    EXPECT_EQ(at, (Vec2i{33, 33}));
    EXPECT_EQ(walker.facing(), Dir::Down);
    walker.walk(config, Hold({.down = true, .right = true}), kWalls);
    EXPECT_EQ(at, (Vec2i{33, 34}));
    walker.walk(config, Hold({.right = true}), kWalls);
    EXPECT_EQ(at, (Vec2i{34, 34}));
    EXPECT_EQ(walker.facing(), Dir::Right);

    Vec2i tieAt{32, 32};
    InputWalker tie{tieAt};
    tie.walk(config, Press({.down = true, .left = true}), kWalls);
    EXPECT_EQ(tieAt, (Vec2i{31, 32}));
    EXPECT_EQ(tie.facing(), Dir::Left);
}

// ── Sprite directions ────────────────────────────────────────────────────────────────────────

TEST(SpriteDirections, WithFourTheSpriteShowsTheFacingThroughADiagonal) {
    const WalkerConfig config = Config(Ways::Eight, Ways::Four);
    Vec2i at{32, 32};
    InputWalker walker{at, Dir::Right};
    walkTicks(walker, config, Press({.down = true, .right = true}), 3);
    EXPECT_EQ(walker.spriteFacing(config), Facing::Right);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(SpriteDirections, WithEightADiagonalShowsItsOwnAnimation) {
    const WalkerConfig config = Config(Ways::Eight, Ways::Eight);
    Vec2i at{24, 24};
    Walker walker{at};
    walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{14});
    for (int tick = 0; tick < 7; ++tick) {
        walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    }
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{15});
    walker.walk(config, Directions{}, kWalls);
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{14});  // standing: frame 0
    walker.walk(config, Directions{.right = true}, kWalls);
    EXPECT_EQ(walker.spriteFacing(config), Facing::Right);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
}

TEST(SpriteDirections, WithEightADiagonalWithNoFramesThrows) {
    WalkerConfig config = Config(Ways::Eight, Ways::Eight);
    config.walk.downRight = {};
    Vec2i at{24, 24};
    Walker walker{at};
    walker.walk(config, Directions{.down = true, .right = true}, kWalls);
    EXPECT_THROW((void)walker.frame(config), std::logic_error);
}

TEST(SpriteDirections, FacingTurnsWithoutMovingOrLosingThePlaceInTheStride) {
    const WalkerConfig config = Config(Ways::Eight, Ways::Eight);
    Vec2i at{24, 24};
    Walker walker{at};
    for (int tick = 0; tick < 8; ++tick) {
        walker.walk(config, Directions{.right = true}, kWalls);
    }
    walker.face(Dir::Left);
    EXPECT_EQ(walker.facing(), Dir::Left);
    EXPECT_EQ(walker.spriteFacing(config), Facing::Left);
    EXPECT_EQ(at, (Vec2i{32, 24}));
    EXPECT_TRUE(walker.moving());
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{5});   // the stride's frame 1

    walker.face(Facing::UpLeft);
    EXPECT_EQ(walker.facing(), Dir::Left);  // one of the two: kept
    EXPECT_EQ(walker.spriteFacing(config), Facing::UpLeft);
    walker.face(Facing::DownRight);
    EXPECT_EQ(walker.facing(), Dir::Right);  // neither: the horizontal one
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    walker.face(Facing::Up);
    EXPECT_EQ(walker.facing(), Dir::Up);
    EXPECT_EQ(at, (Vec2i{32, 24}));
}

// ── Being moved ──────────────────────────────────────────────────────────────────────────────

TEST(MoveTo, ItIsPutThereAndFacesAlongItsTravel) {
    const WalkerConfig config = Config(Ways::Eight, Ways::Eight);
    Vec2i at{16, 16};
    Walker walker{at, Dir::Up};
    walker.moveTo(config, {5, 5}, {1.0f, 0.0f});  // inside the wall: nothing collides
    EXPECT_EQ(at, (Vec2i{5, 5}));
    EXPECT_EQ(walker.facing(), Dir::Right);
    EXPECT_EQ(walker.spriteFacing(config), Facing::Right);
    EXPECT_TRUE(walker.moving());

    walker.moveTo(config, {5, 5}, {0.0f, 0.0f});
    EXPECT_FALSE(walker.moving());
    EXPECT_EQ(walker.facing(), Dir::Right);
    EXPECT_EQ(walker.spriteFacing(config), Facing::Right);

    // 0°, 20°, 30°, 45°, 60° and 90° below the horizontal.
    walker.moveTo(config, {5, 5}, {0.9397f, 0.3420f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::Right);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.moveTo(config, {5, 5}, {0.8660f, 0.5000f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    EXPECT_EQ(walker.facing(), Dir::Right);
    walker.moveTo(config, {5, 5}, {0.5000f, 0.8660f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    EXPECT_EQ(walker.facing(), Dir::Down);
    walker.moveTo(config, {5, 5}, {0.0f, 1.0f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::Down);
    EXPECT_EQ(walker.facing(), Dir::Down);
    walker.moveTo(config, {5, 5}, {0.5f, 0.5f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::DownRight);
    EXPECT_EQ(walker.facing(), Dir::Down);  // an exact tie keeps a facing that is one of the two
    walker.moveTo(config, {5, 5}, {-0.5f, -0.5f});
    EXPECT_EQ(walker.spriteFacing(config), Facing::UpLeft);
    EXPECT_EQ(walker.facing(), Dir::Left);  // and otherwise takes the horizontal one
}

TEST(MoveTo, TheStrideAdvancesWhileItTravelsHoweverLittle) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    Walker walker{at};
    for (int tick = 0; tick < 8; ++tick) {
        walker.moveTo(config, {16 + (tick + 1) / 2, 16}, {0.5f, 0.0f});  // half a pixel a tick
    }
    EXPECT_EQ(at, (Vec2i{20, 16}));
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{7});  // eight ticks: frame 1
}

// ── Routes ───────────────────────────────────────────────────────────────────────────────────

// The routes in these tests resolve at 60 ticks a second. Their legs are paced linearly over a whole
// number of ticks, so a leg of n pixels over n ticks is exactly a pixel a tick and each leg ends on the
// tick it says; a speed in pixels per second resolves through a float tick length and can end a tick
// later.
class Routes : public ::testing::Test {
protected:
    void SetUp() override {
        was_ = retropp::SpritePath::defaultTiming;
        retropp::SpritePath::defaultTiming = kHz60;
    }
    void TearDown() override { retropp::SpritePath::defaultTiming = was_; }

private:
    retropp::TimingProfile was_{};
};

// A leg's pace: the whole leg, linearly, over `ticks` ticks.
retropp::PathPacing Ticks(int ticks) {
    return retropp::PathPacing::eased(ticks * kOneTick, retropp::Easing::Linear);
}

TEST_F(Routes, ARouteSlowerThanAPixelATickStillWalks) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    Walker walker{at};
    retropp::SpritePath route = TopDownMovement::routeFrom(walker, {
        {.move = retropp::SpritePathMove::to({64.0f, 16.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    });
    for (int tick = 0; tick < 8; ++tick) {
        TopDownMovement::follow(walker, config, route);
        EXPECT_TRUE(walker.moving()) << "tick " << tick;
    }
    EXPECT_EQ(at, (Vec2i{20, 16}));  // half a pixel a tick
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{7});  // eight ticks: frame 1
}

TEST_F(Routes, AWalkerFollowsItsRouteFacingAlongItAndStandsThroughAWait) {
    const WalkerConfig config = Config();
    Vec2i at{16, 16};
    Walker walker{at};
    retropp::SpritePath route = TopDownMovement::routeFrom(walker, {
        {.label = "east", .move = retropp::SpritePathMove::to({40.0f, 16.0f}), .pacing = Ticks(24)},
        {.label = "wait", .move = retropp::SpritePathMove::to({40.0f, 16.0f}), .pacing = Ticks(10)},
        {.label = "north", .move = retropp::SpritePathMove::to({40.0f, 0.0f}), .pacing = Ticks(16)},
    });
    EXPECT_EQ(route.position(), (retropp::Vec2{16.0f, 16.0f}));

    const auto follow = [&](int ticks) {
        for (int tick = 0; tick < ticks; ++tick) {
            TopDownMovement::follow(walker, config, route, retropp::PlaybackMode::single());
        }
    };
    follow(1);
    EXPECT_EQ(at, (Vec2i{17, 16}));
    EXPECT_EQ(walker.facing(), Dir::Right);
    EXPECT_TRUE(walker.moving());
    follow(23);
    EXPECT_EQ(at, (Vec2i{40, 16}));
    follow(6);  // into the wait
    ASSERT_NE(route.currentNode(), nullptr);
    EXPECT_EQ(route.currentNode()->label, "wait");
    EXPECT_EQ(at, (Vec2i{40, 16}));
    EXPECT_FALSE(walker.moving());
    EXPECT_EQ(walker.facing(), Dir::Right);
    EXPECT_EQ(walker.frame(config).tileIndex, std::optional<std::size_t>{6});
    follow(5);  // on along the second leg
    EXPECT_EQ(at, (Vec2i{40, 15}));
    EXPECT_EQ(walker.facing(), Dir::Up);
    EXPECT_TRUE(walker.moving());
    follow(15);
    EXPECT_EQ(at, (Vec2i{40, 0}));
    EXPECT_TRUE(route.finished());
}

TEST_F(Routes, AnInterruptIsFollowedAndTheRouteResumesUnderEachPolicy) {
    const WalkerConfig config = Config();
    for (const retropp::ResumePolicy resume : {retropp::ResumePolicy::Continue, retropp::ResumePolicy::Return}) {
        Vec2i at{16, 16};
        Walker walker{at};
        retropp::SpritePath route = TopDownMovement::routeFrom(walker, {
            {.move = retropp::SpritePathMove::to({64.0f, 16.0f}), .pacing = Ticks(48)},
        });
        const auto follow = [&](int ticks) {
            for (int tick = 0; tick < ticks; ++tick) {
                TopDownMovement::follow(walker, config, route, retropp::PlaybackMode::single());
            }
        };
        follow(8);
        EXPECT_EQ(at, (Vec2i{24, 16}));

        route.interrupt({{.move = retropp::SpritePathMove::to({24.0f, 24.0f}), .pacing = Ticks(8)}},
                        retropp::PlaybackMode::single(), resume);
        follow(4);
        EXPECT_EQ(at, (Vec2i{24, 20}));
        EXPECT_EQ(walker.facing(), Dir::Down);
        follow(5);  // the detour ends and the route goes on
        EXPECT_FALSE(route.interrupted());
        EXPECT_EQ(walker.facing(), Dir::Right);
        const retropp::Vec2 onRoute = route.position();
        EXPECT_EQ(at, (Vec2i{static_cast<int>(std::lround(onRoute.x)), static_cast<int>(std::lround(onRoute.y))}));
        EXPECT_EQ(at.x, 25);
        if (resume == retropp::ResumePolicy::Continue) {
            EXPECT_GT(at.y, 16);  // carried on below, where the detour took it
        } else {
            EXPECT_EQ(at.y, 16);  // back on the row where it was interrupted
        }
    }
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
    Vec2i at{16, 16};
    InputWalker walker{at};
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
    Vec2i at{40, 30};
    const Walker walker{at};
    EXPECT_EQ(walker.anchor(config, "hand"), (retropp::Point{54, 36}));
    EXPECT_THROW((void)walker.anchor(config, "foot"), std::out_of_range);
    EXPECT_EQ(walker.collision(config).bounds(), (IntRect{.x = 40, .y = 30, .width = 8, .height = 8}));
}

// ── The sprite (resolved through a renderer on a GPU device) ─────────────────────────────────

// Windows on ARM has no production-representative GPU backend in CI; its production path (D3D12 + DXIL)
// is covered by the Windows x64 job, so a missing device there is a skip. Everywhere else it fails.
#if defined(_WIN32) && (defined(_M_ARM64) || defined(__aarch64__))
inline constexpr bool kDeviceOptional = true;
#else
inline constexpr bool kDeviceOptional = false;
#endif

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
            if (kDeviceOptional) {
                GTEST_SKIP() << "Windows on ARM has no production-representative GPU backend in CI; the "
                                "Windows x64 job covers the D3D12 path. (" << error_ << ")";
            }
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
    Vec2i at{40, 30};
    const Walker walker{at, Dir::Left};

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

TEST_F(WalkerSpriteOnDevice, ShowsTheSpritesDirectionAtTheBoundPositionForEveryPathway) {
    retropp::Renderer renderer{device_, nullptr};
    // 8 rows × 2 frames of 16×16, one row per animation in DirectionalAnimations' order.
    const std::vector<std::uint8_t> pixels(32 * 128, 1);
    const retropp::AtlasManifest sheet = renderer.uploadAtlas(
        pixels.data(), 32, 128, retropp::AssetDimensions{.width = 16, .height = 16},
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
    const WalkerConfig config{
        .sprites = Ways::Eight,
        .collision = IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
        .walk = {.up = clip(0), .down = clip(1), .left = clip(2), .right = clip(3),
                 .upLeft = clip(4), .upRight = clip(5), .downLeft = clip(6), .downRight = clip(7)},
        .timing = kHz60,
    };
    const auto open = [](IntRect) { return false; };

    Vec2i heroAt{40, 30};
    InputWalker hero{heroAt};
    hero.walk(config, Press({.down = true, .right = true}), open);
    const retropp::Sprite walked = TopDownMovement::WalkerSprite("hero", hero, config);
    EXPECT_EQ(walked.x, 41);
    EXPECT_EQ(walked.y, 31);
    EXPECT_EQ(walked.tile, sheet[14].tile);   // down-right, frame 0

    Vec2i guardAt{0, 0};
    Walker guard{guardAt};
    guard.moveTo(config, {12, 20}, {-1.0f, -1.0f});
    const retropp::Sprite moved = TopDownMovement::WalkerSprite("guard", guard, config);
    EXPECT_EQ(moved.x, 12);
    EXPECT_EQ(moved.y, 20);
    EXPECT_EQ(moved.tile, sheet[8].tile);     // up-left, frame 0
}

}  // namespace
