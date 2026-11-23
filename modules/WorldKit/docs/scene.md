# Scene

A `WorldKit::Scene` is your world running: the camera you hand it, every actor your world lists, and
the calls that tick them, follow one with the camera and draw them. This page covers what the scene does
each tick, how its actors go through openings, when they are awake, how it draws, and every type and
member of `scene.h`.

```cpp
#include "WorldKit/scene.h"   // Scene, SceneActor, SceneLayerKey
```

`scene.h` includes ActorKit's headers, so a game that includes it links `ActorKit::ActorKit`. A game
with a system of its own for what lives in its world includes the other WorldKit headers alone.

## Contents

- [Overview](#overview)
- [What the scene does in a tick](#what-the-scene-does-in-a-tick)
- [Where an actor goes through an opening](#where-an-actor-goes-through-an-opening)
- [The edge of a reach](#the-edge-of-a-reach)
- [Awake or frozen](#awake-or-frozen)
- [Drawing](#drawing)
- [The level beneath](#the-level-beneath)
- [`Scene`](#scene-1)
- [`SceneActor`](#sceneactor)
- [`SceneLayerKey`](#scenelayerkey)
- [Exceptions](#exceptions)
- [Lifetimes and copying](#lifetimes-and-copying)

## Overview

```cpp
enum Layout : std::uint8_t { Castle };
enum CastleArea : std::uint8_t { Hall, WideHall };

// The world, once: its areas, and who starts where. Where an actor is written is how far it may roam.
const WorldKit::World world{
    .layouts = {WorldKit::Layout{
        .name = "castle",
        .areas = {
            WorldKit::Area{.name = "hall",
                           .layers = {{.name = "ground", .tiles = &maps.hall, .art = stone}},
                           .actorSlots = {{.name = "actors", .z = 10}},
                           .collision = &maps.hall,
                           .openings = {{.name = "east door", .side = WorldKit::Side::East, .span = {6, 9},
                                         .leadsTo = {.area = WideHall, .opening = "west door"}}},
                           .actors = {{.actor = Guard(art), .at = {40, 64}}}},          // stays in the hall
            WorldKit::Area{.name = "wide hall",
                           .layers = {{.name = "ground", .tiles = &maps.wideHall, .art = stone}},
                           .actorSlots = {{.name = "actors", .z = 10}},
                           .collision = &maps.wideHall,
                           .openings = {{.name = "west door", .side = WorldKit::Side::West, .span = {6, 9},
                                         .leadsTo = {.area = Hall, .opening = "east door"}}}},
        },
        .actors = {{.actor = Hound(art), .in = WideHall, .at = {120, 80}}},        // roams the castle
        .cellSize = 8,
        .open = Cell::Open,
        .view = kView}},
    .actors = {{.name = "marker", .actor = Marker(art), .in = {Castle, Hall}, .at = {76, 68}}},   // goes anywhere
};
WorldKit::validate(world);

// The scene, once: the camera you create, handed over, and the actor it follows.
struct State {
    WorldKit::Scene scene;
};
State state{.scene = WorldKit::Scene{world, WorldKit::Camera{Castle, Hall}, config}};
state.scene.follow("marker");

// Each tick, and each frame.
loop.simTick([&](const retropp::InputState& in) { state.scene.tick(readInput(in), state); });
loop.renderLoop([&] {
    frame.layers = state.scene.layers(state);
    frame.regions = state.scene.fade();
    renderer.renderFrame(frame);
});
```

| Name | What it is in your program |
|---|---|
| `maps` | your loaded maps, `retropp::IndexGrid`s that outlive the world |
| `stone` | a `retropp::TileCatalog` for the ground |
| `Guard`, `Hound`, `Marker`, `art` | your functions returning an `ActorKit::Actor`, and what they make the actor from |
| `Cell::Open` | the open value of your collision maps |
| `kView` | your `CameraKit::View` |
| `config` | your `ActorKit::Config`, with its `world` left out: the interact action, your triggers, values and commands |
| `readInput` | your function from the engine's input to your game's input value |
| `loop`, `renderer`, `frame` | your `retropp::RunLoop`, `retropp::Renderer` and `retropp::FrameDrawState` |

1. **The world lists its actors** at three levels: `Area::actors`, `Layout::actors` and `World::actors`
   ([areas.md](areas.md#areaactor-layoutactor-worldactor)). The level is the actor's reach.
2. **The scene spawns them** into one `ActorKit::Actors` set when it is built, in the order the world is
   written.
3. **`follow`** names the actor the camera follows. The camera carries it through the openings.
4. **`tick`** runs every actor and the camera, once a tick.
5. **`layers` and `fade`** are the frame.

The scene is a value in your game state, like any other. Everything below it stays in reach:
`state.scene.camera()` is the camera, and `state.scene.actors()` is the ActorKit set
([The level beneath](#the-level-beneath)).

## What the scene does in a tick

`tick(input, state)` takes your input value and your state, as `ActorKit::tick` does, and returns the
camera's [`Step`](camera.md#step), whose `entered` tells your game when the camera changes area. Its steps,
in order:

| Step | What happens |
|---|---|
| 1. New actors | Every live actor new to the scene — one a script spawned, or one you spawned through `actors()` — takes `Reach::Area`, `AtEdge::Stop` and `AwakeWhen::Always`. |
| 2. Back from hiding | A hidden actor your game has put in a place of the world comes back, shown fresh. |
| 3. The camera's place | When the followed actor is in another place than the camera, the camera goes there, on the actor's box, and the actor is shown fresh. |
| 4. Awake or frozen | Each actor wakes or freezes by its rule, for the areas the camera shows as the tick begins ([Awake or frozen](#awake-or-frozen)). |
| 5. Boxes | Each awake actor's box is noted. |
| 6. ActorKit | `ActorKit::tick` runs every awake actor, against the camera. |
| 7. The followed actor | The camera follows its box, and the actor goes where the camera says. |
| 8. Every other actor | Each awake actor that went through an opening this tick goes on by the opening's transition, or meets the edge of its reach ([below](#where-an-actor-goes-through-an-opening)). |

To hold the whole world still, under a menu or a message, leave `tick` out for those ticks. The world
stays exactly as it is, and the frame draws it.

## Where an actor goes through an opening

An actor goes through an opening when its walk takes its center out through a gap in a wall, or onto a
spot from outside it.

**The followed actor** goes through every opening that leads somewhere, and the camera takes it through:
a scroll carries it the layout's `carry` in while the screen slides, a fade lands it on the entrance at
the middle, and a seam takes it into the next area's pixels with the picture holding still
([camera.md](camera.md#camera)). It is shown fresh on the tick it enters the next area.

**Every other actor** goes through an opening inside its reach by the opening's `transition`, on the tick
its center crosses:

| The opening's `transition` | Where the actor goes |
|---|---|
| `Seamless` | It walks on into the next area, into that area's pixels. |
| `Scroll` | It stands the layout's `carry` pixels inside the next area's entrance, at its place along the opening. |
| `Fade` | It stands on the next area's entrance: centered on a spot, or one cell in from a wall's gap and centered along it. |

The screen holds still for it, and the camera stays where it is. It is shown fresh on that tick, so its
sprite appears where it lands.

| Reach | The openings inside it |
|---|---|
| `Reach::Area` | its area's openings all lead outside it |
| `Reach::Layout` | those that lead to an area of the actor's own layout |
| `Reach::World` | every opening that leads somewhere |

An opening outside its reach is [the edge of its reach](#the-edge-of-a-reach).

Watch for:

- **Nothing walks while a scroll or a fade runs.** That is ActorKit's rule for every actor
  ([actors/actors.md](actors/actors.md#tick)). An awake actor's scripts still run.
- **The followed actor's reach and `atEdge` wait.** While the camera follows it, it goes through every
  opening that leads somewhere, as the player does. Following another actor hands it back to its own.

## The edge of a reach

An actor's edge is an opening that leads outside its reach. What it does there is its `atEdge`:

| `atEdge` | At a gap in a wall (a door, a seam) | At a spot (stairs, a cave mouth) |
|---|---|---|
| `AtEdge::Stop` (the default) | It is held there, its box inside its area. | It walks across the spot and stays in its area. |
| `AtEdge::Despawn` | It walks out through the gap and is removed on the tick its center crosses. | It is removed on the tick its center steps onto the spot. |
| `AtEdge::Hide` | It walks out through the gap and is hidden on the tick its center crosses. | It is hidden on the tick its center steps onto the spot. |

**A hidden actor is out of the world: not drawn, not ticked, not solid, not touchable, until your game
puts it somewhere again.** It keeps its id, its values and where its scripts are; the ticks left in a
wait hold. Put it back with `actors().moveTo`:

```cpp
if (state.scene.actor(bat).hidden()) {
    state.scene.actors().moveTo(bat, {Castle, Cellar}, {24, 24});   // back on the next tick
}
```

`bat` is an `ActorKit::Id`, and `Cellar` your area number. On the next tick the actor is in the world
again, awake by its rule, and shown fresh.

A removed actor's id is retired. To bring one back, spawn it again from its entry with
[`spawn`](#spawn).

## Awake or frozen

Every actor of a scene is awake or frozen. An awake actor takes part in each tick. A frozen actor stays
exactly as it is: its position, its values and where its scripts are all hold, and its waits keep the
ticks they had left. It is still there, drawn if it is on screen and solid if it was solid, and it carries
on from exactly there when it wakes.

Its `awakeWhen` is the rule:

| `awakeWhen` | The actor is awake |
|---|---|
| `AwakeWhen::Always` (the default) | all the time, wherever the camera is |
| `AwakeWhen::InLayout` | while any area of the layout it is in is on screen |
| `AwakeWhen::OnScreen` | while the area it is in is on screen |

On screen is what the camera shows: its own area, the areas across seams that are in view, and the area
scrolling in. Two layouts can be on screen at once, across a seam between them. The rule is applied at
the start of each tick, for the areas on screen then.

```cpp
.actors = {{.actor = Bat(art), .at = {24, 24}, .awakeWhen = WorldKit::AwakeWhen::OnScreen}},   // flies while the cellar is shown
```

`freeze()` and `wake()` command it by hand:

| Call | Result |
|---|---|
| `scene.actor(x).freeze()` | Frozen at once, and from then on, whatever its rule says. |
| `scene.actor(x).wake()` | The freeze by hand is lifted. It wakes at once if its rule holds for the areas on screen now and it is in the world; otherwise it stays frozen until its rule holds. |
| `scene.actor(x).awakeWhen(…)` | The new rule decides from the next tick. |
| `AtEdge::Hide` | Frozen while it is hidden, whatever its rule. |

`frozen()` reports whether it is frozen for any of the three reasons, and `awake()` is its opposite.

## Drawing

`layers(state)` is the frame's layers: [`WorldLayers`](drawing.md#worldlayers), then, for each area the
camera shows and each actor slot of that area, one sprite layer:

| Of each sprite layer | It is |
|---|---|
| the key | [`SceneLayerKey`](#scenelayerkey)`(layout, area, slot)`: `"scene:<layout>:<area>:<slot>"` |
| the depth | `ActorZ(world, shown, slot)`, the slot's depth for that area on screen ([camera.md](camera.md#actorz-layerz-by-shown)) |
| the scroll | that area's, so a sprite in that area's pixels lands on its map |
| the regions | [`ViewStencil`](drawing.md#viewstencil) of the current layout's view |
| the sprites | `ActorKit::Sprites` of that area and slot, each keyed and at the depth of its feet, as ActorKit reports them ([actors/drawing.md](actors/drawing.md)) |

Every slot gets its layer, empty or with sprites, so every layer keeps its key from frame to frame. A
hidden actor is outside every area, so every layer leaves it out.

To change a sprite on its way to the screen, or add sprites of your own at a slot, hand `layers` a
function:

```cpp
frame.layers = state.scene.layers(state, [&](WorldKit::Place place, std::string_view slot, LayerKit::Sprites sprites) {
    if (slot == "underfoot" && place == pond) {
        sprites.push_back(Ripple(state));   // a sprite of your own among the actors'
    }
    return sprites;
});
```

`pond` is a `WorldKit::Place` of yours and `Ripple` your component returning a `retropp::Sprite`. The
function is called once for each area on screen and each of its slots, with that area's sprites in that
area's pixels, and what it returns is drawn at that slot.

`fade()` is the fade over the screen while one runs ([`Fade`](drawing.md#fade)).

## The level beneath

The scene is built from calls your game can make by hand, and every one of them stays yours:

| Through | You have |
|---|---|
| `scene.camera()` | every `Camera` call: `place`, `position`, `scroll`, `shown`, `fade`, and `follow(world, box)` for a camera you drive by hand ([camera.md](camera.md)) |
| `scene.actors()` | every `ActorKit::Actors` call on the scene's actors, with the ids the scene hands out: `spawn`, `remove`, `moveTo`, `get`, `set`, `act`, `box`, `key`, `fresh` ([actors/actors.md](actors/actors.md#actors)) |
| `scene.world()`, `scene.config()` | the world, and the config the scene hands ActorKit |

Three things the scene keeps for you, through calls of its own:

| The scene keeps | Set it with | In place of |
|---|---|---|
| the actor the camera follows | `scene.follow` | `actors().player`, which it sets |
| each actor's confinement, from its reach and `atEdge` | `scene.actor(x).reach`, `.atEdge` | `actors().confinement` |
| each actor's freeze, from its rule and the freeze by hand | `scene.actor(x).awakeWhen`, `.freeze`, `.wake` | `actors().freeze` and `wake`, which the scene applies again at the start of each tick |

Drive the camera by hand beside the scene and the scene carries on from where you put it:

```cpp
state.scene.camera().place(world, state.scene.actors().box(state.scene.following()));
```

## `Scene`

```cpp
class Scene {
public:
    Scene(const World& world, Camera camera, ActorKit::Config config);
    Scene(const World& world, Camera camera);

    void follow(ActorKit::Id id);
    void follow(std::string_view name);
    ActorKit::Id following() const noexcept;

    SceneActor actor(ActorKit::Id id);
    SceneActor actor(std::string_view name);

    ActorKit::Id spawn(const AreaActor& entry, Place place);
    ActorKit::Id spawn(const LayoutActor& entry, int layout);
    ActorKit::Id spawn(const WorldActor& entry);

    template <class State, class Input> Step tick(const Input& input, State& state);
    template <class State> LayerKit::Layers layers(const State& state) const;
    template <class State, class Dress> LayerKit::Layers layers(const State& state, Dress&& dress) const;
    LayerKit::Regions fade() const;

    Camera& camera() noexcept;
    const Camera& camera() const noexcept;
    ActorKit::Actors& actors() noexcept;
    const ActorKit::Actors& actors() const noexcept;
    const World& world() const noexcept;
    const ActorKit::Config& config() const noexcept;
};
```

The world running. Keep it in your game state and call `tick` once a tick and `layers` and `fade` once a
frame.

### Constructors

```cpp
Scene(const World& world, Camera camera, ActorKit::Config config);
Scene(const World& world, Camera camera);
```

Builds the scene of `world`, with `camera` handed over and `config` kept with its `world` set to `world`.
The second form is for actors that use the world alone: its config is `ActorKit::Config{}` with the
world filled in.

```cpp
State state{.scene = WorldKit::Scene{world, WorldKit::Camera{Castle, Hall}, config}};
```

Every actor the world lists is spawned, in the order the world is written: for each layout, each area's
`actors` and then the layout's; then the world's. Ids follow that order, from 1.

| Entry | Its place | Its reach |
|---|---|---|
| `AreaActor`, in an area's `actors` | that area | `Reach::Area` |
| `LayoutActor`, in a layout's `actors` | area `in` of that layout | `Reach::Layout` |
| `WorldActor`, in the world's `actors` | the place `in` | `Reach::World` |

Each also takes its entry's `atEdge` and `awakeWhen`, and its `name`. The scripts' waits are timed by the
engine's active config, `retropp::EngineConfig::active`, which `EngineConfig::setActive` sets.

Watch for:

- **Call `validate` first.** The scene reads the world as `validate` accepts it.
- **Place the camera first**, with `camera.place(world, box)`, for a first frame that looks at the
  followed actor; otherwise the camera comes to it on the first tick.

Throws `std::logic_error`, `WorldKit::Scene: actor '…' is not an ActorKit::Actor`, for an entry whose
`actor` holds another type. What `ActorKit::Actors::spawn` throws passes through.

### `follow` and `following`

```cpp
void follow(ActorKit::Id id);
void follow(std::string_view name);
ActorKit::Id following() const noexcept;
```

`follow` names the actor the camera follows, by id or by its entry's name; `following` returns it, or
`kNoId`. Call `follow` once; call it again with another actor and the camera follows that one.

```cpp
state.scene.follow("marker");
const ActorKit::Id marker = state.scene.following();
```

| While an actor is followed | What it means |
|---|---|
| the camera | follows its box each tick and takes it through every opening that leads somewhere |
| its reach and `atEdge` | wait until another actor is followed |
| `actors().player()` | is that actor, so `PlayerHere`, `PlayerX` and `PlayerY` are its |
| it is removed | `following()` is `kNoId`; a scroll or fade already running runs to its end |
| `follow(ActorKit::kNoId)` | the camera stays where it is until the next `follow` |

Throws `std::out_of_range` for any id but a live actor's, and `WorldKit::Scene: no actor is named '…'` for
any name but a live actor's.

### `actor`

```cpp
SceneActor actor(ActorKit::Id id);
SceneActor actor(std::string_view name);
```

The actor, for the scene's settings of it ([`SceneActor`](#sceneactor)). Every call about one actor's place
in the world hangs off it.

```cpp
state.scene.actor(asker).reach(WorldKit::Reach::World);   // in a handler: "this is now a global actor"
const ActorKit::Id hound = state.scene.actor("hound").id();
```

`asker` is the id a handler is handed. A name is an entry's `name`, or one `spawn` gave; an unnamed
actor is found by its id.

Throws `std::out_of_range`, naming the id, for any id but a live actor's, and `WorldKit::Scene: no actor is
named '…'` for any name but a live actor's.

### `spawn`

```cpp
ActorKit::Id spawn(const AreaActor& entry, Place place);
ActorKit::Id spawn(const LayoutActor& entry, int layout);
ActorKit::Id spawn(const WorldActor& entry);
```

Spawns the actor of `entry` while the game runs and returns its id. Which entry type you hand it is the
reach, as which level an entry is written at is in the world.

```cpp
const ActorKit::Id bat = state.scene.spawn(WorldKit::AreaActor{.actor = Bat(art), .at = {24, 24}}, {Castle, Cellar});
```

| Form | Its place | Its reach |
|---|---|---|
| `spawn(AreaActor, place)` | `place` | `Reach::Area` |
| `spawn(LayoutActor, layout)` | area `entry.in` of `layout` | `Reach::Layout` |
| `spawn(WorldActor)` | `entry.in` | `Reach::World` |

The actor takes part from the next tick. An actor a script spawns, or one you spawn through
`actors().spawn`, takes `Reach::Area`, `AtEdge::Stop` and `AwakeWhen::Always`.

| Throws | When |
|---|---|
| `std::invalid_argument`, `WorldKit::Scene: an actor is already named '…'` | A live actor has the entry's name. A removed actor's name is free again. |
| `std::logic_error`, `WorldKit::Scene: actor '…' is not an ActorKit::Actor` | The entry's `actor` holds another type. |
| what `ActorKit::Actors::spawn` throws | The actor is one `spawn` refuses. |

### `tick`

```cpp
template <class State, class Input> Step tick(const Input& input, State& state);
```

One tick of the scene ([What the scene does in a tick](#what-the-scene-does-in-a-tick)). `input` is your
game's input value and `state` your game state, the types your config's bindings, handlers and movers are
written for. It returns the camera's `Step`: `entered` is the place the camera came into, on the tick it
changes area.

```cpp
const WorldKit::Step step = state.scene.tick(readInput(in), state);
if (step.entered) {
    state.areaName.reset();   // your game's own reaction to a new area
}
```

`areaName` is a member of your state.

Watch for:

- **A handler may call the scene** it runs in: `state.scene.actor(asker)`, `follow`, `spawn` and every
  `actors()` call work from one.

Throws what `ActorKit::tick` throws.

### `layers` and `fade`

```cpp
template <class State> LayerKit::Layers layers(const State& state) const;
template <class State, class Dress> LayerKit::Layers layers(const State& state, Dress&& dress) const;
LayerKit::Regions fade() const;
```

The frame ([Drawing](#drawing)). `dress` is any function callable as
`LayerKit::Sprites(WorldKit::Place, std::string_view, LayerKit::Sprites)`.

```cpp
frame.layers = state.scene.layers(state);
frame.layers.push_back(LayerKit::SpriteLayer("hud", WorldKit::TopZ(world) + 1, {Hud(state)}));
frame.regions = state.scene.fade();
```

`Hud` is your component. Throws what `WorldLayers` and `ActorKit::Sprites` throw.

### `camera`, `actors`, `world` and `config`

```cpp
Camera& camera() noexcept;
const Camera& camera() const noexcept;
ActorKit::Actors& actors() noexcept;
const ActorKit::Actors& actors() const noexcept;
const World& world() const noexcept;
const ActorKit::Config& config() const noexcept;
```

The camera you handed over, the scene's actors, the world, and the config the scene hands ActorKit, its
`world` the scene's ([The level beneath](#the-level-beneath)).

```cpp
const retropp::Vec2i at = state.scene.camera().position();
state.scene.actors().set(hound, Hungry, true);
```

`Hungry` is one of the hound's own values.

## `SceneActor`

```cpp
class SceneActor {
public:
    ActorKit::Id id() const noexcept;

    void reach(Reach reach);
    Reach reach() const;
    void atEdge(AtEdge atEdge);
    AtEdge atEdge() const;
    void awakeWhen(AwakeWhen awakeWhen);
    AwakeWhen awakeWhen() const;

    void freeze();
    void wake();
    bool frozen() const;
    bool awake() const;
    bool hidden() const;
};
```

One actor of a scene, as [`Scene::actor`](#actor) returns it: its reach, what it does at the edge of its
reach, when it is awake, and the freeze you command. It is valid while its scene is.

A setting you choose is set and read by the same word. A state you command is done by a verb and reported
by a bare word:

| Kind | Calls |
|---|---|
| a setting | `reach(…)` / `reach()`, `atEdge(…)` / `atEdge()`, `awakeWhen(…)` / `awakeWhen()` |
| a state | `freeze()`, `wake()` / `frozen()`, `awake()`, `hidden()` |

Every member but `id` throws `std::out_of_range`, naming the id, once its actor is removed.

### `id`

```cpp
ActorKit::Id id() const noexcept;
```

The actor's id: the one `actors()` and every ActorKit call know it by.

```cpp
const ActorKit::Id marker = state.scene.actor("marker").id();
```

### `reach`

```cpp
void reach(Reach reach);
Reach reach() const;
```

How far the actor may roam ([areas.md](areas.md#reach)): set, and read back. It starts as the level its
entry is written at.

```cpp
state.scene.actor(asker).reach(WorldKit::Reach::World);   // the hound goes anywhere from now
```

| `reach` | `atEdge` `Stop` | `atEdge` `Despawn` or `Hide` |
|---|---|---|
| `Reach::Area` | held at every opening out of its area | meets its edge at every opening out of its area |
| `Reach::Layout` | goes through the openings within its layout, held at those into another | goes through the openings within its layout, and meets its edge at those into another |
| `Reach::World` | goes through every opening that leads somewhere | goes through every opening that leads somewhere |

- **It governs the next crossing.** An actor that stands outside a reach made smaller stays where it is.
- **The scene sets the actor's confinement** from its reach and `atEdge`, each time either changes.

### `atEdge`

```cpp
void atEdge(AtEdge atEdge);
AtEdge atEdge() const;
```

What the actor does at an opening that leads outside its reach ([The edge of a reach](#the-edge-of-a-reach)):
set, and read back. It starts as its entry's `atEdge`, `AtEdge::Stop` by default.

```cpp
state.scene.actor(bat).atEdge(WorldKit::AtEdge::Hide);
```

### `awakeWhen`

```cpp
void awakeWhen(AwakeWhen awakeWhen);
AwakeWhen awakeWhen() const;
```

When the actor is awake ([Awake or frozen](#awake-or-frozen)): set, and read back. It starts as its
entry's `awakeWhen`, `AwakeWhen::Always` by default. A new rule decides from the next tick.

```cpp
state.scene.actor(gardener).awakeWhen(WorldKit::AwakeWhen::InLayout);
```

### `freeze` and `wake`

```cpp
void freeze();
void wake();
```

`freeze` holds the actor still at once, and from then on, whatever its rule says. `wake` lifts that
freeze and hands the actor back to its rule.

```cpp
state.scene.actor(gardener).freeze();   // a cut scene holds the gardener
state.scene.actor(gardener).wake();     // and lets it go: awake if its rule holds
```

| Before `wake` | After `wake` |
|---|---|
| its rule holds for the areas on screen now, and it is in the world | awake at once |
| its rule fails for the areas on screen now | frozen until its rule holds |
| it is hidden | frozen until your game puts it back |

Called from a handler, `freeze` holds the actor from that tick's walking on.

### `frozen`, `awake` and `hidden`

```cpp
bool frozen() const;
bool awake() const;
bool hidden() const;
```

`frozen` is true while the actor is paused: frozen by hand, by its rule, or hidden. `awake` is its
opposite. `hidden` is true while `AtEdge::Hide` has put it out of the world.

```cpp
if (state.scene.actor(bat).frozen()) {
    // the bat is paused: off screen, held by freeze(), or hidden
}
```

## `SceneLayerKey`

```cpp
std::string SceneLayerKey(int layout, int area, std::string_view slot);   // "scene:<layout>:<area>:<slot>"
```

The key of the sprite layer `layers` draws for the actor slot `slot` of area `area` of layout `layout`.
Each area's slot keeps its key from the frame it comes on screen to the frame it leaves. A layer of your
own uses a key of its own.

## Exceptions

| Call | Throws | When |
|---|---|---|
| the constructors | `std::logic_error` | An entry's `actor` holds another type than `ActorKit::Actor`. |
| `follow(id)`, `actor(id)` | `std::out_of_range` | The id is any but a live actor's. |
| `follow(name)`, `actor(name)` | `std::out_of_range` | The name is any but a live actor's. |
| `spawn` | `std::invalid_argument` | A live actor has the entry's name. |
| `spawn` | `std::logic_error` | The entry's `actor` holds another type than `ActorKit::Actor`. |
| every `SceneActor` member but `id` | `std::out_of_range` | The actor has been removed. |
| `tick` | what `ActorKit::tick` throws | |
| `layers` | what `WorldLayers` and `ActorKit::Sprites` throw | |

## Lifetimes and copying

| Thing | Rule |
|---|---|
| the world | outlives the scene; the scene keeps its address |
| the bindings and handlers `config` names | outlive the scene |
| the camera, the config, the actors | the scene's own, copied in |
| a `SceneActor` | valid while its scene is; it names the scene it came from |
| a copy of a `Scene` | runs on by itself from where the original was: its own camera, actors and settings, the same world |
| a copy of your state | carries the whole scene |
