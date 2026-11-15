# Testing

How to test code that uses the module. Walking is deterministic: a walker's next state depends only on
its current state, the config, the input and the solids. There is no clock and no randomness, so a test
can call `walk` in a loop and assert the result.

The snippets use `assert`. Use your test framework's assertions in their place.

## Contents

- [What each part needs](#what-each-part-needs)
- [A test config](#a-test-config)
- [Test walking and facing](#test-walking-and-facing)
- [Test the walk animation](#test-the-walk-animation)
- [Test a route](#test-a-route)
- [Test `readDirections`](#test-readdirections)
- [Test `WalkerSprite`](#test-walkersprite)

## What each part needs

| Code under test | Needs |
|---|---|
| `Walker`, `InputWalker`, `CollisionShape`, `TileCollision`, `routeFrom`, `follow` | The engine library. No window, renderer or GPU device. |
| `readDirections` | A `retropp::InputState`, which a `retropp::RunLoop` provides. No window. |
| `WalkerSprite` | A `retropp::Renderer` on a GPU device, with the walker's sheet loaded. |

Keep `WalkerSprite` out of simulation code and most tests need nothing but the first row.

## A test config

```cpp
constexpr retropp::TimingProfile kHz60{.tickPeriodNs = retropp::TickPeriodNs::Hz60};
const std::chrono::nanoseconds kTick = kHz60.tickPeriod();

// A two-frame animation, 8 ticks per frame. The sheet and palette ids are made up: no renderer reads them.
retropp::Animation clip(std::string_view first, std::string_view second) {
    return retropp::Animation{{
        {.label = first, .sheet = retropp::AtlasId{1}, .tileIndex = 0, .palette = retropp::PaletteId{1}, .duration = 8 * kTick},
        {.label = second, .sheet = retropp::AtlasId{1}, .tileIndex = 1, .palette = retropp::PaletteId{1}, .duration = 8 * kTick},
    }};
}

const TopDownMovement::WalkerConfig config{
    .speed = 1,
    .collision = retropp::IntRect{.x = 0, .y = 0, .width = 8, .height = 8},
    .walk = {.up = clip("up-0", "up-1"), .down = clip("down-0", "down-1"),
             .left = clip("left-0", "left-1"), .right = clip("right-0", "right-1")},
    .timing = kHz60,
};
```

- **Set `timing` explicitly.** The default is a global the engine sets at startup. A test that sets it
  does not depend on what ran before.
- **Label the frames.** A test can then assert which frame shows by name.
- **Write durations in ticks.** `8 * kTick` is exactly 8 ticks at the config's timing.

## Test walking and facing

Use a lambda for the solids and a `DirectionalInput` written by hand:

```cpp
const auto open = [](retropp::IntRect) { return false; };
const auto wallPastX40 = [](retropp::IntRect box) { return box.x + box.width > 40; };

retropp::Vec2i at{16, 16};
TopDownMovement::InputWalker hero{at};

hero.walk(config, {.held = {.right = true}, .pressed = {.right = true}}, open);
assert((at == retropp::Vec2i{17, 16}));
assert(hero.facing() == retropp::Dir::Right);
assert(hero.moving());

// Walk into the wall: the 8-pixel-wide box stops with its right edge at x = 40.
for (int tick = 0; tick < 40; ++tick) {
    hero.walk(config, {.held = {.right = true}}, wallPastX40);
}
assert(at.x == 32);
assert(!hero.moving());
assert(hero.facing() == retropp::Dir::Right);
```

Assert on the position you bound (`at`). It is the same value as `hero.position()`.

For a map, build a `retropp::IndexGrid` in the test and read it through a `TileCollision`:

```cpp
enum class Cell : std::uint16_t { Open = 0, Wall = 1 };

retropp::IndexGrid room{.width = 10, .height = 10};
room.values.assign(100, static_cast<std::uint16_t>(Cell::Open));
room.values[3 * 10 + 5] = static_cast<std::uint16_t>(Cell::Wall);   // column 5, row 3

const TopDownMovement::TileCollision walls{.map = &room, .cellSize = 8, .open = Cell::Open};
assert(walls.solidAt({40, 24}));
assert(!walls.solidAt({39, 24}));
```

## Test the walk animation

`frame` returns the frame the walker shows. Reading its `label` or `tileIndex` needs no renderer.

```cpp
retropp::Vec2i at{16, 16};
TopDownMovement::InputWalker hero{at};

assert(hero.frame(config).label == "down-0");   // a new walker faces down and stands

for (int tick = 0; tick < 8; ++tick) {
    hero.walk(config, {.held = {.right = true}}, open);
}
assert(hero.frame(config).label == "right-1");  // 8 ticks of walking: the second frame

hero.walk(config, {}, open);
assert(hero.frame(config).label == "right-0");  // standing: frame 0
```

## Test a route

A route takes its timing from `retropp::SpritePath::defaultTiming` when it is constructed. Set it for
the test and restore it afterwards. Pace legs by duration so they end on a known tick
([routes.md](routes.md#engine-behavior-to-know)).

```cpp
const retropp::TimingProfile was = retropp::SpritePath::defaultTiming;
retropp::SpritePath::defaultTiming = kHz60;

retropp::Vec2i at{16, 16};
TopDownMovement::Walker guard{at};
retropp::SpritePath route = TopDownMovement::routeFrom(guard, {
    {.move = retropp::SpritePathMove::to({40.0f, 16.0f}), .pacing = retropp::PathPacing::eased(24 * kTick, retropp::Easing::Linear)},
});

for (int tick = 0; tick < 24; ++tick) {
    TopDownMovement::follow(guard, config, route, retropp::PlaybackMode::single());
}
assert((at == retropp::Vec2i{40, 16}));
assert(guard.facing() == retropp::Dir::Right);
assert(route.finished());

retropp::SpritePath::defaultTiming = was;
```

## Test `readDirections`

`readDirections` needs an `InputState`. A `RunLoop` on a clock you control produces one per tick with no
window:

```cpp
class ManualClock final : public retropp::Clock {
public:
    std::chrono::nanoseconds now() const noexcept override { return now_; }
    void advance(std::chrono::nanoseconds by) noexcept { now_ += by; }

private:
    std::chrono::nanoseconds now_{};
};

enum class Action : std::uint8_t { Up, Down, Left, Right };

ManualClock clock;
retropp::RunLoop loop{clock, kHz60};
TopDownMovement::DirectionalInput read;
loop.simTick([&](const retropp::InputState& input) {
    read = TopDownMovement::readDirections(
        input, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right});
});
loop.renderLoop([] {});
loop.advance();   // the first call runs no tick

retropp::InputSample sample;
sample.players[0].held.set(retropp::actionId(Action::Left), true);
loop.setRawInput(sample);
clock.advance(loop.tickPeriod());
loop.advance();   // one tick

assert((read == TopDownMovement::DirectionalInput{.held = {.left = true}, .pressed = {.left = true}}));
```

Test this once. In every other test, write the `DirectionalInput` by hand.

## Test `WalkerSprite`

`WalkerSprite` resolves the frame's art through a renderer, so it needs a GPU device and a loaded sheet.
A renderer constructed with no window is enough:

```cpp
// `device` is an SDL_GPUDevice* from SDL_CreateGPUDevice.
retropp::Renderer renderer{device, nullptr};

// A sheet of 4 rows × 2 frames of 16×16, every pixel index 1.
const std::vector<std::uint8_t> pixels(32 * 64, 1);
const retropp::AtlasManifest sheet = renderer.uploadAtlas(
    pixels.data(), 32, 64, retropp::AssetDimensions{.width = 16, .height = 16},
    retropp::ContentKind::AnimationSeries, retropp::ReadOrder::LeftRightThenDown, 0,
    retropp::TransparentIndices::of({0}), 2);

const auto row = [&](std::size_t n) {
    return retropp::Animation{{
        {.sheet = sheet.atlasId, .tileIndex = n * 2, .palette = retropp::PaletteId{1}, .duration = 8 * kTick},
        {.sheet = sheet.atlasId, .tileIndex = n * 2 + 1, .palette = retropp::PaletteId{1}, .duration = 8 * kTick},
    }};
};
const TopDownMovement::WalkerConfig spriteConfig{
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk = {.up = row(0), .down = row(1), .left = row(2), .right = row(3)},
    .timing = kHz60,
};

retropp::Vec2i at{40, 30};
const TopDownMovement::Walker walker{at, retropp::Dir::Left};
const retropp::Sprite sprite = TopDownMovement::WalkerSprite("hero", walker, spriteConfig);

assert(sprite.x == 40 && sprite.y == 30);
assert(sprite.z == 30 + 16);              // y plus the bottom edge of the collision box
assert(sprite.tile == sheet[4].tile);     // row 2 (left), frame 0
```

A machine with no GPU device cannot run this test. Keep it separate from the tests that need none.
