# ActorKit

**Date:** 2026-11-22
**Status:** In testing — the module, its guide and its example program are in place; the game's own actors
are built on it next.

## Concept

ActorKit is the part of the WorldKit module for what lives in a world: a hero, a hen, a gate, a sign. An
actor is a plain value the game declares — the slot it is drawn at, its box, whether it is solid, values
of its own, the looks it chooses between, its scripts, what moves it, the actions it performs and the
triggers it sets off — and any number of live actors are spawned from one value, at any time, into an
`ActorKit::Actors` set in the game's state. One call a tick runs a whole set: it writes what the actors'
scripts asked for, walks every mover, performs the actions, works out what happened between the actors,
and steps their scripts. Asked, an actor reports where it is, and a set reports the sprites of an area's
slot for the game to draw.

A world runs its actors in one of three ways, all in the one module:

| The game wants | It uses |
|---|---|
| a world with its own system for what lives in it | WorldKit's world, camera, solids and layers; ActorKit stays out of the build |
| ActorKit, wired its own way | an `Actors` set, `ActorKit::tick` and `ActorKit::Sprites` beside its own camera calls |
| the whole world run for it | `WorldKit::Scene`, over the actors written in the world |

## Design decisions

- **An actor is a value.** A game declares it once, or writes a function that makes one, and spawns from
  it as often as it likes. The value copies, so a set copies whole, and a copied game state runs on by
  itself.
- **A set detects within itself.** Touch, near, interact and solidity are worked out between the actors of
  one set; two sets exchange through the game's values. A game that wants every actor to meet every other
  keeps them in one set, which is what the scene does.
- **Each actor runs its own copy of its scripts,** and one actor affects another through a value the other
  reads, or through a command the game's handler carries out. A script reaches only its own actor.
- **Movers are taken by their shape.** `movedBy` accepts any type with a walk, a facing, a collision shape
  and a frame, so ActorKit includes the engine's, LayerKit's, WorldKit's and ScriptKit's headers alone;
  TopDownMovement's walkers are two such types. A
  mover is driven by a member of the game's input, a direction value, or a function of the actor's values.
- **Triggers are bound like input actions.** ActorKit's own — `Interact`, `Touch`, `Near`, `Enter`,
  `Becomes` — are ready to use, and a game names its own in a `TriggerMap`, each bound to a detection.
- **The camera is read and left as it is, and sprites are reported.** `tick` reads which area the camera
  is in and whether a scroll or fade runs; `Sprites` reports one area's slot. Moving the camera and
  building the layers are the game's calls, or the scene's.
- **Sprite keys stay apart across sets.** Each set takes a number of its own, part of every key it reports;
  `fresh` gives a moved actor a throwaway key for a tick, so it appears where it lands.
- **The player** is the one actor of a set the camera follows: it walks out through its area's openings,
  and `PlayerHere`, `PlayerX` and `PlayerY` are filled in from it for every actor's scripts.
- **Confinement** says what each other actor's walk is held inside: its area (the default), its layout,
  or the map's walls alone.
- **Freeze and wake** hold an actor exactly as it is — position, values, scripts and the ticks left in
  their waits — while it is still drawn and still solid, and let it carry on from there.
- **ActorKit is a part of WorldKit, with the scene over it.** It needs WorldKit's places, maps, solids and
  camera, so it ships from WorldKit's directory as a target of its own, `ActorKit::ActorKit`, with headers
  of its own, `"ActorKit/…"`. `WorldKit::Scene` runs it: actors written in an area's, a layout's or the
  world's `actors`, the level being how far each may roam; the camera following one of them; every other
  actor carried through the openings its reach allows, or stopped, despawned or hidden at its edge; each
  actor awake or frozen by a rule of its own; one sprite layer for each area on screen and slot. Every
  ActorKit call stays available on the scene's actors.

## Implementation details

| Part | Where |
|---|---|
| the actor, the set, the config, the tick, the sprites | `modules/WorldKit/include/ActorKit/` — `actor.h`, `actors.h`, `config.h`, `tick.h`, `sprites.h`, with `look.h`, `mover.h`, `values.h`, `triggers.h`, `commands.h` |
| the actor entries of the world, and the scene | `modules/WorldKit/include/WorldKit/area.h`, `scene.h` |
| the guide | `modules/WorldKit/docs/actors/` (ten pages and an index), and `modules/WorldKit/docs/scene.md` |
| the example program | `WorldKit-example-farm`, `modules/WorldKit/examples/farm/`: a farmyard of three areas, its pond rippling under a shader of its own |
| the tests | `tests/test_ActorKit.cpp`, `test_ActorKit_confinement.cpp`, `test_ActorKit_freezing.cpp`, `test_WorldKit_scene.cpp`, `test_WorldKit_scene_actors.cpp` |

A tick of a set, in order: requests written, solidity worked out, movers walked (between the camera's
scrolls and fades), actions performed, triggers worked out, scripts stepped. An actor spawned during a tick takes part
from the next.

## Open questions / future work

- **A switchboard,** a system in the game's state through which one actor's script affects others by
  values alone, streamlining what the game's bound values do today.
- **The game's own actors** — the player among them, and a wandering enemy — built on the scene.
- **One screen-wide actor layer** across a seam, so two actors overlapping on the seam line, one in each
  area, sort by their feet; today they are ordered by their areas.
