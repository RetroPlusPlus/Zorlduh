# Triggers

What starts an actor's scripts: ActorKit's five ready-made triggers, the exact rule of each thing
ActorKit detects, and triggers of your own.

```cpp
#include "ActorKit/triggers.h"   // On, Interact, Touch, Near, Enter, Becomes, TriggerMap,
                                 // actedOn, touched, near, entered, becomes
```

`actor.h` includes this header.

## Contents

- [Overview](#overview)
- [ActorKit's own triggers](#actorkits-own-triggers)
- [How a trigger starts a script](#how-a-trigger-starts-a-script)
- [The detections](#the-detections)
- [Triggers of your own](#triggers-of-your-own)
- [`On`](#on)
- [`TriggerMap`](#triggermap)
- [The detection functions](#the-detection-functions)
- [`TriggerKind`, `ActionName` and `Detection`](#triggerkind-actionname-and-detection)

## Overview

```cpp
// A torch that lights when the hero comes within 24 pixels.
const ScriptKit::Script light{.name = "light", .on = ActorKit::Near(24), .body = {set(Lit, true)}};

// A chest the hero opens with Interact, standing below it.
const ScriptKit::Script open{.name = "open", .on = ActorKit::Interact, .from = ActorKit::Below,
                             .body = {set(Opened, true)}};

const ActorKit::Actor torch{.slot = "actors", .box = {.x = 0, .y = 0, .width = 8, .height = 16},
                            .ownValues = {{Lit, false}},
                            .look = {{Lit, torchLit}, {torchDark}},
                            .scripts = {&light}};
const ActorKit::Actor chest{.slot = "actors", .box = {.x = 0, .y = 8, .width = 16, .height = 8},
                            .solid = true,
                            .ownValues = {{Opened, false}},
                            .look = {{Opened, chestOpen}, {chestShut}},
                            .scripts = {&open}};

// The hero sets off Near in the actors it comes close to, and performs Interact on the actor it faces.
const ActorKit::Actor hero{.slot = "actors", .solid = true,
                           .mover = ActorKit::movedBy<TopDownMovement::InputWalker>(heroWalk, &Input::move),
                           .actions = {{Action::Interact, &Input::interact}},
                           .setsOff = {ActorKit::On::Near}};

const ActorKit::Config config{.world = &world, .interactAction = Action::Interact};
```

Names from your game:

| Name | What it is |
|---|---|
| `Lit`, `Opened` | `ScriptKit::Value<bool>` constants |
| `torchLit`, `torchDark`, `chestOpen`, `chestShut` | `ActorKit::Look`s |
| `heroWalk` | a `TopDownMovement::WalkerConfig` |
| `Input` | your input struct, with `move`, a `TopDownMovement::DirectionalInput`, and `interact`, a `bool` |
| `Action` | your enum of input actions |
| `world` | your `WorldKit::World` |

The torch lights when the hero's box comes within 24 pixels of the torch's. The chest opens when the
hero stands below it, faces up and presses Interact. From its other three sides it stays shut.

## ActorKit's own triggers

Five triggers work as they are. Write one in a script's `on`:

| In `on` | Happens to the actor when | Needs |
|---|---|---|
| `ActorKit::Interact` | an actor facing it performs your Interact action | `.interactAction` in your config, and an `.actions` row on the acting actor |
| `ActorKit::Touch` | an actor that sets off `Touch` begins to overlap its box | `ActorKit::On::Touch` in the other actor's `.setsOff` |
| `ActorKit::Near(distance)` | an actor that sets off `Near` comes within `distance` pixels | `ActorKit::On::Near` in the other actor's `.setsOff` |
| `ActorKit::Enter` | the camera comes into its area, or it is spawned into the camera's area | ready as it is |
| `ActorKit::Becomes(value)` | `value` goes from false to true | ready as it is |

A trigger happens to one actor and starts that actor's scripts. The torch's `light` runs for the torch
the hero came near, and the other torches spawned from the same value stay dark.

`setsOff` says which actors' bodies count. The hero lists `ActorKit::On::Near`, so the hero lights
torches. A hen with an empty `setsOff` walks past them in the dark.

## How a trigger starts a script

When a trigger happens to an actor, ActorKit looks at each of that actor's scripts whose `on` is the
trigger:

| The script | Result |
|---|---|
| is off, its `when` holds and its `from` fits | It starts. Its first entries run on this tick, and what they ask for is written on the next. |
| is off, and its `when` is false at that moment | It stays off. |
| is off, and its `from` names another side | It stays off. |
| is running or waiting | It carries on where it is. |

- **A running script misses its trigger.** A script in the middle of a `wait(1s)` when its trigger
  happens again carries on with the wait. For a script that answers every time, keep its body free of
  waits.
- **`when` is tested at the moment the trigger happens.** A trigger that finds `when` false is gone.
  When the condition later turns true, the script starts at the trigger's next occurrence.
- **`from`** applies to [acted-on](#acted-on) triggers. Leave it unset for the others.

## The detections

ActorKit detects five things. Each of its own triggers is bound to one, and so is each trigger of
yours.

| Detection | ActorKit's trigger | For a trigger of yours |
|---|---|---|
| [Acted on](#acted-on) | `Interact` | `actedOn(action)` |
| [Touched](#touched) | `Touch` | `touched()` |
| [Near](#near) | `Near(distance)` | `near()` |
| [Entered](#entered) | `Enter` | `entered()` |
| [Becomes](#becomes) | `Becomes(value)` | `becomes()` |

Acted on, touched and near work within one `Actors` value and one area: an actor detects the actors of
its own set that are in its own area ([concepts.md](concepts.md#several-sets)).

### Acted on

An actor performs an action on a tick when one of its [action rows](actors.md#actionrow-and-actions)
is driven, or when your code called [`Actors::act`](actors.md#act) for it.

The actor acted on is the one whose box holds a point one pixel beyond the acting actor's box, at the
middle of the side it faces:

| The acting actor faces | The point | The side a script's `from` sees |
|---|---|---|
| up | one pixel above the middle of its box's top side | `ActorKit::Below` |
| down | one pixel below the middle of its bottom side | `ActorKit::Above` |
| left | one pixel left of the middle of its left side | `ActorKit::Right` |
| right | one pixel right of the middle of its right side | `ActorKit::Left` |

The side is where the acting actor stands. An actor facing up stands below the actor it acts on.

A bell with its box at (64, 64), 16 wide and 8 tall, and a hero with an 8×8 box:

| The hero's box is at | Facing | Result |
|---|---|---|
| (68, 72), under the bell | up | The point (72, 71) is in the bell's box. The bell is acted on from `Below`. |
| (68, 72), under the bell | down | The point (72, 80) is in open ground. The action goes unanswered. |
| (84, 72), under a second bell at (80, 64) | up | The point (88, 71) is in the second bell's box. The second bell is acted on. |

- **One actor per action.** If two boxes hold the point, the actor spawned first is acted on.
- **The acting actor stands next to its target.** A walker that walks into a solid actor stops with
  the two boxes meeting, which puts the point inside the target.
- **The target needs a box.**
- **An actor with an empty `mover` faces down.**
- **Both kinds of script start.** When the action is the one in `Config::interactAction`, the target's
  scripts on `ActorKit::Interact` start. Its scripts on a trigger of yours bound to `actedOn` of the
  same action start too.

### Touched

Happens to an actor when another actor, one whose `setsOff` lists the trigger, begins to overlap its
box in the same area.

| Situation | Result |
|---|---|
| A setter walks onto the actor. | The trigger happens once, on the first tick of the overlap. |
| The setter stays where it is. | The trigger stays quiet. |
| The setter leaves and comes back. | The trigger happens again. |
| A second setter arrives while the first stays. | The trigger happens again. |
| An actor with the trigger left out of its `setsOff` overlaps. | The trigger stays quiet. |

`ActorKit::Touched` is the lasting form: a value that is true for an actor on every tick a setter of
any touch trigger overlaps it. A script that acts for as long as the touch lasts waits on it:

```cpp
const ScriptKit::Script splash{.name = "splash", .on = ActorKit::Touch,
                               .body = {set(Splashing, true), waitWhile(ActorKit::Touched), set(Splashing, false)}};
```

`Splashing` is a `ScriptKit::Value<bool>` the puddle owns.

- **Both boxes need a size.**
- **A walker stops against a solid actor before the two overlap.** Make the things a walker touches
  passable: a plate, a puddle, a pickup. For a solid thing use `Interact` or `Near`.

### Near

Happens to an actor when a setter comes within the script's distance. The distance is measured between
the two boxes, edge to edge, and is 0 when they meet or overlap.

With a script on `ActorKit::Near(10)`:

| Situation | Result |
|---|---|
| A setter stops with an 11-pixel gap between the boxes. | The trigger stays quiet. |
| The setter moves one pixel closer: a 10-pixel gap. | The trigger happens. |
| The setter stays within 10 pixels. | The trigger stays quiet. |
| A second setter comes within 10 pixels while the first is still there. | The trigger stays quiet. |
| Every setter leaves, and one comes back. | The trigger happens again. |
| The actor is spawned with a setter already within 10 pixels. | The trigger happens on the actor's first tick. |

- **The distance is per script.** An actor with a script on `Near(24)` and another on `Near(8)` starts
  the first as the hero approaches and the second when the hero is close.
- **The gap is a straight line** between the nearest points of the two boxes. A setter 6 pixels across
  and 8 pixels down from a box's corner is 10 pixels away.
- **An empty box counts as the point at the actor's position.**

### Entered

Happens to an actor in two cases:

| Case | Which actors |
|---|---|
| The camera's area changes to the actor's area: by a scroll, a fade, a seam, or your code assigning the camera. | every actor in that area |
| The actor's first tick finds it in the camera's area. | that actor |

The first tick of your game is the second case for every actor in the camera's starting area.

| Situation | Result |
|---|---|
| The hero walks through a scrolling exit into the field. | The trigger happens to the field's actors on the tick the scroll ends. |
| The camera leaves the field and comes back later. | The trigger happens again. Scripts that are still running carry on. |
| A script spawns an egg in the camera's area. | The trigger happens to the egg on its first tick. |
| Your code moves an actor into the camera's area with `moveTo`. | The trigger stays quiet: it follows the camera. |

`tick` reads the camera's area from the `WorldKit::Camera` you hand it. During a scroll that is the
area being left until the last tick, and during a fade it changes at the middle (WorldKit's
`docs/camera.md`).

Use it to wake an area's inhabitants when the player arrives, and to start what a new actor does.

### Becomes

Happens to an actor when the script's value is true on this tick and was false on the tick before.
The value is one of the actor's own, one of your game's, or one of ActorKit's six.

| Situation | Result |
|---|---|
| The value goes from false to true. | The trigger happens. |
| The value stays true. | The trigger stays quiet. |
| The value goes false, then true again on a later tick. | The trigger happens again. |
| The value is already true on the actor's first tick. | The trigger stays quiet. The first tick records the value. |

- **The value is read once per tick.** A value set true and then false by consecutive entries of one
  script body is false both times it is read.
- **It is how one actor reacts to another.** A bell's script sets `BellRung`, and a dog's script starts
  on `ActorKit::Becomes(BellRung)` ([how-to.md](how-to.md#have-one-actor-react-to-another)).
- **It is per script,** like the distance of `Near`.

## Triggers of your own

Declare a trigger of your own when you need a second action, a second group of setters, or your own
name for a detection. It takes three lines: an enumerator, the trigger a script puts in `on`, and a row
that binds it.

```cpp
enum class On : std::uint8_t { Poke, Brush };

constexpr ScriptKit::Trigger Poke{On::Poke};
constexpr ScriptKit::Trigger Brush{On::Brush};

const ActorKit::TriggerMap triggers{
    {On::Poke,  ActorKit::actedOn(Action::Poke)},   // an actor facing this one performs Poke
    {On::Brush, ActorKit::touched()},               // a second touch trigger, with its own setters
};

const ActorKit::Config config{.world = &world, .interactAction = Action::Interact, .triggers = triggers};
```

`Action::Poke` is one of your input actions. With this map:

| To | Write |
|---|---|
| start a script when the actor is poked | `.on = Poke` in the script, and `.actions = {{Action::Poke, &Input::poke}}` on the actor that pokes |
| start a script when an enemy brushes the actor | `.on = Brush` in the script, and `.setsOff = {On::Brush}` on the enemies |
| keep the hero pressing plates | `.setsOff = {ActorKit::On::Touch}` on the hero, as before |

`Touch` and `Brush` are both touch triggers and have separate setters. A plate with a script on
`ActorKit::Touch` answers the hero, and a tripwire with a script on `Brush` answers the enemies.

A near trigger carries its distance and a becomes trigger its value, as ActorKit's own do:

```cpp
enum class On : std::uint8_t { Hears, Wakes };

constexpr ScriptKit::Trigger Hears(int distance) { return {On::Hears, distance}; }
ScriptKit::Trigger Wakes(ScriptKit::Value<bool> value) { return {On::Wakes, value}; }

const ActorKit::TriggerMap triggers{
    {On::Hears, ActorKit::near()},
    {On::Wakes, ActorKit::becomes()},
};
```

- **Your enum and ActorKit's stay apart.** Number your enumerators from 0 as usual. ActorKit's own are
  numbered from `0x40000000`.
- **Every map binds ActorKit's own `Touch`, `Near`, `Enter` and `Becomes`.** Your rows are added to
  them.
- **A trigger needs a row.** A script whose `on` is a trigger outside the map stays off.
- **A trigger bound twice** makes the `TriggerMap` constructor throw `std::invalid_argument`.

## `On`

```cpp
enum class On : std::uint32_t { Interact = 0x40000000, Touch, Near, Enter, Becomes };

inline constexpr ScriptKit::Trigger Interact{On::Interact};
inline constexpr ScriptKit::Trigger Touch{On::Touch};
inline constexpr ScriptKit::Trigger Enter{On::Enter};
constexpr ScriptKit::Trigger Near(int distance) noexcept;
ScriptKit::Trigger Becomes(ScriptKit::Value<bool> value) noexcept;
```

`On` holds the names of ActorKit's own triggers. `Interact`, `Touch`, `Enter`, `Near(distance)` and
`Becomes(value)` are the triggers themselves, for a script's `on`.

You write an `On` enumerator in one place: `setsOff`.

```cpp
.setsOff = {ActorKit::On::Touch, ActorKit::On::Near},
```

Touch and near are the two detections a body sets off, so `On::Touch` and `On::Near` are the two
enumerators `setsOff` uses.

## `TriggerMap`

```cpp
class TriggerMap {
public:
    struct Row {
        TriggerKind trigger;
        Detection detection;
    };

    TriggerMap();
    TriggerMap(std::initializer_list<Row> rows);

    std::optional<Detection> detectionOf(std::uint32_t kind) const noexcept;
    const std::vector<Row>& rows() const noexcept;
};
```

Your triggers, each bound to a detection. Build one at startup and put it in `Config::triggers`.

```cpp
const ActorKit::TriggerMap triggers{
    {On::Poke,  ActorKit::actedOn(Action::Poke)},
    {On::Brush, ActorKit::touched()},
};
```

| Member | What it does |
|---|---|
| `TriggerMap()` | A map of ActorKit's own triggers. It is the default of `Config::triggers`. |
| `TriggerMap(rows)` | Your rows, then ActorKit's own `Touch`, `Near`, `Enter` and `Becomes`. |
| `detectionOf(kind)` | The detection bound to the trigger whose `ScriptKit::Trigger::kind` is `kind`, or an empty optional. |
| `rows()` | Every row, yours first. |

- Two triggers can share a detection. Each is its own trigger, with its own setters.
- `Interact` is bound through `Config::interactAction`, apart from the map.
- Throws `std::invalid_argument`, naming the trigger's number, when a trigger is bound twice. A row
  that binds one of ActorKit's own, such as `{ActorKit::On::Touch, ActorKit::near()}`, binds it twice.

## The detection functions

```cpp
constexpr Detection actedOn(ActionName action) noexcept;
constexpr Detection touched() noexcept;
constexpr Detection near() noexcept;
constexpr Detection entered() noexcept;
constexpr Detection becomes() noexcept;
```

Each returns the `Detection` for a `TriggerMap` row.

| Function | The trigger bound to it is written | Rule |
|---|---|---|
| `actedOn(action)` | `{On::Poke}` | [Acted on](#acted-on), for that action |
| `touched()` | `{On::Brush}` | [Touched](#touched) |
| `near()` | `{On::Hears, distance}` | [Near](#near) |
| `entered()` | `{On::Arrives}` | [Entered](#entered) |
| `becomes()` | `{On::Wakes, value}` | [Becomes](#becomes) |

## `TriggerKind`, `ActionName` and `Detection`

```cpp
struct TriggerKind {
    std::uint32_t kind = 0;

    template <class E> requires std::is_enum_v<E>
    constexpr TriggerKind(E trigger) noexcept;
};
using TriggerKinds = std::vector<TriggerKind>;

struct ActionName {
    retropp::ActionId id = 0;

    template <retropp::ActionLike A>
    constexpr ActionName(A action) noexcept;
};

enum class DetectionKind : std::uint8_t { ActedOn, Touched, Near, Entered, Becomes };

struct Detection {
    DetectionKind kind = DetectionKind::Touched;
    retropp::ActionId action = 0;
};
```

The types behind the braces you write. Your code names them rarely.

| Type | What it is | Where you meet it |
|---|---|---|
| `TriggerKind` | One trigger enumerator, yours or ActorKit's. It converts from any enumerator. | a `TriggerMap` row's first element |
| `TriggerKinds` | A list of them. | the type of `Actor::setsOff` |
| `ActionName` | One of your input actions. It converts from your enumerator. | `actedOn(action)`, `Config::interactAction` |
| `Detection` | One thing ActorKit detects, with the action for acted-on. | what the detection functions return |
| `DetectionKind` | The five detections. | `Detection::kind` |

`TriggerKind::kind` is the enumerator's value plus one, the same number `ScriptKit::Trigger::kind`
holds for that enumerator.
