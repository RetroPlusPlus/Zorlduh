# Testing

How to test code that uses the module. A tick is deterministic: the actors' next state depends only on
their current state, the config, the camera, the input and your game state. A test spawns actors, calls
`tick` in a loop and asserts the result.

The snippets use `assert`. Use your test framework's assertions in their place.

## Contents

- [What each part needs](#what-each-part-needs)
- [A test world](#a-test-world)
- [A test state and config](#a-test-state-and-config)
- [Test a script](#test-a-script)
- [Test walking and solidity](#test-walking-and-solidity)
- [Test Interact](#test-interact)
- [Test a command](#test-a-command)
- [Test what an actor shows](#test-what-an-actor-shows)
- [Test a saved state](#test-a-saved-state)
- [Test a mover's frame](#test-a-movers-frame)

## What each part needs

| Code under test | Needs |
|---|---|
| `Actors`, `tick`, triggers, scripts, values, commands, movers, sprite keys | The engine library alone, and a `WorldKit::World` built in code. Runs headless. |
| `Sprites`, for actors shown by a look | The engine library alone. A `Look` can be filled in by hand. |
| `lookOf` | A `retropp::AtlasManifest`. One filled in by hand works. |
| `Sprites`, for an actor shown by its mover's frame | A `retropp::Renderer` on a GPU device, with the mover's sheet loaded. |

Give a test's walkers an unconditional look and every test falls in the first two rows: a look that
holds is shown in place of the mover's frame.

## A test world

`tick` needs a world for its collision maps and area sizes. Build a small one in the test:

```cpp
enum class Cell : std::uint16_t { Wall = 0, Open = 1 };

// A room of open cells ringed by wall.
retropp::IndexGrid room(int width, int height) {
    retropp::IndexGrid map{.width = width, .height = height};
    map.values.resize(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const bool edge = x == 0 || y == 0 || x == width - 1 || y == height - 1;
            map.values[static_cast<std::size_t>(y) * static_cast<std::size_t>(width) + static_cast<std::size_t>(x)] =
                static_cast<std::uint16_t>(edge ? Cell::Wall : Cell::Open);
        }
    }
    return map;
}
```

```cpp
const retropp::IndexGrid map = room(20, 16);   // 160 by 128 pixels, with cells of 8
const WorldKit::World world{.layouts = {WorldKit::Layout{
    .areas = {WorldKit::Area{.layers = {{.name = "ground", .tiles = &map}},
                             .actorSlots = {{.name = "actors", .z = 5}},
                             .collision = &map}},
    .cellSize = 8,
    .open = Cell::Open,
    .view = {.size = {.width = 160, .height = 128}},
}}};
WorldKit::validate(world);
```

- The map must outlive the world, and the world the config.
- One area is enough for most tests. Add a second area and an opening to test the player going
  through a door (WorldKit's `docs/areas.md`).
- The ground layer's art stays empty, since a headless test skips drawing the map.

## A test state and config

```cpp
enum class Action : std::uint8_t { Interact };

struct Input {
    TopDownMovement::DirectionalInput move;
    bool interact = false;
};

struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{0, 0};
    bool gateOpen = false;
    int coins = 0;
};
```

```cpp
retropp::EngineConfig engineConfig;
engineConfig.timing = retropp::TimingProfile{.tickPeriodNs = retropp::TickPeriodNs::Hz60};

const ScriptKit::Bindings<State> bindings{{GateOpen, &State::gateOpen}};
const ActorKit::Handlers<State> handlers{
    {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.coins; }},
};
const ActorKit::Config config{.world = &world, .interactAction = Action::Interact,
                              .bindings = &bindings, .handlers = &handlers};

State state{.actors = ActorKit::Actors{engineConfig}};
```

`GateOpen` is a `ScriptKit::Value<bool>` and `Collect` a `ScriptKit::Command`, both from the code under
test.

- **Set `timing` explicitly.** A script's `wait(1s)` is 60 ticks at this rate, whatever ran before the
  test.
- **Write the input by hand.** `Input{.move = {.held = {.right = true}}}` is a tick of holding right,
  and `Input{.interact = true}` a tick of pressing Interact.
- **A test walker** needs a `TopDownMovement::WalkerConfig` with a collision shape and its own
  `.timing` (TopDownMovement's `docs/testing.md`, "A test config").

The tests below use these actors:

```cpp
const ActorKit::Look shutLook{.size = {.width = 8, .height = 16}, .atlas = retropp::AtlasId{1}, .tile = 0};
const ActorKit::Look openLook{.size = {.width = 8, .height = 16}, .atlas = retropp::AtlasId{1}, .tile = 2};
const ActorKit::Look heroLook{.size = {.width = 8, .height = 8}, .atlas = retropp::AtlasId{1}, .tile = 4};

const TopDownMovement::WalkerConfig heroWalk{
    .collision = retropp::IntRect{.x = 0, .y = 0, .width = 8, .height = 8},
    .timing = engineConfig.timing,
};

const ScriptKit::Script open{.name = "open", .on = ActorKit::Interact, .body = {set(GateOpen, true)}};
const ScriptKit::Script pickUp{.name = "pick up", .on = ActorKit::Touch, .body = {Collect(), ActorKit::remove()}};

const ActorKit::Actor gate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                           .solid = !GateOpen,
                           .look = {{GateOpen, openLook}, {shutLook}},
                           .scripts = {&open}};
const ActorKit::Actor coin{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 8},
                           .scripts = {&pickUp}};
const ActorKit::Actor hero{.slot = "actors", .solid = true,
                           .look = heroLook,
                           .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
                           .actions = {{Action::Interact, &Input::interact}},
                           .setsOff = {ActorKit::On::Touch}};

const WorldKit::Place here{.layout = 0, .area = 0};
```

## Test a script

Spawn the actor, make its trigger happen, and tick twice: once for the script to start and ask, once
for the request to be written.

```cpp
const ActorKit::Id coinId = state.actors.spawn(coin, here, {40, 40});
const ActorKit::Id heroId = state.actors.spawn(hero, here, {100, 40});

ActorKit::tick(state.actors, config, state.camera, Input{}, state);
assert(state.actors.has(coinId));

state.actors.moveTo(heroId, here, {42, 42});                          // onto the coin
ActorKit::tick(state.actors, config, state.camera, Input{}, state);   // the touch starts the script
assert(state.actors.processor(coinId).idle());                        // it ran and ended on that tick
assert(state.coins == 0);

ActorKit::tick(state.actors, config, state.camera, Input{}, state);   // the requests are written
assert(state.coins == 1);
assert(!state.actors.has(coinId));
```

- Use `moveTo` to set a scene. It places an actor exactly.
- `Actors::processor(id)` reports where a script is: `running(script)`, `waiting(script)`, `idle()`.
- A script that sets a value and then reads one needs a third tick
  ([scripts.md](scripts.md#when-a-request-is-written)).

## Test walking and solidity

```cpp
const ActorKit::Id gateId = state.actors.spawn(gate, here, {60, 40});
state.actors.moveTo(heroId, here, {40, 44});

for (int tick = 0; tick < 30; ++tick) {
    ActorKit::tick(state.actors, config, state.camera, Input{.move = {.held = {.right = true}}}, state);
}
assert((state.actors.position(heroId) == retropp::Vec2i{52, 44}));   // the 8-pixel box stops at the gate's x = 60
assert(state.actors.facing(heroId) == retropp::Dir::Right);
```

Assert on `position`, `box` and `facing`. For a wall, walk into the edge of the room: its first open
pixel is 8.

## Test Interact

Face the actor, press, and allow the two ticks:

```cpp
ActorKit::tick(state.actors, config, state.camera, Input{.interact = true}, state);   // the press
assert(!state.gateOpen);
ActorKit::tick(state.actors, config, state.camera, Input{.move = {.held = {.right = true}}}, state);
assert(state.gateOpen);                                                               // written
assert((state.actors.position(heroId) == retropp::Vec2i{53, 44}));                    // and the gate lets it through
(void)gateId;
```

The hero already faces right, from walking into the gate. A walker turns to face what it walks into,
so a tick of walking into something solid sets the facing and leaves the position as it is.

## Test a command

A handler changes your state, so assert on the state. The coin test above asserts `state.coins`. To
test the rule that a command is carried out once for every actor that asks:

```cpp
State many{.actors = ActorKit::Actors{engineConfig}};
for (int n = 0; n < 3; ++n) {
    many.actors.spawn(coin, here, {40 + n * 8, 40});
}
many.actors.spawn(ActorKit::Actor{.box = {.x = 0, .y = 0, .width = 24, .height = 8},
                                  .setsOff = {ActorKit::On::Touch}},
                  here, {40, 40});   // one wide actor touching all three coins

for (int tick = 0; tick < 3; ++tick) {
    ActorKit::tick(many.actors, config, many.camera, Input{}, many);
}
assert(many.coins == 3);
assert(many.actors.size() == 1);
```

## Test what an actor shows

`Sprites` returns plain `retropp::Sprite` values. For an actor shown by a look, it runs headless:

```cpp
state.gateOpen = false;
LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, here, "actors", state);
assert(sprites.size() == 2);   // the hero and the gate, in spawn order

const retropp::Sprite& heroSprite = sprites[0];
const retropp::Sprite& gateSprite = sprites[1];
assert(heroSprite.key.value == state.actors.key(heroId));
assert(heroSprite.tile == heroLook.tile);
assert(heroSprite.z == 44 + 8);   // its y plus the bottom of its box
assert(gateSprite.tile == shutLook.tile);

state.gateOpen = true;
sprites = ActorKit::Sprites(state.actors, config, here, "actors", state);
assert(sprites[1].tile == openLook.tile);
```

- A `Look` filled in by hand takes any `atlas` and `tile`. The test compares the fields.
- Find an actor's sprite by its key, `Actors::key(id)`, when the order is unknown.
- The hero's `heroLook` is unconditional, so `Sprites` shows that and leaves the walker's frame
  unread.

## Test a saved state

An `Actors` value copies whole, so a test can run a copy forward and compare:

```cpp
State saved = state;

for (int tick = 0; tick < 5; ++tick) {
    ActorKit::tick(state.actors, config, state.camera, Input{.move = {.held = {.down = true}}}, state);
}
assert((state.actors.position(heroId) == retropp::Vec2i{53, 49}));
assert((saved.actors.position(heroId) == retropp::Vec2i{53, 44}));   // the copy stayed

for (int tick = 0; tick < 5; ++tick) {
    ActorKit::tick(saved.actors, config, saved.camera, Input{.move = {.held = {.down = true}}}, saved);
}
assert((saved.actors.position(heroId) == state.actors.position(heroId)));   // the same input gives the same result
```

## Test a mover's frame

An actor with an empty `look` shows its mover's frame, and the frame's art is resolved through the
renderer that loaded the sheet. That test needs a GPU device and a loaded sheet. A renderer constructed
with a null window is enough:

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
const TopDownMovement::WalkerConfig walk{
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk = {.up = row(0), .down = row(1), .left = row(2), .right = row(3)},
    .timing = engineConfig.timing,
};

State drawn{.actors = ActorKit::Actors{engineConfig}};
const ActorKit::Id walker = drawn.actors.spawn(
    ActorKit::Actor{.slot = "actors", .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(walk, &Input::move)},
    here, {40, 30});

const LayerKit::Sprites frames = ActorKit::Sprites(drawn.actors, config, here, "actors", drawn);
assert(frames.size() == 1);
assert(frames[0].key.value == drawn.actors.key(walker));
assert(frames[0].tile == sheet[2].tile);   // row 1 (down), frame 0: a new walker faces down and stands
assert(frames[0].z == 30 + 16);            // y plus the bottom edge of the collision box
```

`kTick` is the tick period, `engineConfig.timing.tickPeriod()`. This test requires a GPU device. Keep
it in its own target, apart from the headless tests.
