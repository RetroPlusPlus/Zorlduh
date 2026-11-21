# How-to

Recipes for common tasks. Each one is independent. The snippets use these names from your game:

| Name | What it is |
|---|---|
| `state` | your game state, with `actors`, an `ActorKit::Actors`, and `camera`, a `WorldKit::Camera` |
| `config` | your `ActorKit::Config` |
| `world` | your `WorldKit::World` |
| `input` | the tick's `Input`, your own struct |
| `hero` | the hero's `ActorKit::Id` |
| `Action` | your enum of input actions |
| names ending in `Look` | `ActorKit::Look`s |
| names ending in `Walk` | `TopDownMovement::WalkerConfig`s |

## Contents

The player and the camera

- [Make the player an actor](#make-the-player-an-actor)
- [Follow the player with the camera](#follow-the-player-with-the-camera)
- [Warp an actor](#warp-an-actor)
- [Send an actor to another area](#send-an-actor-to-another-area)

Switches and doors

- [Open and shut something with Interact](#open-and-shut-something-with-interact)
- [Make an actor solid while a value holds](#make-an-actor-solid-while-a-value-holds)

Values and scripts

- [Give each actor a value of its own](#give-each-actor-a-value-of-its-own)
- [Have one actor react to another](#have-one-actor-react-to-another)
- [Share one script among many actors](#share-one-script-among-many-actors)
- [Give a script something random](#give-a-script-something-random)

Movement

- [Walk toward the player](#walk-toward-the-player)

Sets and layers

- [Keep several sets and choose their layers](#keep-several-sets-and-choose-their-layers)

## Make the player an actor

Give the hero a mover driven by your input, an action row for Interact, and the triggers its body sets
off:

```cpp
struct Input {
    TopDownMovement::DirectionalInput move;
    bool interact = false;
};

const ActorKit::Actor heroActor{
    .slot    = "actors",
    .solid   = true,
    .mover   = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
    .actions = {{Action::Interact, &Input::interact}},
    .setsOff = {ActorKit::On::Touch, ActorKit::On::Near},
};
```

Spawn it once, keep its id, and record it as the player:

```cpp
const ActorKit::Id hero = state.actors.spawn(heroActor, {.layout = 0, .area = 0}, {72, 96});
state.actors.player(hero);
state.camera.place(world, state.actors.box(hero));
```

- Keep `hero` in your game state. Your camera code and your own logic ask the set about it:
  `state.actors.position(hero)`, `state.actors.facing(hero)`.
- Fill `Input` once per tick from the engine's input. Use `InputState::justPressed` for `interact`, so
  one press is one Interact.
- Name the same action in your config: `.interactAction = Action::Interact`.
- `setsOff` decides which touch and near scripts the hero starts in other actors. List the triggers
  the player should set off.

## Follow the player with the camera

After `ActorKit::tick`, follow the player's box with your `WorldKit::Camera`, and put the player where
the camera says:

```cpp
ActorKit::tick(state.actors, config, state.camera, input, state);

const WorldKit::Step step = state.camera.follow(world, state.actors.box(hero));
const retropp::Vec2i at = state.actors.position(hero);
state.actors.moveTo(hero, step.entered.value_or(state.actors.place(hero)),
                    {.x = at.x + step.move.x, .y = at.y + step.move.y});
if (step.entered) {
    state.actors.fresh(hero);
}
```

| Line | What it does |
|---|---|
| `camera.follow` | Follows the box. When the box goes out through an opening, it starts the scroll or the fade, or crosses the seam. |
| `step.move` | How far to carry the actor this tick: zero while it walks by itself, and the carry through an opening during a transition. |
| `step.entered` | The place the camera came into, on the tick it changes area. |
| `moveTo` | Applies both: the actor is carried through the opening and lands in the next area's pixels. |
| `fresh` | On the tick the player lands, shows it in place. Its position has jumped to the next area's pixels and its sprite to that area's layer, and the engine would ease it across the jump ([drawing.md](drawing.md#a-layer-for-each-area)). |

- Call `state.actors.player(hero)` once, when you spawn the hero. It lets the hero walk out through
  its area's openings.
- `step.entered` is also where your game reacts to a new area: change the music, save the place.
- If the player can be removed, test `state.actors.has(hero)` before these lines.
- To make another actor the player, call `state.actors.player(other)`, and put the camera on it as
  [Warp an actor](#warp-an-actor) does.

## Warp an actor

Put the actor in its new place, report it fresh so it appears there at once, and move the camera when
it is the player:

```cpp
void warp(State& state, const WorldKit::World& world, ActorKit::Id id, WorldKit::Place place, retropp::Vec2i at) {
    state.actors.moveTo(id, place, at);
    state.actors.fresh(id);
    if (id == state.actors.player()) {
        state.camera = WorldKit::Camera(place.layout, place.area);
        state.camera.place(world, state.actors.box(id));
    }
}
```

`State` is your game state's type. Call it after `ActorKit::tick`:

```cpp
ActorKit::tick(state.actors, config, state.camera, input, state);
if (state.warpRequested) {
    warp(state, world, hero, {.layout = 1, .area = 0}, {40, 32});
    state.warpRequested = false;
}
```

- `moveTo` puts the actor exactly at `at`, walls included. Choose a position in open ground.
- `fresh` gives the sprite a throwaway key for this tick, so the engine shows it in place
  ([drawing.md](drawing.md#fresh-keys)).
- `WorldKit::Camera(layout, area)` followed by `place` puts the camera in the new area, on the actor's
  box, with any transition dropped.
- On the next tick, `ActorKit::Enter` happens to every actor in the new area, the warped one included.

## Send an actor to another area

The player leaves its area through an opening. Every other actor's walk is held inside its
[confinement](actors.md#confinement): by default its area, so it stops at the area's edge. To take one
of those into another area, move it from your code when it reaches the edge:

```cpp
// A hen that reaches the yard's east gateway goes into the field, beside its west gateway.
const retropp::IntRect box = state.actors.box(hen);
if (state.actors.place(hen) == yard && box.x + box.width >= kYardWidth) {
    const retropp::Vec2i at = state.actors.position(hen);
    state.actors.moveTo(hen, field, {.x = 8, .y = at.y + kGatewayDrop});
    state.actors.fresh(hen);
}
```

| Name | What it is |
|---|---|
| `hen` | the actor's `ActorKit::Id` |
| `yard`, `field` | the two `WorldKit::Place`s |
| `kYardWidth` | the yard's width in pixels |
| `kGatewayDrop` | how many pixels lower the field's gateway is than the yard's |

- The new position is in the destination area's pixels. Work it out from where the two openings are on
  their maps.
- Confined to its area, a walker other than the player stops with its box against the area's edge, so
  the test is the box's far side reaching the area's width.
- From a script, do the same in a command's handler, which is handed the asking actor's id
  ([scripts.md](scripts.md#commands)).
- The actor keeps its values and its scripts, and its scripts keep running in the new area.

**Walk out through the opening.** Set the actor's confinement to `Layout` or `None`, once, and its walk
goes out through the opening as the player's does. It stands beyond the edge, in its area's pixels,
until your code moves it:

```cpp
state.actors.confinement(hen, ActorKit::Confinement::Layout);   // once, when the hen is spawned

// Each tick, after ActorKit::tick: the hen has walked wholly out through the yard's east gateway.
if (state.actors.place(hen) == yard && state.actors.box(hen).x >= kYardWidth) {
    const retropp::Vec2i at = state.actors.position(hen);
    state.actors.moveTo(hen, field, {.x = at.x - kYardWidth, .y = at.y + kGatewayDrop});
    state.actors.fresh(hen);
}
```

| `Confinement` | The hen at the east gateway |
|---|---|
| `Area` | It stops with its box against the edge. |
| `Layout` | It walks out when the gateway leads to an area of its own layout, and stops at one into another layout. |
| `None` | It walks out through any gateway that leads somewhere. |

- Beyond a scroll or a fade the ground is open within the opening's span, so the hen keeps walking
  until your code moves it. Beyond a seam it walks over the next area's own cells.
- `at.x - kYardWidth` keeps how far past the edge it has walked, and puts the hen that far inside the
  field's west gateway.

## Open and shut something with Interact

Write a script that flips a value on Interact, and have the actor's solidity and look follow the value:

```cpp
// Each Interact flips the value it is handed.
ScriptKit::Script toggle(ScriptKit::Value<bool> value) {
    return {.name = "toggle", .on = ActorKit::Interact, .body = {set(value, !value)}};
}

const ActorKit::Actor gate{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                           .solid = !Open,
                           .ownValues = {{Open, false}},
                           .look = {{Open, gateOpenLook}, {gateShutLook}},
                           .scripts = {toggle(Open)}};
```

`Open` is a `ScriptKit::Value<bool>`. Each gate spawned from `gate` owns its `Open`, so each opens and
shuts by itself.

| For | Change |
|---|---|
| one gate whose state your game also reads | Bind `Open` in your `ScriptKit::Bindings` and leave it out of `ownValues`. |
| a chest that opens once | Use `.body = {set(value, true)}`. |
| a door that opens from one side | Add `.from = ActorKit::Below` to the script. |
| a locked door | Add `.when = HasKey` to the script, where `HasKey` is a value of your game's. |

The hero needs an `.actions` row for Interact and your config needs `.interactAction`
([Make the player an actor](#make-the-player-an-actor)).

## Make an actor solid while a value holds

Set `solid` to an expression. ActorKit evaluates it every tick:

```cpp
// A barrier that is down while the switch is on.
const ActorKit::Actor barrier{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                              .solid = !SwitchOn,
                              .look = {{SwitchOn, barrierDownLook}, {barrierUpLook}}};
```

`SwitchOn` is a `ScriptKit::Value<bool>` of your game's.

| `solid` | The actor is solid |
|---|---|
| `true` | always |
| `!SwitchOn` | while the switch is off |
| `Raised` | while its own `Raised` holds |
| `Health > 0` | while it has health left |
| `Locked && !HasKey` | while it is locked and the key is missing |

- A script that sets the value on tick N changes the solidity on tick N + 1.
- Use the same value in the actor's `look`, so what the player sees matches what stops them.
- A walker standing in the box when it turns solid is held there until it opens
  ([movers.md](movers.md#what-a-mover-collides-with)).

## Give each actor a value of its own

List the value in the actor's `ownValues`, with its starting worth:

```cpp
const ScriptKit::Value<int> Health;

const ActorKit::Actor slime{.slot = "actors", .box = {.x = 0, .y = 8, .width = 16, .height = 8},
                            .solid = true,
                            .ownValues = {{Health, 3}},
                            .look = {{Health <= 1, slimeHurtLook}, {slimeLook}},
                            .scripts = {&hurt}};
```

`hurt` is a `ScriptKit::Script` that sets `Health`. Every slime spawned from `slime` starts with 3, and
a hit on one slime changes that slime's `Health`.

To read and write an actor's own value from your code, keep its id:

```cpp
std::vector<ActorKit::Id> slimes;
slimes.push_back(state.actors.spawn(slime, {.layout = 0, .area = 0}, {96, 40}));

for (const ActorKit::Id id : slimes) {
    if (state.actors.has(id) && state.actors.get(id, Health) == 1) {
        state.actors.set(id, Health, 3);   // a kind game: the slime is healed
    }
}
```

- `get` and `set` take a value from the actor's `ownValues`. They throw `std::out_of_range` for any
  other, so ask only the actors that own it.
- `has(id)` is `false` once an actor is removed.
- One `Value` constant can be listed by many kinds of actor. Each actor has its own.

## Have one actor react to another

Actors affect each other through a value of your game's. One actor's script sets it, and the other
actor reads it:

```cpp
const ScriptKit::Value<bool> PlateDown;   // the game's: bound to State::plateDown
const ScriptKit::Value<bool> Alert;       // each guard's own

// The plate holds PlateDown true while something stands on it.
const ScriptKit::Script weigh{.name = "weigh", .on = ActorKit::Touch,
                              .body = {set(PlateDown, true), waitWhile(ActorKit::Touched), set(PlateDown, false)}};
const ActorKit::Actor plate{.slot = "underfoot", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                            .look = {{PlateDown, plateDownLook}, {plateUpLook}},
                            .scripts = {&weigh}};

// The portcullis reads it: raised, and passable, while the plate is down.
const ActorKit::Actor portcullis{.slot = "actors", .box = {.x = 0, .y = 0, .width = 16, .height = 16},
                                 .solid = !PlateDown,
                                 .look = {{PlateDown, portcullisUpLook}, {portcullisDownLook}}};

// A guard's script starts on it.
const ScriptKit::Script notice{.name = "notice", .on = ActorKit::Becomes(PlateDown), .body = {set(Alert, true)}};
const ActorKit::Actor guard{.slot = "actors", .box = {.x = 0, .y = 8, .width = 16, .height = 8},
                            .solid = true,
                            .ownValues = {{Alert, false}},
                            .look = {{Alert, guardAlertLook}, {guardLook}},
                            .scripts = {&notice}};
```

An actor can read the value in five places:

| Place | Example | Effect |
|---|---|---|
| `solid` | `.solid = !PlateDown` | It blocks or lets through. |
| `look` | `{PlateDown, portcullisUpLook}` | It changes what it shows. |
| a script's `on` | `ActorKit::Becomes(PlateDown)` | A script starts when the value turns true. |
| a script's `when` | `.when = PlateDown` | A trigger starts the script only while the value holds. |
| a script's body | `waitWhile(PlateDown)` | A running script waits on it or branches on it. |

- The value is your game's, bound in your `ScriptKit::Bindings`, because two actors share it. An own
  value is private to its actor.
- For many plate-and-portcullis pairs, use one value per pair: write `plate` and `portcullis` as
  functions that take the value.
- For anything a value leaves out, such as a sound or a message, use a command
  ([scripts.md](scripts.md#commands)).

## Share one script among many actors

Write the script as a function of the values it acts on:

```cpp
// Touched, it makes `pressed` true, holds while `held` does, and makes it false again.
ScriptKit::Script pressWhile(ScriptKit::Value<bool> pressed, ScriptKit::Value<bool> held) {
    return {.name = "press", .on = ActorKit::Touch,
            .body = {set(pressed, true), waitWhile(held), set(pressed, false)}};
}
```

Each actor hands it the values it wants:

```cpp
.scripts = {pressWhile(Pressed, ActorKit::Touched)},   // a plate, with its own Pressed
```

```cpp
.scripts = {pressWhile(Splashing, ActorKit::Touched)},   // a puddle, with its own Splashing
```

- A script made this way is started by its trigger. Scripts that other scripts name in `call`, `stop`,
  `restart` or `interrupts` are constants listed by address.
- Build each actor value once and spawn from it. Every actor spawned from one value shares the one
  script.

[scripts.md](scripts.md#share-a-script-with-a-function) covers this in full.

## Give a script something random

A script reads values and asks for commands, and both can carry a random number from your game.

**One roll a tick, for every actor.** Bind a value and roll it in your tick, before `ActorKit::tick`:

```cpp
const ScriptKit::Value<int> Roll;   // 0 to 99, rolled anew every tick

const ScriptKit::Bindings<State> bindings{
    {Roll, &State::roll},
};
```

```cpp
state.roll = static_cast<int>(state.random() % 100);
ActorKit::tick(state.actors, config, state.camera, input, state);
```

```cpp
// A hen that turns left on about one tick in fifty.
const ScriptKit::Script fidget{
    .name = "fidget",
    .on = ActorKit::Enter,
    .body = {loop({when(Roll < 2, {set(Heading, retropp::Dir::Left)}), wait(1)})},
};
```

`State` has `int roll` and `std::minstd_rand random`. Every actor reads the same roll on a given tick.

**A roll for each actor.** Ask for a command, and roll in its handler, which knows the asking actor:

```cpp
const ScriptKit::Command Turn{"turn"};

const ActorKit::Handlers<State> handlers{
    {Turn, [](State& state, ActorKit::Id asker, const ScriptKit::Arguments&) {
         constexpr std::array<retropp::Dir, 4> ways{retropp::Dir::Up, retropp::Dir::Right, retropp::Dir::Down,
                                                    retropp::Dir::Left};
         state.actors.set(asker, Heading, ways[state.random() % ways.size()]);
     }},
};

// Each hen turns a way of its own every second.
const ScriptKit::Script roam{.name = "roam", .on = ActorKit::Enter, .body = {loop({Turn(), wait(1s)})}};
```

`Heading` is a `ScriptKit::Value<retropp::Dir>` each hen owns and its mover is driven by.

- Keep the generator in your game state. A copied state then replays the same rolls.
- The handler runs at the start of the tick after the script asked.

## Walk toward the player

Drive the mover with a function that compares the actor's position with the player's:

```cpp
// A heading toward the player, while the slime is angry.
TopDownMovement::Directions chase(const ScriptKit::Values& values) {
    if (!values.get(Angry)) {
        return {};
    }
    const int dx = values.get(ActorKit::PlayerX) - values.get(ActorKit::X);
    const int dy = values.get(ActorKit::PlayerY) - values.get(ActorKit::Y);
    return {.up = dy < 0, .down = dy > 0, .left = dx < 0, .right = dx > 0};
}

const ActorKit::Actor slime{.slot = "actors", .solid = true,
                            .ownValues = {{Angry, false}},
                            .scripts = {&provoke},
                            .mover = ActorKit::movedBy<TopDownMovement::Walker>(slimeWalk, chase)};
```

`Angry` is a `ScriptKit::Value<bool>` the slime owns and `provoke` a `ScriptKit::Script` that sets it.

- The slime walks diagonally until it is level with the hero on one axis, then straight. It slides
  along walls and stops against the hero's box when the hero is solid.
- While the hero is in another area, `PlayerX` and `PlayerY` equal the slime's own position and
  the slime stands.
- To flee, swap the comparisons.
- The function is handed the actor's own values and ActorKit's six
  ([movers.md](movers.md#a-function-of-the-actors-values)).

## Keep several sets and choose their layers

Keep one `Actors` value for each group, tick each, and send each group's sprites to the layer you
choose:

```cpp
struct State {
    ActorKit::Actors actors;   // the hero and everything it touches, blocks and acts on
    ActorKit::Actors lights;   // torches and lamps: they share only values with the rest
    WorldKit::Camera camera{0, 0};
    bool night = false;
};
```

```cpp
// Every tick.
ActorKit::tick(state.actors, config, state.camera, input, state);
ActorKit::tick(state.lights, config, state.camera, input, state);

// Every frame: in each area on screen, both sets' sprites at the "actors" slot, sorted together.
frame.layers = WorldKit::WorldLayers(state.camera, world);
for (const WorldKit::Shown& shown : state.camera.shown(world)) {
    const WorldKit::Place place{.layout = shown.layout, .area = shown.area};
    frame.layers.push_back(
        AreaSlot(world, shown, "underfoot", ActorKit::Sprites(state.actors, config, place, "underfoot", state)));
    LayerKit::Sprites standing = ActorKit::Sprites(state.lights, config, place, "actors", state);
    const LayerKit::Sprites actors = ActorKit::Sprites(state.actors, config, place, "actors", state);
    standing.insert(standing.end(), actors.begin(), actors.end());
    frame.layers.push_back(AreaSlot(world, shown, "actors", std::move(standing)));
}
```

`frame` is your `retropp::FrameDrawState`, and `AreaSlot` your component that puts one area's sprites
at one slot on a layer of their own ([drawing.md](drawing.md#a-layer-for-each-area)). A torch in
`lights` lights up when night falls, through a value both sets are ticked with:

```cpp
const ScriptKit::Value<bool> Night;   // the game's: bound to State::night
const ScriptKit::Value<bool> Lit;     // each torch's own

const ScriptKit::Script kindle{.name = "kindle", .on = ActorKit::Becomes(Night), .body = {set(Lit, true)}};
const ActorKit::Actor torch{.slot = "actors", .box = {.x = 0, .y = 8, .width = 8, .height = 8},
                            .ownValues = {{Lit, false}},
                            .look = {{Lit, torchLitLook}, {torchLook}},
                            .scripts = {&kindle}};
```

| Between two sets | How |
|---|---|
| An event in one starts a script in the other | A script in the first sets a value of your game's, and a script in the second starts on `ActorKit::Becomes` of it. Tick both with the same bindings. |
| Sprites on one layer | Join both `Sprites` results into one list for the area's layer. Each sprite's `z` sorts it among all of them. |
| Sprites on separate layers | Give the sets' actors different `slot` names. |

- Touch, near, Interact and solidity work inside a set. Keep actors that touch, block or act on each
  other in one set, as the hero and everything it meets are here.
- Each set counts its ids from 1. Keep each id with the set it came from.
- A set you leave out of a tick stays as it is. Skip `actors` while a menu is open and the torches keep
  running. To hold one actor still while the rest of its set runs, freeze it with
  `state.actors.freeze(id)`: it stays as it is, still drawn and still solid, until
  `state.actors.wake(id)` ([actors.md](actors.md#freeze-wake-and-frozen)).

[concepts.md](concepts.md#several-sets) has the rules.
