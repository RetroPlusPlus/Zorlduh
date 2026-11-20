# Getting started

This page walks through a complete program that uses the module: a farmyard of three areas — a yard, a
field and a pond — with a hero and the actors it meets.

| Actor | Where | What it does |
|---|---|---|
| The hero | starts in the yard | walks by the arrow keys, WASD or a d-pad, and presses Interact with X or a pad's A button |
| The bell | the yard | rings for a second when the hero presses Interact standing below it |
| The dog | the yard, by its kennel | sleeps until the hero comes near, and from then on trots to the bell and back when it rings |
| The gate | the yard's east fence | opens and shuts with Interact, and is solid while shut |
| The puddle | the yard | splashes while the hero stands in it |
| Four hens | the field | scatter when the camera comes into the field, then wander, and each lays three eggs |
| The eggs | where the hens lay them | are picked up when the hero walks onto them |
| The pond | south of the yard | ripples out from the hero's feet with every step the hero takes in it |

The source is [`examples/farm/farm.cpp`](../../examples/farm/farm.cpp), with the farm's names in
[`examples/farm/farm.h`](../../examples/farm/farm.h), its scripts in
[`examples/farm/scripts/`](../../examples/farm/scripts/), the pond's shader in
[`examples/farm/shaders/`](../../examples/farm/shaders/) and its art in
[`examples/farm/assets/`](../../examples/farm/assets/). The code on this page is taken from those
files.

## Contents

- [1. Build it](#1-build-it)
- [2. Run it](#2-run-it)
- [3. The art](#3-the-art)
- [4. The world](#4-the-world)
- [5. The farm's names](#5-the-farms-names)
- [6. The scripts](#6-the-scripts)
- [7. The input and the state](#7-the-input-and-the-state)
- [8. The actors](#8-the-actors)
- [9. The config](#9-the-config)
- [10. Spawning](#10-spawning)
- [11. The tick](#11-the-tick)
- [12. Drawing](#12-drawing)
- [13. The pond's ripples](#13-the-ponds-ripples)
- [14. Next](#14-next)

## 1. Build it

ActorKit is the target `ActorKit::ActorKit` of the WorldKit module. It needs the engine as the CMake
target `retropp::engine`, and the modules WorldKit is built on: LayerKit, CameraKit and ScriptKit. The
example also links TopDownMovement, whose walkers move its actors. The engine's
`docs/guide/build-and-consume.md` covers building the engine. Add ScriptKit and TopDownMovement before
WorldKit, and turn the examples on:

```cmake
add_subdirectory(engine)
add_subdirectory(modules/ScriptKit)
add_subdirectory(modules/LayerKit)
add_subdirectory(modules/TopDownMovement)
add_subdirectory(modules/CameraKit)
set(WORLDKIT_BUILD_EXAMPLES ON CACHE BOOL "" FORCE)
add_subdirectory(modules/WorldKit)
```

```sh
cmake -S . -B build
cmake --build build --target WorldKit-example-farm
```

The executable is written to `build/modules/WorldKit/`. Close the window to quit.

WorldKit's `CMakeLists.txt` calls the engine's `retropp_autocompile_shaders` for the example, which
compiles the pond's shader into the executable ([13. The pond's ripples](#13-the-ponds-ripples)).

## 2. Run it

The window shows a fenced yard: a kennel in the top-left corner with a dog asleep beside it, a bell at
the top, a puddle at the bottom-left, a gate in the east fence, a gap in the south fence, and the hero.

| Do this | Result |
|---|---|
| Hold a direction | The hero walks, and stops at the fence, the kennel, the bell, the dog and the shut gate. |
| Stand below the bell, face it and press X | The bell swings for a second. |
| Press X at the bell from its side | The bell stays still. |
| Walk to within 24 pixels of the dog | The dog wakes and stands up. |
| Ring the bell with the dog awake | The dog trots right, stops beside the bell for two seconds, and trots back to its kennel. |
| Stand in the puddle | The puddle splashes for as long as the hero stands in it, drawn under the hero's feet. |
| Face the gate and press X | The gate opens. Press X again and it shuts. |
| Walk through the open gate | The screen scrolls to the field. Four hens run for half a second, each its own way, then start to wander. |
| Wait in the field | Every four seconds each hen lays an egg, until it has laid three. |
| Walk onto an egg | The egg is removed, and an egg is added to the row along the top of the screen. |
| Leave the field and come back later | The hens have gone on wandering and laying while the camera was away. |
| Walk through the gap in the yard's south fence | The screen scrolls down to the pond. |
| Walk through the pond | Each step sends a ring of ripples out from the hero's feet. The rings of several steps spread over each other and fade. |

## 3. The art

`examples/farm/assets/` holds three sheets and fourteen palette images.

| File | Size | Contents |
|---|---|---|
| `tiles.png` | 24×8 | three 8×8 cells: grass, fence, kennel |
| `sprites.png` | 128×64 | 16×16 frames, eight to a row. A row each of walking frames for the hero, the dog and the hen: two facing down, two up, two left, two right. The last row holds the sleeping dog, the bell still and swinging, the gate shut and open, the puddle still and splashing, and the egg. |
| `pond.png` | 128×128 | one slot: the pond, 128×80, in its middle rows, with glints and dark specks on the water |
| `palettes/*.png` | 4×1 each | the colors of the grass, the field, the fence, the kennel, the hero, the dog, the bell, the gate, the puddle, the egg, and four hens |

The sheets are indexed PNGs: each pixel is a palette index, and a palette image supplies the colors.
The four hens share the hen's frames and differ in palette. The pond is drawn through the puddle's
palette.

The program reads the files from the directory named by `WORLDKIT_FARM_ASSETS`, which WorldKit's
`CMakeLists.txt` defines, and loads them with the engine's `loadAtlasFromMemory`:

```cpp
.sprites = renderer.loadAtlasFromMemory(readAsset("sprites.png"), retropp::AssetDimensions{.width = 16, .height = 16},
                                        retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                        /*count=*/32, retropp::TransparentIndices::of({0})),
.pond = renderer.loadAtlasFromMemory(readAsset("pond.png"), retropp::AssetDimensions{.width = 128, .height = 128},
                                     retropp::ContentKind::Tileset, retropp::ReadOrder::LeftRightThenDown,
                                     /*count=*/1, retropp::TransparentIndices::of({0})),
```

An enum names the sprite sheet's slots:

```cpp
enum Sprite : std::size_t {
    kHeroWalk = 0,
    kDogWalk = 8,
    kHenWalk = 16,
    kDogAsleep = 24,
    kBellStill,
    kBellSwinging,
    kGateShut,
    kGateOpen,
    kPuddleStill,
    kPuddleSplashing,
    kEgg,
};
```

The engine's `docs/guide/images-and-transparency.md` covers sheets and `docs/guide/tiles-and-color.md`
covers palettes.

## 4. The world

The world is WorldKit's: one layout of three areas, each with a ground layer, a collision map, two actor
slots and its openings.

```cpp
struct Maps {
    retropp::IndexGrid yard = Pen(20, 16, {{19, 7}, {19, 8}, {9, 15}, {10, 15}}, {{2, 2}, {3, 2}, {2, 3}, {3, 3}});
    retropp::IndexGrid field = Pen(40, 24, {{0, 11}, {0, 12}}, {});
    retropp::IndexGrid pond = Pen(20, 16, {{9, 0}, {10, 0}}, {});
};
```

`Pen(width, height, openings, kennel)` is the program's own: a map ringed by fence, with the cells of
`openings` opened in the fence and `kennel` built inside. The yard:

```cpp
WorldKit::Area{.layers = {{.name = "ground", .tiles = &maps.yard, .art = Ground(art, art.grass), .z = kGroundZ}},
               .actorSlots = {{.name = "underfoot", .z = kUnderfootZ}, {.name = "actors", .z = kActorsZ}},
               .collision = &maps.yard,
               .openings = {{.name = "gate", .side = Side::East, .span = {7, 8},
                             .transition = Transition::Scroll,
                             .leadsTo = {.area = Area::Field, .opening = "gate"}},
                            {.name = "gap", .side = Side::South, .span = {9, 10},
                             .transition = Transition::Scroll,
                             .leadsTo = {.area = Area::Pond, .opening = "gap"}}}},
```

| Area | Map | Openings |
|---|---|---|
| the yard | 20×16 cells of 8 pixels: one screen | a gate in the east fence to the field, a gap in the south fence to the pond |
| the field | 40×24: larger than the screen | a gate in the west fence back to the yard |
| the pond | 20×16: one screen | a gap in the north fence back to the yard |

Every area has the actor slots `"underfoot"` at depth 5 and `"actors"` at depth 10, over the ground at
depth 0. The slot names are what the actors put in `slot`, and the depths are where each slot's sprites
are drawn. WorldKit's `docs/getting-started.md` covers building a world.

## 5. The farm's names

`farm.h` declares what the scripts are written in. Every script file includes it.

```cpp
// The farm's values: one instance each, kept in its state and bound there.
inline const ScriptKit::Value<bool> GateOpen;    // the gate to the field stands open
inline const ScriptKit::Value<bool> BellRung;    // the bell is ringing

// The farm's one command: count an egg as picked up. A command is done once for every script that asks,
// so three eggs picked up on one tick count as three; farm.cpp has the handler.
inline const ScriptKit::Command Collect{"collect"};
inline ScriptKit::Entry collect() { return Collect(); }

// The farm's second command: ripple the asking actor's sprite from a point in the sprite's own art
// pixels, `x` across and `y` down from its top-left. farm.cpp has the handler.
inline const ScriptKit::Command Ripple{"ripple"};
inline ScriptKit::Entry ripple(ScriptKit::Expr<int> x, ScriptKit::Expr<int> y) { return Ripple(x, y); }
```

```cpp
// Values an actor owns: each actor that lists one in its ownValues holds its own instance.
inline const ScriptKit::Value<bool> Awake;            // the dog's
inline const ScriptKit::Value<bool> Splashing;        // the puddle's
inline const ScriptKit::Value<int> Laid;              // each hen's: the eggs it has laid
inline const ScriptKit::Value<retropp::Dir> Heading;  // the dog's and each hen's: where its walker heads
inline const ScriptKit::Value<int> SteppedX;          // the pond's: where the hero stood at its last ripple
inline const ScriptKit::Value<int> SteppedY;
```

| Kind | Names | Where the worth is kept |
|---|---|---|
| the farm's values | `GateOpen`, `BellRung` | in the program's state, one of each |
| values an actor owns | `Awake`, `Splashing`, `Laid`, `Heading`, `SteppedX`, `SteppedY` | in each actor that lists the value |
| commands | `Collect`, `Ripple` | the program's handlers: one adds to its egg count, the other starts a ripple |

The farm's scripts start on ActorKit's own triggers, which `farm.h` brings into its namespace:

```cpp
using ActorKit::Becomes;    // a value goes from false to true
using ActorKit::Enter;      // the camera comes into the actor's area
using ActorKit::Interact;   // the hero, facing the actor, presses Interact
using ActorKit::Near;       // the hero comes within a distance
using ActorKit::Touch;      // the hero walks onto the actor
```

[scripts.md](scripts.md#values) covers the kinds of value and
[triggers.md](triggers.md#actorkits-own-triggers) the triggers.

## 6. The scripts

Each script is a file in `examples/farm/scripts/`.

| Script | On | What it does |
|---|---|---|
| `ring` | `Interact`, from below | sets `BellRung`, waits a second, clears it |
| `wake` | `Near(24)` | sets the dog's `Awake` |
| `answer` | `Becomes(BellRung)`, when `Awake` | steers the dog to the bell, waits two seconds, steers it back |
| `toggle(value)` | `Interact` | flips the value it is handed |
| `pressWhile(pressed, held)` | `Touch` | sets `pressed`, waits while `held` holds, clears `pressed` |
| `scatter(flees)` | `Enter` | heads the hen one way for half a second |
| `wander(first)` | `Enter` | after a second, walks the hen round in legs, turning clockwise |
| `lay(egg)` | `Enter` | every four seconds spawns an egg, three in all |
| `pickUp` | `Touch` | asks for `Collect`, and removes the egg |
| `splash` | `Touch` | while the hero is in the pond, asks for `Ripple` at the hero's feet each time the hero has moved |

The bell's script is a constant:

```cpp
inline const ScriptKit::Script ring{
    .name = "ring",
    .on = Interact,
    .from = ActorKit::Below,
    .body = {
        set(BellRung, true), wait(1s), set(BellRung, false),
    },
};
```

`from` limits it to an actor standing below the bell. The bell's look reads `BellRung`, so the bell
swings for the second the value is true.

The dog's `answer` starts when `BellRung` becomes true, and only for a dog that is awake. It steers the
dog's walker through the dog's own `Heading`, and reads where the dog is from `ActorKit::X`:

```cpp
inline const ScriptKit::Script answer{
    .name = "answer",
    .on = Becomes(BellRung),
    .when = Awake,
    .body = {
        loopWhile(ActorKit::X < kBellX - 16, {set(Heading, retropp::Dir::Right), wait(1)}),
        set(Heading, retropp::Dir::None),
        wait(2s),
        loopWhile(ActorKit::X > kKennelX, {set(Heading, retropp::Dir::Left), wait(1)}),
        set(Heading, retropp::Dir::None),
    },
};
```

The bell and the dog share only the value: the bell's script sets `BellRung` and the dog's script
starts on it.

The puddle's script is a function, so that any actor can use it with values of its own:

```cpp
inline ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {
        .name = "press",
        .on = Touch,
        .body = {set(pressed, true), waitWhile(held), set(pressed, false)},
    };
}
```

The puddle lists `pressWhile(Splashing, ActorKit::Touched)`: touched, it sets its own `Splashing`, and
clears it when `ActorKit::Touched` turns false. [scripts.md](scripts.md#share-a-script-with-a-function)
covers script functions.

An egg's script counts the egg and removes it:

```cpp
inline const ScriptKit::Script pickUp{
    .name = "pick up",
    .on = Touch,
    .body = {collect(), ActorKit::remove()},
};
```

The count is a command so that every egg picked up on one tick is counted
([scripts.md](scripts.md#count-with-a-command)).

The pond's script runs for as long as the hero is in the water:

```cpp
inline const ScriptKit::Script splash{
    .name = "splash",
    .on = Touch,
    .body = {
        loopWhile(ActorKit::Touched,
                  {
                      when(ActorKit::PlayerX != SteppedX || ActorKit::PlayerY != SteppedY,
                           {
                               ripple(ActorKit::PlayerX + kFeetX - ActorKit::X,
                                      ActorKit::PlayerY + kFeetY - ActorKit::Y),
                               set(SteppedX, ActorKit::PlayerX),
                               set(SteppedY, ActorKit::PlayerY),
                           }),
                      wait(kStepTicks),
                  }),
    },
};
```

| Part | What it does |
|---|---|
| `loopWhile(ActorKit::Touched, …)` | Keeps going while the hero overlaps the pond, and ends when the hero steps out. The next step in starts it again. |
| `wait(kStepTicks)` | Looks once every 8 ticks: one frame of the hero's walk. |
| `when(… != SteppedX …)` | Ripples only when the hero has moved since the last ripple, so a hero standing still makes one. |
| `ActorKit::PlayerX + kFeetX - ActorKit::X` | The middle of the hero's feet, less the pond's own position: a point in the pond's art pixels. |

The hero is the player ([10. Spawning](#10-spawning)), so `PlayerX` and `PlayerY` are its position. The
script asks for the `Ripple` command, and the program's handler and shader draw the ripple
([13. The pond's ripples](#13-the-ponds-ripples)).

## 7. The input and the state

The program reads the engine's input once per tick into a plain value of its own:

```cpp
struct Input {
    TopDownMovement::DirectionalInput move;
    bool interact = false;
};

Input readInput(const retropp::InputState& in) {
    return Input{
        .move = TopDownMovement::readDirections(
            in, {.up = Action::Up, .down = Action::Down, .left = Action::Left, .right = Action::Right}),
        .interact = in.justPressed(Action::Interact),
    };
}
```

`move` drives the hero's walker and `interact` drives the hero's Interact action. `interact` is true on
the tick the button goes down, so one press performs the action once.

The state holds the actors, the camera, what the farm's values and commands are bound to, and the
ripples running on the pond:

```cpp
// A ripple running over one actor's sprite: from `center`, in the sprite's art pixels, for kRippleTicks.
struct Rippling {
    ActorKit::Id actor = ActorKit::kNoId;
    retropp::Point center{};
    int age = 0;   // ticks since it started
};

struct State {
    ActorKit::Actors actors;
    WorldKit::Camera camera{Layout::Farm, Area::Yard};
    int eggs = 0;
    bool gateOpen = false;
    bool bellRung = false;
    std::vector<Rippling> ripples;
};
```

## 8. The actors

Each actor is a value, made by a function from the loaded art.

The hero has a mover, an action and the triggers its body sets off:

```cpp
ActorKit::Actor Hero(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(
                Walk(art, kHeroWalk, art.hero, {.x = 3, .y = 10, .width = 10, .height = 6}), &Input::move),
            .actions = {{Action::Interact, &Input::interact}},
            .setsOff = {ActorKit::On::Touch, ActorKit::On::Near}};
}
```

| Field | Value | Meaning |
|---|---|---|
| `slot` | `"actors"` | drawn at the `"actors"` slot |
| `solid` | `true` | other walkers stop at the hero |
| `mover` | an `InputWalker`, driven by `Input::move` | the pad steers it. `Walk(…)` builds its `WalkerConfig`: the hero's row of the sheet, its palette, and a 10×6 collision box at its feet. |
| `actions` | Interact, driven by `Input::interact` | pressing X performs Interact on the actor the hero faces |
| `setsOff` | `Touch` and `Near` | the hero's body sets off the touch scripts of the puddle, the eggs and the pond, and the dog's near script |

The gate is an object: it has a box and an empty `mover`. Its solidity and its look both follow
`GateOpen`:

```cpp
ActorKit::Actor Gate(const Art& art) {
    return {.slot = "actors",
            .box = {.x = 0, .y = 0, .width = 8, .height = 16},
            .solid = !GateOpen,
            .look = {{GateOpen, LookOf(art, kGateOpen, art.gate)}, {LookOf(art, kGateShut, art.gate)}},
            .scripts = {toggle(GateOpen)}};
}
```

The dog owns two values, shows its sleeping look until it is awake, and is moved by a `Walker` that its
`Heading` drives:

```cpp
ActorKit::Actor Dog(const Art& art) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Awake, false}, {Heading, retropp::Dir::None}},
            .look = {{!Awake, LookOf(art, kDogAsleep, art.dog)}},
            .scripts = {&wake, &answer},
            .mover = ActorKit::movedBy<TopDownMovement::Walker>(
                Walk(art, kDogWalk, art.dog, {.x = 2, .y = 8, .width = 12, .height = 8}), Heading)};
}
```

While `Awake` is false the look applies. Once it is true, the dog shows its walker's frame.

A hen is a function of its colors and the way it flees, so the four hens are four calls:

```cpp
ActorKit::Actor Hen(const Art& art, retropp::PaletteId colors, retropp::Dir flees) {
    return {.slot = "actors",
            .solid = true,
            .ownValues = {{Heading, retropp::Dir::None}, {Laid, 0}},
            .scripts = {scatter(flees), wander(flees), lay(Egg(art))},
            .mover = ActorKit::movedBy<TopDownMovement::Walker>(
                Walk(art, kHenWalk, colors, {.x = 3, .y = 9, .width = 10, .height = 6}), Heading)};
}
```

The pond lies under the hero's feet, and its box is the water:

```cpp
ActorKit::Actor PondActor(const Art& art) {
    return {.slot = "underfoot",
            .box = {.x = 8, .y = 32, .width = 112, .height = 64},
            .ownValues = {{SteppedX, 0}, {SteppedY, 0}},
            .look = ActorKit::lookOf(art.pond, 0, art.puddle),
            .scripts = {&splash}};
}
```

The water is the middle 80 rows of the pond's 128-pixel art, so the box starts 32 pixels down.

The bell, the puddle and the egg are objects like the gate. The puddle's and the pond's `slot` is
`"underfoot"`, which is how they are drawn under the hero.

[actors.md](actors.md#actor) covers every field.

## 9. The config

```cpp
const ScriptKit::Bindings<State> bindings{
    {GateOpen, &State::gateOpen},
    {BellRung, &State::bellRung},
};
const ActorKit::Handlers<State> handlers{
    {Collect, [](State& state, ActorKit::Id, const ScriptKit::Arguments&) { ++state.eggs; }},
    // A new ripple over the asking actor's sprite. With kRipplesAtOnce already running there, the oldest
    // of them makes room.
    {Ripple, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments) {
         const auto mine = [asker](const Rippling& ripple) { return ripple.actor == asker; };
         if (static_cast<std::size_t>(std::count_if(state.ripples.begin(), state.ripples.end(), mine)) >=
             kRipplesAtOnce) {
             state.ripples.erase(std::find_if(state.ripples.begin(), state.ripples.end(), mine));
         }
         state.ripples.push_back({.actor = asker,
                                  .center = {.x = static_cast<float>(arguments.number(0)),
                                             .y = static_cast<float>(arguments.number(1))}});
     }},
};
const ActorKit::Config config{
    .world = &world, .interactAction = Action::Interact, .bindings = &bindings, .handlers = &handlers};
```

| Field | Value | Meaning |
|---|---|---|
| `world` | the farm's world | the areas, their collision maps and their slots |
| `interactAction` | `Action::Interact` | the input action `ActorKit::Interact` stands for |
| `bindings` | `GateOpen` and `BellRung`, each bound to a member of `State` | where the farm's two values are kept |
| `handlers` | `Collect` and `Ripple` | what the program does when an egg's script asks to be counted, and when the pond's script asks for a ripple |

The `Ripple` handler is handed the asking actor's id, the pond's, and records a new ripple on it at the
point the script worked out.

The config is built once and handed to every `tick` and `Sprites` call.

## 10. Spawning

```cpp
State state{.actors = ActorKit::Actors{engine}};
const ActorKit::Id hero = state.actors.spawn(Hero(art), {Layout::Farm, Area::Yard}, {72, 96});
state.actors.player(hero);
state.camera.place(world, state.actors.box(hero));
state.actors.spawn(Bell(art), {Layout::Farm, Area::Yard}, {kBellX, 16});
state.actors.spawn(Dog(art), {Layout::Farm, Area::Yard}, {kKennelX, 16});
state.actors.spawn(Gate(art), {Layout::Farm, Area::Yard}, {152, 56});
state.actors.spawn(Puddle(art), {Layout::Farm, Area::Yard}, {32, 96});
state.actors.spawn(PondActor(art), {Layout::Farm, Area::Pond}, {16, 4});
```

`spawn` takes the actor value, the place and the position of the actor's top-left in that area's
pixels, and returns the actor's id. The program keeps the hero's id.

- `state.actors.player(hero)` records the hero as the player: the one actor that walks out through the
  gate and the gap, and the one every script reads through `PlayerX` and `PlayerY`.
- `state.camera.place(…)` is WorldKit's. It puts the camera on the hero's box before the first tick.

The four hens are spawned into the field from the one function, each with its colors, the way it
flees and its position. They are live from the start. Their scripts start on `Enter`, when the camera
comes into the field.

## 11. The tick

```cpp
loop.simTick([&](const retropp::InputState& in) {
    AdvanceRipples(state.ripples);
    ActorKit::tick(state.actors, config, state.camera, readInput(in), state);

    // The camera follows the hero's box, and the hero goes where the camera says: carried through
    // the gate while the screen scrolls, and into the next area's pixels when it gets there.
    const WorldKit::Step step = state.camera.follow(world, state.actors.box(hero));
    const retropp::Vec2i at = state.actors.position(hero);
    state.actors.moveTo(hero, step.entered.value_or(state.actors.place(hero)),
                        {.x = at.x + step.move.x, .y = at.y + step.move.y});
    // On the tick the hero enters the next area its position changes to that area's pixels and its sprite
    // to that area's layer: shown fresh, the sprite mounts where it is, in place of easing across.
    if (step.entered) {
        state.actors.fresh(hero);
    }
});
```

**`AdvanceRipples`** ages every running ripple by a tick and drops the ones that have run their course
([13. The pond's ripples](#13-the-ponds-ripples)).

**`ActorKit::tick`** runs every actor: it writes what the scripts asked for on the tick before, walks
the hero by the pad and the dog and the hens by their `Heading`s, performs the hero's Interact,
detects the triggers and steps the scripts ([concepts.md](concepts.md#a-tick)).

**The camera lines** are the program's own WorldKit code. `camera.follow` follows the hero's box and
returns how far to carry the hero and, on the tick the camera changes area, the place it entered.
`moveTo` applies both. While the screen scrolls through an opening, `ActorKit::tick` holds every walker
still and these lines carry the hero across.

**`fresh`** is for the tick the hero lands in the next area. Its position jumps to that area's pixels
and its sprite moves to that area's layer, so the program reports it fresh and the engine shows it in
place ([drawing.md](drawing.md#fresh-keys)).

## 12. Drawing

```cpp
// The world's layers, then, for every area on screen — the camera's, and during a scroll the one
// scrolling in — its actors at each of its two slots.
frame.layers = WorldKit::WorldLayers(state.camera, world);
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(AreaSlot(world, shown, "underfoot", Underfoot(state, config, place, rippleShader)));
    frame.layers.push_back(
        AreaSlot(world, shown, "actors", ActorKit::Sprites(state.actors, config, place, "actors", state)));
}
frame.layers.push_back(LayerKit::SpriteLayer("hud:eggs", kHudZ, {EggRow(state.eggs, art)}));
frame.regions = WorldKit::Fade(state.camera, world);
renderer.renderFrame(frame);
```

`state.camera.shown(world)` lists the areas on screen: the camera's, and during a scroll the area
scrolling in, each with the scroll its layers take. For each, the program draws that area's actors at
each slot.

`ActorKit::Sprites` returns the sprites of one area's actors at one slot. `AreaSlot` puts them on a
layer of their own:

```cpp
retropp::DrawLayer AreaSlot(const WorldKit::World& world, const WorldKit::Shown& shown, std::string_view slot,
                            LayerKit::Sprites sprites) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    const std::string key =
        "farm:" + std::to_string(shown.layout) + ":" + std::to_string(shown.area) + ":" + std::string(slot);
    const CameraKit::View& view = world.layouts[static_cast<std::size_t>(shown.layout)].view;
    return LayerKit::SpriteLayer(key, WorldKit::ActorZ(world, place, slot), {std::move(sprites)},
                                 {.scroll = shown.scroll, .regions = WorldKit::ViewStencil(view)});
}
```

| Part | Why |
|---|---|
| a key naming the area and the slot | each area keeps its own layer, so a layer's scroll moves only with the camera |
| `WorldKit::ActorZ(world, place, slot)` | the slot's depth in that area, among its map layers |
| `.scroll = shown.scroll` | the area's own scroll, so an area scrolling in brings its actors with it |
| `WorldKit::ViewStencil(view)` | keeps the layer inside the part of the screen the world is drawn in |

[drawing.md](drawing.md#a-layer-for-each-area) explains the layer for each area.

`Underfoot` is `ActorKit::Sprites` for the `"underfoot"` slot, with the pond's ripples put on the pond's
sprite ([13. The pond's ripples](#13-the-ponds-ripples)).

Each sprite's `z` is the bottom of its actor's box, so the hero is drawn in front of the bell when it
stands below it and behind the bell when it stands above.

`EggRow` is the program's own component: one sprite along the top of the screen for each egg in
`state.eggs`.

## 13. The pond's ripples

A ripple goes from the pond's script to the screen in four steps.

| Step | Where | What happens |
|---|---|---|
| 1. The script asks. | `splash`, in the tick's step 6 | It asks for `Ripple` at the hero's feet, in the pond's art pixels. |
| 2. The handler records it. | the `Ripple` handler, in the next tick's step 1 | A `Rippling` for the pond is added to `state.ripples`, at age 0. |
| 3. The tick ages it. | `AdvanceRipples`, at the top of each tick | Each ripple ages a tick, and one 60 ticks old is dropped. |
| 4. The frame draws it. | `Underfoot`, each frame | The ripples running on the pond go onto the pond's sprite as one effect, which the ripple shader draws. |

The shader is the example's own, `examples/farm/shaders/ripple_ring.frag.hlsl`. The program registers
it by its path, and the build compiles it into the executable:

```cpp
const retropp::PostProcessStageId rippleShader =
    renderer.registerPostProcessStage("examples/farm/shaders/ripple_ring.frag.hlsl");
```

`Underfoot` puts every ripple running on an actor into one effect on its sprite:

```cpp
LayerKit::Sprites Underfoot(const State& state, const ActorKit::Config& config, WorldKit::Place here,
                            retropp::PostProcessStageId shader) {
    LayerKit::Sprites sprites = ActorKit::Sprites(state.actors, config, here, "underfoot", state);
    for (const ActorKit::Id id : state.actors.ids()) {
        const std::string key = state.actors.key(id);
        for (retropp::Sprite& sprite : sprites) {
            if (sprite.key.value != key) {
                continue;
            }
            if (const std::optional<retropp::ScreenSpaceEffect> ripples = RipplesOn(state, id, sprite.size, shader)) {
                sprite.effects.push_back(*ripples);
            }
        }
    }
    return sprites;
}
```

`RipplesOn` builds that effect: a `ScreenSpaceEffectKind::Custom` naming the shader, with up to four
ripples, each where the foot came down, how far its front has travelled and how strong it still is.

| The shader draws | From |
|---|---|
| a band of rings 6 pixels apart around each ripple's front | `ringWidth`, `ringSpacing` |
| the front moving outward a pixel a tick | the ripple's age |
| crests pushing the art out by whole pixels and lightening it, troughs darkening it | `ringPush`, `ringShade` |
| several ripples added together, the newest taking the oldest's place past four | `ringA` to `ringD` |
| the ripple fading over its 60 ticks | each ripple's strength |

- **One effect carries all of an actor's ripples.** A custom effect on a sprite reads the sprite's own
  art, so a second custom effect would draw over the first.
- **The pond's art is square.** The shader works in the sprite's own pixels on any size; the square
  sheet keeps the pond's look the same in every direction.
- **The ripples are the program's.** ActorKit tells the program where the hero's feet were, through
  `PlayerX` and `PlayerY` and the `Ripple` command. The program keeps the ripples and draws them.

The engine's `docs/guide/rendering.md`, "Custom shader stages", covers registering a shader, and its
`docs/guide/blend-modes.md`, "Custom effects on a sprite", covers running one on a sprite.

## 14. Next

| To learn | Read |
|---|---|
| How the parts fit together and what a tick does | [concepts.md](concepts.md) |
| Every field of an actor, and the set of actors | [actors.md](actors.md) |
| What starts a script, and triggers of your own | [triggers.md](triggers.md) |
| Values, commands and sharing a script | [scripts.md](scripts.md) |
| Movers and what drives them | [movers.md](movers.md) |
| Looks, slots and sprite keys | [drawing.md](drawing.md) |
| How to do a specific task | [how-to.md](how-to.md) |
| How to fix a problem | [troubleshooting.md](troubleshooting.md) |
| How to test code that uses the module | [testing.md](testing.md) |
