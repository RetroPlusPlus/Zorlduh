# ActorKit guide

Documentation for ActorKit, the WorldKit module's actors: actors for a top-down game on the Polyrhythm
engine, in a WorldKit world, with their scripts written in ScriptKit.

New to ActorKit? Read [getting-started.md](getting-started.md), then [concepts.md](concepts.md).

## Pages

| Page | Contents |
|---|---|
| [getting-started.md](getting-started.md) | A complete example program, a farmyard, explained step by step. |
| [concepts.md](concepts.md) | How the module works: the actor value, a set of actors, the tick, values, triggers, movers, the camera, drawing, several sets, lifetimes. |
| [actors.md](actors.md) | Reference for `Actor`, `Id`, `Side`, `Scripts`, `Actions`, `Confinement`, `Actors`, `Config` and `tick`. |
| [triggers.md](triggers.md) | What starts a script. ActorKit's own triggers, triggers of your own, the exact rule of each detection. Reference for `TriggerMap` and the detections. |
| [scripts.md](scripts.md) | Scripts on an actor, the three kinds of value, when a request is written, sharing a script, commands. Reference for `OwnValue`, the six values, `spawn`, `remove` and `Handlers`. |
| [movers.md](movers.md) | What moves an actor: the mover's shape, the three drives, what it collides with, a mover of your own. Reference for `movedBy`, `Mover` and `Solids`. |
| [drawing.md](drawing.md) | What an actor shows, slots, depth, sprite keys. Reference for `Look`, `lookOf`, `Looks` and `Sprites`. |
| [how-to.md](how-to.md) | Recipes for specific tasks. |
| [troubleshooting.md](troubleshooting.md) | Symptoms, causes and fixes. Compile errors and exceptions. |
| [testing.md](testing.md) | Testing code that uses the module. |

## Find it by task

| I want to | Read |
|---|---|
| Put a character or an object in the world | [getting-started.md](getting-started.md), [concepts.md](concepts.md#overview) |
| Make the player an actor | [how-to.md](how-to.md#make-the-player-an-actor) |
| Follow the player with the camera | [how-to.md](how-to.md#follow-the-player-with-the-camera) |
| Make something happen when the player presses a button at an actor | [triggers.md](triggers.md#actorkits-own-triggers), [how-to.md](how-to.md#open-and-shut-something-with-interact) |
| Make something happen when the player walks onto or near an actor | [triggers.md](triggers.md#the-detections) |
| Make a door or a gate that is solid until it opens | [how-to.md](how-to.md#make-an-actor-solid-while-a-value-holds) |
| Give every enemy its own health or timer | [scripts.md](scripts.md#values), [how-to.md](how-to.md#give-each-actor-a-value-of-its-own) |
| Use one script on many actors | [scripts.md](scripts.md#share-a-script-with-a-function) |
| Have one actor react to another | [how-to.md](how-to.md#have-one-actor-react-to-another) |
| Count pickups, hits or kills | [scripts.md](scripts.md#count-with-a-command) |
| Spawn or remove an actor from a script | [scripts.md](scripts.md#commands) |
| Make an actor walk on its own | [movers.md](movers.md#the-three-drives), [how-to.md](how-to.md#walk-toward-the-player) |
| Use a mover of my own | [movers.md](movers.md#a-mover-of-your-own) |
| Change what an actor looks like | [drawing.md](drawing.md#what-an-actor-shows) |
| Draw actors on different layers | [drawing.md](drawing.md#slots), [how-to.md](how-to.md#keep-several-sets-and-choose-their-layers) |
| Warp an actor so it appears in place | [how-to.md](how-to.md#warp-an-actor) |
| Let an actor other than the player walk out through doors and seams | [actors.md](actors.md#confinement), [how-to.md](how-to.md#send-an-actor-to-another-area) |
| Hold an actor still, exactly as it is, and let it carry on later | [actors.md](actors.md#freeze-wake-and-frozen) |
| Declare a trigger of my own | [triggers.md](triggers.md#triggers-of-your-own) |
| Fix a script that stays off, or an actor that is missing from the screen | [troubleshooting.md](troubleshooting.md) |
| Write tests | [testing.md](testing.md) |

## Find it by name

| Name | Header | Page |
|---|---|---|
| `Actor` | `actor.h` | [actors.md](actors.md#actor) |
| `Id`, `kNoId` | `actor.h` | [actors.md](actors.md#id) |
| `Side` | `actor.h` | [actors.md](actors.md#side) |
| `ScriptRef`, `Scripts` | `actor.h` | [actors.md](actors.md#scriptref-and-scripts) |
| `ActionRow`, `Actions` | `actor.h` | [actors.md](actors.md#actionrow-and-actions) |
| `Confinement` | `actors.h` | [actors.md](actors.md#confinement) |
| `Actors` | `actors.h` | [actors.md](actors.md#actors) |
| `Actors::confinement` | `actors.h` | [actors.md](actors.md#confinement-1) |
| `Actors::freeze`, `wake`, `frozen` | `actors.h` | [actors.md](actors.md#freeze-wake-and-frozen) |
| `Config`, `AnyBindings`, `AnyHandlers` | `config.h` | [actors.md](actors.md#config) |
| `tick` | `tick.h` | [actors.md](actors.md#tick) |
| `On`, `Interact`, `Touch`, `Near`, `Enter`, `Becomes` | `triggers.h` | [triggers.md](triggers.md#on) |
| `TriggerMap` | `triggers.h` | [triggers.md](triggers.md#triggermap) |
| `actedOn`, `touched`, `near`, `entered`, `becomes` | `triggers.h` | [triggers.md](triggers.md#the-detection-functions) |
| `TriggerKind`, `TriggerKinds`, `ActionName`, `Detection`, `DetectionKind` | `triggers.h` | [triggers.md](triggers.md#triggerkind-actionname-and-detection) |
| `OwnValue`, `OwnValues` | `values.h` | [scripts.md](scripts.md#ownvalue) |
| `Touched`, `X`, `Y`, `PlayerHere`, `PlayerX`, `PlayerY` | `values.h` | [scripts.md](scripts.md#the-six-values) |
| `spawn`, `remove` | `commands.h` | [scripts.md](scripts.md#spawn-and-remove) |
| `Handler`, `Handlers` | `commands.h` | [scripts.md](scripts.md#handlers) |
| `movedBy`, `Mover` | `mover.h` | [movers.md](movers.md#movedby) |
| `Solids` | `mover.h` | [movers.md](movers.md#solids) |
| `Look`, `lookOf` | `look.h` | [drawing.md](drawing.md#look) |
| `LookRule`, `Looks` | `look.h` | [drawing.md](drawing.md#lookrule-and-looks) |
| `Sprites` | `sprites.h` | [drawing.md](drawing.md#sprites) |

## What the module does

| The module does | Your game does |
|---|---|
| Keeps every live actor: its area, position, own values, mover and running scripts | Declares the actor values and spawns them |
| Walks each actor with a mover, against its area's collision map and the other solid actors, held inside its confinement | Reads the player's input into its own `Input` value once a tick |
| Holds a frozen actor exactly as it is, and lets it carry on when woken | Freezes and wakes the actors it wants held |
| Detects what happens between actors: acted on, touched, near, entered, a value becoming true | Names its Interact action, and declares any triggers of its own |
| Starts the scripts a trigger sets off, and steps every actor's scripts | Writes the scripts |
| Writes what the scripts ask for: own values itself, your values through your bindings, your commands through your handlers | Binds its values and handles its commands |
| Reports an actor's place, position, box, facing and key | Moves the camera, with `WorldKit::Camera`, and carries the player where the camera says |
| Reports the sprites of an area's slot | Draws: puts each area's sprites, slot by slot, on a layer of that area's own, beside any sprites of its own |

ActorKit reads the WorldKit world and camera you hand it and leaves both as they are.

## Provided

| Feature | Page |
|---|---|
| Actors declared as values, spawned and removed at any time | [concepts.md](concepts.md#an-actor-is-a-value) |
| Ids handed out once | [actors.md](actors.md#id) |
| Values an actor holds as its own | [scripts.md](scripts.md#values) |
| Solidity and looks that follow from values | [concepts.md](concepts.md#an-actor-is-a-value) |
| Ready-made triggers: interact, touch, near, enter, becomes | [triggers.md](triggers.md#actorkits-own-triggers) |
| Triggers of your own, bound to the same detections | [triggers.md](triggers.md#triggers-of-your-own) |
| A side for interact: from above, below, left or right | [triggers.md](triggers.md#the-detections) |
| A copy of each script per actor | [scripts.md](scripts.md#scripts-on-an-actor) |
| Scripts shared through a function of their values | [scripts.md](scripts.md#share-a-script-with-a-function) |
| Spawn and remove from a script | [scripts.md](scripts.md#commands) |
| Handlers handed the asking actor | [scripts.md](scripts.md#handlers) |
| Movers of your own type, driven by input, a value or a function | [movers.md](movers.md) |
| Walking against the map and the other solid actors | [movers.md](movers.md#what-a-mover-collides-with) |
| A walk confined to the actor's area, its layout, or the map's walls alone | [actors.md](actors.md#confinement) |
| Actors frozen and woken one at a time, still drawn and solid while frozen | [actors.md](actors.md#freeze-wake-and-frozen) |
| Sprites sorted by the bottom of the box | [drawing.md](drawing.md#depth) |
| Sprite keys that stay apart across sets | [drawing.md](drawing.md#keys) |
| A fresh key for a move shown at once | [drawing.md](drawing.md#fresh-keys) |
| Several sets of actors, each ticked and drawn on its own | [concepts.md](concepts.md#several-sets) |
| A state value that copies whole | [concepts.md](concepts.md#lifetimes) |

## Left to your game

Each of these is yours to add. The page shows how.

| Feature | How | Page |
|---|---|---|
| Moving the camera | Call `WorldKit::Camera::follow` with the actor's box, and put the actor where the camera says, or `WorldKit::Scene` does it | [how-to.md](how-to.md#follow-the-player-with-the-camera), [../scene.md](../scene.md) |
| Drawing | Put `Sprites` on a layer for each area on screen and each slot, or `WorldKit::Scene` does it | [drawing.md](drawing.md#a-layer-for-each-area), [../scene.md](../scene.md#drawing) |
| An actor other than the player going through a door | Move it with `moveTo` when it reaches the door. Its `confinement` is what lets its walk cross the door first. Or `WorldKit::Scene` does it. | [how-to.md](how-to.md#send-an-actor-to-another-area), [../scene.md](../scene.md#where-an-actor-goes-through-an-opening) |
| An actor shown in place as it changes area | Call `fresh` for it on the tick it moves into the next area, or `WorldKit::Scene` does it | [drawing.md](drawing.md#fresh-keys), [../scene.md](../scene.md#what-the-scene-does-in-a-tick) |
| Detection between two sets | Put the actors in one set, or exchange through a value | [concepts.md](concepts.md#several-sets) |
| Counting things that happen on one tick | Use a command | [scripts.md](scripts.md#count-with-a-command) |
| Random behavior | Bind a value of yours that your tick rolls | [how-to.md](how-to.md#give-a-script-something-random) |
| Saving and loading | Copy your state: an `Actors` value copies whole | [concepts.md](concepts.md#lifetimes) |
