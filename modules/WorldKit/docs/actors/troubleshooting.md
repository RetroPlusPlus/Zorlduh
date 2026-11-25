# Troubleshooting

Symptoms, their causes and the fix for each. The last two sections list the module's compile errors and
exceptions.

## Contents

- [A script stays off](#a-script-stays-off)
- [Interact goes unanswered](#interact-goes-unanswered)
- [Touch goes unanswered](#touch-goes-unanswered)
- [Near goes unanswered](#near-goes-unanswered)
- [A script answers its trigger once](#a-script-answers-its-trigger-once)
- [A change shows up a tick later](#a-change-shows-up-a-tick-later)
- [A count comes up short](#a-count-comes-up-short)
- [An actor is missing from the screen](#an-actor-is-missing-from-the-screen)
- [An actor is drawn in the wrong place](#an-actor-is-drawn-in-the-wrong-place)
- [An actor slides across the screen when moved](#an-actor-slides-across-the-screen-when-moved)
- [An actor stands still](#an-actor-stands-still)
- [An actor walks through things](#an-actor-walks-through-things)
- [An actor is held in place](#an-actor-is-held-in-place)
- [An actor stops at a doorway](#an-actor-stops-at-a-doorway)
- [An actor stands beyond its area's edge](#an-actor-stands-beyond-its-areas-edge)
- [The hero walks off the screen](#the-hero-walks-off-the-screen)
- [Actors in two sets ignore each other](#actors-in-two-sets-ignore-each-other)
- [Compile errors](#compile-errors)
- [Exceptions](#exceptions)

## A script stays off

| Cause | Fix |
|---|---|
| Its trigger has yet to happen to this actor. | See the next three sections for Interact, touch and near. |
| The script's `when` was false at the moment the trigger happened. | `when` is tested once, when the trigger happens. Make the condition true before the trigger, or start the script on `ActorKit::Becomes` of the condition's value. |
| The script has a `from`, and its trigger is touch, near, enter or becomes. | Leave `from` unset. A side applies to `Interact` and to triggers bound to `actedOn`. |
| Its `on` is a trigger of your own that the trigger map leaves out, or the map was left out of the config. | Add the row, and set `.triggers` in your `ActorKit::Config` ([triggers.md](triggers.md#triggers-of-your-own)). |
| Its `on` is `ActorKit::Enter` and the actor is in another area than the camera. | It starts when the camera comes into the actor's area. An actor your code moves into the camera's area with `moveTo` keeps its `Enter` scripts off. |
| Its `on` is `ActorKit::Becomes(value)` and the value was already true on the actor's first tick. | The trigger happens when the value goes from false to true. Start it false. |
| The script is still running from an earlier trigger. | See [A script answers its trigger once](#a-script-answers-its-trigger-once). |
| The actor is frozen: `state.actors.frozen(id)` is true. | A frozen actor's triggers hold as they are. Wake it with `state.actors.wake(id)` ([actors.md](actors.md#freeze-wake-and-frozen)). |

To see whether a script is running, ask the actor's processor:
`state.actors.processor(id).running(script)`.

## Interact goes unanswered

| Cause | Fix |
|---|---|
| `Config::interactAction` is unset. | Set `.interactAction = Action::Interact`, with your own enumerator. |
| The acting actor has an empty `actions`. | Add `.actions = {{Action::Interact, &Input::interact}}`. |
| The member that drives the row stays false. | Fill it each tick from the engine's input, and bind the action in the `ActionMap` you give the platform. |
| The acting actor faces another way. | Interact acts on the actor one pixel beyond the middle of the side the actor faces ([triggers.md](triggers.md#acted-on)). |
| The acting actor stands off to one side, so the point misses the target's box. | Make the target's box as wide as the place the player stands, or make the target solid so the hero stops squarely against it. |
| A gap separates the two boxes. | Make the target solid, so a walker stops with the boxes meeting. |
| The target's `box` is empty. | Give it a box. |
| The script's `from` names another side. | `ActorKit::Below` means the acting actor stands below the target and faces up. |
| The two actors are in different `Actors` values. | Put them in one set ([concepts.md](concepts.md#several-sets)). |
| The target is frozen. | An action aimed at a frozen actor goes to the next awake actor whose box holds the point. Wake the target with `state.actors.wake(id)`. |
| The acting actor is frozen. | Its actions wait for its first tick awake. Wake it with `state.actors.wake(id)`. |

## Touch goes unanswered

| Cause | Fix |
|---|---|
| The moving actor's `setsOff` lacks the trigger. | Add `ActorKit::On::Touch`, or your own touch trigger's enumerator, to its `.setsOff`. |
| The touched actor is solid. A walker stops against a solid actor before the boxes overlap. | Make it passable, or use `Interact` or `Near` for a solid actor. |
| One of the two boxes is empty. | Give both a box. An actor with a mover has its mover's. |
| The two actors are in different areas or different sets. | Touch works within one area and one `Actors` value. |
| The two already overlapped. | The trigger happens on the first tick of an overlap. For the whole time it lasts, read `ActorKit::Touched`. |

## Near goes unanswered

| Cause | Fix |
|---|---|
| The moving actor's `setsOff` lacks the trigger. | Add `ActorKit::On::Near`, or your own near trigger's enumerator. |
| The distance is measured between the two boxes, edge to edge. A box at the actor's feet is further from things above it than its art is. | Raise the distance, or enlarge the box. |
| A setter was already within the distance. | The trigger happens when the first setter comes within the distance, and again after every setter has left. |
| The two actors are in different areas or different sets. | Near works within one area and one `Actors` value. |

## A script answers its trigger once

A trigger starts a script that is off. A script that is still running carries on where it is.

| Cause | Fix |
|---|---|
| The body ends in a loop or a `waitWhile` that lasts. | Let the body end, so the script is off for the next trigger. |
| The body has a long `wait`, and the trigger happens again during it. | Shorten the body, or move the long part into a second script started by a value. |
| The script reads a value after a `set`, which takes a second tick, and the trigger happened on that tick. | This is one tick. For presses read with `justPressed` it is out of reach of a player. |

## A change shows up a tick later

A script's `set`, command, `ActorKit::spawn` and `ActorKit::remove` are written at the start of the
next tick ([scripts.md](scripts.md#when-a-request-is-written)).

| What you see | Why |
|---|---|
| A value a script set is unchanged right after `tick` returns. | The request is written in step 1 of the next `tick`. |
| A script's second `set` lands a tick after its first. | After asking for something, a script reads values on the next tick. A `set` whose new worth reads a value waits for it. |
| An actor a script spawned stands idle for a tick. | It is spawned in step 1, and takes part from the tick after. `Sprites` reports it at once. |
| `Actors::set` from your code shows at once. | Your own call writes directly. |

## A count comes up short

Several scripts each did `set(Count, Count + 1)` on the same tick. Each read the same `Count`, and the
writes replaced one another.

Fix: count with a command ([scripts.md](scripts.md#count-with-a-command)).

## An actor is missing from the screen

| Cause | Fix |
|---|---|
| Your frame leaves out the `Sprites` call for the actor's `slot`, or the names differ. | Call `Sprites` with the same slot name, and hand the result to a layer. |
| The actor is in another area than the `place` you passed. | `Sprites` returns the actors of one place. Ask for every area `camera.shown(world)` lists ([drawing.md](drawing.md#a-layer-for-each-area)). |
| The actor is in the area scrolling in, and appears only when the scroll ends. | Your frame draws the camera's area alone. Draw every area `camera.shown(world)` lists, each on its own layer ([drawing.md](drawing.md#a-layer-for-each-area)). |
| Every look's condition is false and the actor has an empty `mover`. | End the `look` list with an unconditional look. |
| The mover's frame is a palette-only frame. | Give the walk animations frames with a `tileIndex`. |
| The actor was removed. | Check `state.actors.has(id)`. |
| The slot's depth is under a map layer that covers it. | Raise the slot's `z` in your world. |

## An actor is drawn in the wrong place

| Cause | Fix |
|---|---|
| The sprite layer has a different scroll from the world. Sprites are in the area's pixels. | Give each area's layer the scroll `camera.shown(world)` lists for that area ([drawing.md](drawing.md#a-layer-for-each-area)). |
| The position is treated as the actor's center or feet. It is the top-left of its art. | Offset by the art's size where you set the position. |
| The actor is drawn over something it stands behind, or under something it stands in front of. | Sprites sort by the bottom of the box. Put the box at the actor's feet ([drawing.md](drawing.md#depth)). |
| Two things must always stack one way. | Give them different slots. |

## An actor slides across the screen when moved

The engine eases each sprite from its previous position by matching sprite keys between ticks, across
every layer of the frame.

| When | Cause | Fix |
|---|---|---|
| After `moveTo` | The sprite is eased from the old position to the new one. | Call `state.actors.fresh(id)` on the tick you move it ([drawing.md](drawing.md#fresh-keys)). |
| When the player comes into the next area | Its position jumps to the next area's pixels and its sprite to that area's layer. | Call `fresh` for it on the tick `step.entered` holds a place ([how-to.md](how-to.md#follow-the-player-with-the-camera)). |
| When the camera changes area, every actor slides into place | The actors are on one layer for every area, such as `WorldKit::Actors`. Its scroll jumps at the change, and the engine eases it. | Draw each area's actors on a layer of that area's own ([drawing.md](drawing.md#a-layer-for-each-area)). |

## An actor stands still

| Cause | Fix |
|---|---|
| Its `mover` is empty. | Set `.mover` with `movedBy`. |
| Its drive is a direction value that holds `Dir::None`. | Have a script set the value, or start it at a direction in `ownValues`. |
| Its drive is a member of your input that stays empty. | Fill the member each tick. |
| Its drive function returns an empty heading. With the player in another area, `PlayerX` and `PlayerY` equal the actor's own position. | This is correct behavior for a heading toward the player. |
| The camera is in a scroll or a fade. Walking waits until it ends. | Keep calling `camera.follow` every tick so the transition finishes. |
| `tick` is skipped for the actor's set. | Call `tick` once per tick for each `Actors` value. |
| It is frozen: `state.actors.frozen(id)` is true. | A frozen actor stands. Wake it with `state.actors.wake(id)` ([actors.md](actors.md#freeze-wake-and-frozen)). |
| It is held by something solid. | See [An actor is held in place](#an-actor-is-held-in-place). |
| The mover itself stands. | For a `TopDownMovement` walker see that module's `docs/troubleshooting.md`, "The walker stands still". |

## An actor walks through things

| Cause | Fix |
|---|---|
| The other actor's `solid` is false, or its box is empty. | Set `.solid`, and give it a box. |
| The two actors are in different `Actors` values. | Walking actors stop at the solid actors of their own set. |
| The walker's own collision shape is empty. | Set the shape in the mover's config. |
| The actor was put there by `spawn` or `moveTo`. Both place it exactly, walls included. | Choose a position in open ground. |
| The map's walkable value differs from the layout's `open`. | Set `open` in your `WorldKit::Layout`. |
| The two boxes already overlapped as the walk began: two solid actors that overlap walk apart ([movers.md](movers.md#what-a-mover-collides-with)). | Spawn and move actors apart. For a gate that shuts while a walker stands in it, have its script wait until the box is clear: put `waitWhile(ActorKit::Touched)` before the `set`, and list `ActorKit::On::Touch` in the walker's `setsOff`. |

## An actor is held in place

A walker whose box overlaps a solid cell of its area's map stays where it is.

| Cause | Fix |
|---|---|
| The actor was spawned or moved into a wall. | Choose a position in open ground. |

## An actor stops at a doorway

Every actor but the player has its walk held inside its [confinement](actors.md#confinement), and the
default, `Area`, holds it inside its area, at an open doorway included.

| Cause | Fix |
|---|---|
| Its confinement is `Area`, the default. | Set `state.actors.confinement(id, ActorKit::Confinement::Layout)`, or `None` for a doorway into another layout. |
| Its confinement is `Layout`, and the doorway leads to another layout. | Set `ActorKit::Confinement::None`. |
| The doorway's opening leads nowhere: its `leadsTo.area` is `WorldKit::kNoArea`. | Every actor stops there. Give the opening a `leadsTo` in your world. |
| It is the player's actor and stops anyway. | Call `state.actors.player(id)` for it ([how-to.md](how-to.md#follow-the-player-with-the-camera)). |

To take an actor through while its walk stays in its area, move it from your code when it reaches the edge
([how-to.md](how-to.md#send-an-actor-to-another-area)).

## An actor stands beyond its area's edge

Its confinement let it walk out through an opening. ActorKit keeps it in its area, in that area's
pixels, beyond the edge.

| Cause | Fix |
|---|---|
| Your code leaves it in its area once it has walked out. | Move it into the next area with `moveTo` when its box has passed the edge, and call `fresh` on that tick ([how-to.md](how-to.md#send-an-actor-to-another-area)). |
| It was meant to stay in its area. | Set its confinement back to `ActorKit::Confinement::Area`. |

## The hero walks off the screen

The player walks out through an opening and the screen stays where it was. ActorKit reads the
camera and your game moves it.

Fix: after `ActorKit::tick`, call `camera.follow` with the hero's box and apply the step with `moveTo`
([how-to.md](how-to.md#follow-the-player-with-the-camera)).

## Actors in two sets ignore each other

Touch, near, Interact and solidity work within one `Actors` value.

| To | Do |
|---|---|
| have them touch, block or act on each other | Spawn them into the same `Actors` value. |
| have one react to an event in the other | Exchange through a value of your game's, with both sets ticked under the same bindings ([how-to.md](how-to.md#keep-several-sets-and-choose-their-layers)). |

## Compile errors

| Code that fails to compile | Cause | Fix |
|---|---|---|
| A game state whose `ActorKit::Actors` member is default-constructed | A set is constructed from your `retropp::EngineConfig`. | Write `State state{.actors = ActorKit::Actors{engineConfig}};`, or give the member a default initializer. |
| An `Actor` with designated initializers out of order | C++ requires declaration order. | Use the order `slot`, `box`, `solid`, `ownValues`, `look`, `scripts`, `mover`, `actions`, `setsOff`. |
| `.ownValues = {{Health, true}}` for a `Value<int>` | The starting worth has exactly the value's type. | Write `{Health, 3}`. |
| `.actions = {{Action::Interact, &Input::move}}`, where `move` is another type than `bool` | An action row is driven by a `bool` member. | Add a `bool` member to your input and fill it with `justPressed`. |
| `movedBy<M>(config, drive)` with a drive of another kind | The drive is a member of your input, a `ScriptKit::Value<retropp::Dir>`, or a function of `const ScriptKit::Values&`. | Use one of the three ([movers.md](movers.md#the-three-drives)). |
| `movedBy<M>(…)` with an `M` that lacks one of the mover's members | The mover type has a fixed set of members. | See [movers.md](movers.md#a-mover-of-your-own). |
| A handler written `[](State&, const ScriptKit::Arguments&)` | An `ActorKit` handler also takes the asking actor's id. | Write `[](State& state, ActorKit::Id asker, const ScriptKit::Arguments& arguments)`. |
| `call(toggle(Open))`, or `stop` or `restart` of a script made by a function | Those three keep the address of a script that outlives the body. | Name a script constant, listed by address. |
| `set(Health, true)`, `when(Health, {…})` and other mismatched types in a body | ScriptKit checks value types where a script is written. | See ScriptKit's `docs/scripts.md`, "What does not build". |

## Exceptions

| Call | Throws | When |
|---|---|---|
| `Actors::spawn` | `std::invalid_argument` | The actor has both a mover and a `box` with a size. |
| `Actors::spawn` | `std::invalid_argument` | A value is listed twice in `ownValues`. |
| `Actors::spawn` | `std::invalid_argument` | A script in `scripts` is a null pointer. |
| `Actors::spawn` | `std::logic_error` | ScriptKit refuses the actor's scripts: two of one name, a script naming one outside the list, a script that calls itself, or an interrupt ring. |
| `Actors::place`, `position`, `box`, `facing`, `touching`, `moveTo`, `key`, `fresh`, `get`, `set`, `act`, `processor`, `confinement`, `freeze`, `wake`, `frozen`, and `player(id)` with an id other than `kNoId` | `std::out_of_range` | The id's actor was removed, or the set has yet to hand the id out. The message names the id. |
| `Actors::get`, `Actors::set` | `std::out_of_range` | The value is outside the actor's `ownValues`. |
| `TriggerMap` constructor | `std::invalid_argument` | A trigger is bound twice. |
| `lookOf` | `std::out_of_range` | The slot is past the sheet's last. |
| `tick` | `std::invalid_argument` | `config.world` is null. |
| `tick` | `std::out_of_range` | An awake actor is in a layout or area outside the world. A frozen actor's place goes unchecked until it wakes. |
| `tick`, `Sprites` | `std::out_of_range` | An actor reads a value outside its own, ActorKit's six and your bindings. It is thrown on the first tick or frame that reads it. The message names the actor and the value's id. |
| `tick` | `std::out_of_range` | A script sets a value of your game's and the config's bindings are empty or lack it. |
| `tick` | `std::out_of_range` | A script asks for a command and the config's handlers are empty or lack it. The message names the command. |
| `tick` | `std::logic_error` | A mover or an action row is driven by a member of another type than the input handed to `tick`. |
| `tick`, `Sprites` | `std::logic_error` | The config's bindings or handlers are written for another type than the state handed to the call. |
| `Sprites` | what the mover's `frame` throws | A `TopDownMovement` walker throws `std::logic_error` when the walk animation for its direction is empty. |
| a drive function calling `ScriptKit::Values::get` | `std::out_of_range` | The function reads a value of your game's that the actor's scripts, `solid` and looks leave unread ([movers.md](movers.md#a-function-of-the-actors-values)). |
| `WorldKit::ActorZ`, `WorldKit::Actors` | `std::out_of_range` | The area lacks the slot. These are WorldKit's. |

`Actors::has`, `remove`, `size`, `ids` and `player()` are exception-free. An exception thrown by one
of your bindings, handlers or movers passes through `tick`.
