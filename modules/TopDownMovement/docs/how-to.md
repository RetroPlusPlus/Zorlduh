# How-to

Recipes for common tasks. Each one is independent. The snippets use these names from your game: `hero`,
`bat`, `guard` and `ghost` are walkers, `heroConfig` and the like are their `WalkerConfig`s, `walls` is
a `TileCollision`, and `input` is the tick's `DirectionalInput`.

## Contents

Walls and maps

- [Switch maps](#switch-maps)
- [Combine solids](#combine-solids)
- [Give different walkers different rules](#give-different-walkers-different-rules)
- [Stop walkers walking through each other](#stop-walkers-walking-through-each-other)
- [Spawn clear of walls](#spawn-clear-of-walls)

Movement

- [Move in four directions only](#move-in-four-directions-only)
- [Walk slower than a pixel a tick](#walk-slower-than-a-pixel-a-tick)
- [Wander against walls](#wander-against-walls)
- [Chase a target](#chase-a-target)
- [Push a walker](#push-a-walker)
- [Move a walker with your own mover](#move-a-walker-with-your-own-mover)

Routes

- [Patrol a route](#patrol-a-route)
- [Break off a route and come back](#break-off-a-route-and-come-back)
- [Walk the player along a route](#walk-the-player-along-a-route)

Input

- [Build a `DirectionalInput` by hand](#build-a-directionalinput-by-hand)

Sprites and facing

- [Give the diagonals their own sprites](#give-the-diagonals-their-own-sprites)
- [Turn to face something](#turn-to-face-something)
- [Pin something to a walker](#pin-something-to-a-walker)

State

- [Keep a walker and its position in one value](#keep-a-walker-and-its-position-in-one-value)

## Switch maps

Pass the current map's solids on each call. A walker stores no map.

```cpp
hero.walk(heroConfig, input, maps[state.currentMap].solids());
```

To move to another map, change which map is current and place the walker at the entrance, in the new
map's pixels:

```cpp
state.currentMap = door.leadsTo;
hero.place(door.arrival);
```

`place` does not test collision and resets the walk animation to frame 0.

The engine eases a sprite from its previous position to its new one. To make the walker appear at the
entrance instead of sliding there, draw it with a different sprite key on that tick (the engine's
`docs/guide/sprites.md`, "Identity — `key`").

## Combine solids

Write one callable that asks every source:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return walls.solidBox(box) || doors.solidBox(box) || overlaps(box, state.block);
};

hero.walk(heroConfig, input, solids);
```

`overlaps` is your own rectangle test. [collision.md](collision.md#your-own-solids) has one.

## Give different walkers different rules

Solids are passed per call, so two walkers on one map can use different ones. Here water stops the hero
and not the fish:

```cpp
const auto landSolids = [&](retropp::IntRect box) { return walls.solidBox(box) || water.solidBox(box); };
const auto swimmerSolids = [&](retropp::IntRect box) { return walls.solidBox(box); };

hero.walk(heroConfig, input, landSolids);
fish.walk(fishConfig, fishHeading, swimmerSolids);
```

`walls` and `water` are two `TileCollision`s over two maps.

## Stop walkers walking through each other

To make walkers block each other, add every other walker's collision box to the solids. The module does
not do this on its own.

```cpp
// The bounding box of a walker's collision shape, in world pixels.
retropp::IntRect boxOf(const TopDownMovement::Walker& walker, const TopDownMovement::WalkerConfig& config) {
    const retropp::IntRect box = config.collision.bounds();
    const retropp::Vec2i at = walker.position();
    return {.x = at.x + box.x, .y = at.y + box.y, .width = box.width, .height = box.height};
}

for (TopDownMovement::Walker& bat : bats) {
    const auto solids = [&](retropp::IntRect box) {
        if (walls.solidBox(box)) {
            return true;
        }
        for (const TopDownMovement::Walker& other : bats) {
            if (&other != &bat && overlaps(box, boxOf(other, batConfig))) {
                return true;
            }
        }
        return false;
    };
    bat.walk(batConfig, headingOf(bat), solids);
}
```

- The walker must skip itself, or it blocks its own movement.
- Walkers move in loop order. A later walker sees the earlier ones where they are after moving.
- Two walkers that overlap cannot move apart. Spawn them apart.
- `boxOf` uses the bounding box. It is exact for a box shape and larger than the shape for any other.

## Spawn clear of walls

Test the position with the walker's collision shape before placing it:

```cpp
if (!batConfig.collision.hits(walls, spawn)) {
    bat.place(spawn);
}
```

`hits(solids, position)` is the test `walk` uses. To search for a free position, try candidates until
one does not hit:

```cpp
for (const retropp::Vec2i candidate : spawnPoints) {
    if (!batConfig.collision.hits(walls, candidate)) {
        bat.place(candidate);
        break;
    }
}
```

## Move in four directions only

Set `movement` in the config:

```cpp
const TopDownMovement::WalkerConfig heroConfig{
    .movement = TopDownMovement::Ways::Four,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};
```

With two directions held, an `InputWalker` moves along the one pressed more recently. A `Walker` given a
two-axis heading moves the way it faces if that is one of the two directions, and otherwise
horizontally. Routes are not affected.

## Walk slower than a pixel a tick

`speed` is whole pixels per tick. There are two ways to go slower.

**On a route.** A route's pace is in pixels per second and can be any value. A route does not collide.

```cpp
retropp::SpritePath stroll = TopDownMovement::routeFrom(ghost, {
    {.move = retropp::SpritePathMove::to({120.0f, 40.0f}), .pacing = retropp::PathPacing::speed(20.0f)},
});

TopDownMovement::follow(ghost, ghostConfig, stroll, retropp::PlaybackMode::single());   // every tick
```

**By walking on some ticks.** Call `walk` only on the ticks the walker should move. This collides with
walls. The walk animation advances only on those ticks, so it slows down with the walker.

```cpp
if (tick % 2 == 0) {
    bat.walk(batConfig, batHeading, walls);   // half a pixel per tick
}
```

When the walker should stop, call `walk` with an empty heading so it shows frame 0.

## Wander against walls

Walk a heading, and change it when the walker is blocked. `moving()` is `false` after a `walk` that did
not move the walker.

```cpp
const std::array<TopDownMovement::Directions, 4> kClockwise{{
    {.right = true}, {.down = true}, {.left = true}, {.up = true},
}};

wanderer.walk(wandererConfig, kClockwise[heading], walls);
if (!wanderer.moving()) {
    heading = (heading + 1) % kClockwise.size();
}
```

`heading` is a `std::size_t` you keep in your game state.

## Chase a target

Build the heading from the two positions each tick:

```cpp
const retropp::Vec2i me = bat.position();
const TopDownMovement::Directions toward{
    .up = target.y < me.y, .down = target.y > me.y, .left = target.x < me.x, .right = target.x > me.x};

bat.walk(batConfig, toward, walls);
```

The bat moves diagonally until it is level with the target on one axis, then straight. It slides along
walls and does not find a way around them. To flee, swap the comparisons.

## Push a walker

To move a walker without turning it or changing its walk animation, write to its position. The position
is yours. Test each pixel with `hits` to respect walls:

```cpp
// Knock the hero back up to 3 pixels. `push` is -1, 0 or 1 on each axis.
for (int i = 0; i < 3; ++i) {
    const retropp::Vec2i next{heroAt.x + push.x, heroAt.y + push.y};
    if (heroConfig.collision.hits(walls, next)) {
        break;
    }
    heroAt = next;
}
```

Use the same approach for a conveyor belt or a moving platform.

## Move a walker with your own mover

When your own code decides where a walker goes — a tween, a curve, a physics step — call `moveTo` each
tick with the new position and how far the walker moved. The walker turns to face its travel and plays
its walk animation.

```cpp
// A ghost drifting along a wave. `drift` is its float position, kept in game state.
const retropp::Vec2 before = drift;
drift = {drift.x + 0.5f, 60.0f + 8.0f * std::sin(drift.x / 16.0f)};

ghost.moveTo(ghostConfig,
             {.x = static_cast<int>(std::lround(drift.x)), .y = static_cast<int>(std::lround(drift.y))},
             {drift.x - before.x, drift.y - before.y});
```

- `moveTo` does not test collision.
- Pass the travel as floats, before rounding. A mover slower than a pixel a tick still animates.
- Pass a zero travel on ticks the mover does not move. The walker then shows frame 0.

## Patrol a route

Build a route that ends where it starts, and follow it with the default mode, which loops:

```cpp
retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}),  .pacing = retropp::PathPacing::speed(30.0f)},
});

TopDownMovement::follow(guard, guardConfig, patrol);   // every tick
```

- The `wait` leg goes to the point it starts from, so the guard stands for its duration.
- `patrol.currentNode()->label` is the leg the guard is on.
- For a curved leg use `SpritePathMove::through(points)` or `hermite(…)` (the engine's
  `docs/guide/sprite-path.md`).

[routes.md](routes.md) covers routes in full.

## Break off a route and come back

Interrupt the route with a detour. The walker walks the detour on the following ticks, and the route
resumes when the detour ends.

```cpp
patrol.interrupt({{.move = retropp::SpritePathMove::to(noise), .pacing = retropp::PathPacing::speed(60.0f)}},
                 retropp::PlaybackMode::single(), retropp::ResumePolicy::Return);
```

| `ResumePolicy` | After the detour |
|---|---|
| `Return` | The walker is put back where it left the route. |
| `Continue` | The route carries on from where the detour ended, shifted by the detour's displacement. |

For a detour that lasts until your code ends it, pass `PlaybackMode::loopIndefinitely()` and call
`patrol.popInterrupt()` when it should end.

To switch a walker from a route to a heading, stop calling `follow` and call `walk`. To go back, build a
new route with `routeFrom`, which starts where the walker now stands.

## Walk the player along a route

`follow` takes any walker, including an `InputWalker`. For a scripted walk, follow a route instead of
calling `walk`, then go back to `walk` when the route ends:

```cpp
// When the scripted walk starts.
state.scriptedWalk = TopDownMovement::routeFrom(hero, {
    {.move = retropp::SpritePathMove::to({120.0f, 40.0f}), .pacing = retropp::PathPacing::speed(40.0f)},
});
state.walkingScript = true;

// Every tick.
if (state.walkingScript) {
    TopDownMovement::follow(hero, heroConfig, state.scriptedWalk, retropp::PlaybackMode::single());
    state.walkingScript = !state.scriptedWalk.finished();
} else {
    hero.walk(heroConfig, input, walls);
}
```

- The pad is ignored while the route runs.
- A route does not collide. Draw it through open ground.
- When control returns, a direction the player is still holding moves the hero on the next tick.

## Build a `DirectionalInput` by hand

`readDirections` reads player slot 0. For anything else, fill in the struct yourself.

A second player, from slot 1:

```cpp
const auto pad = engineInput.player(1);
const TopDownMovement::DirectionalInput second{
    .held = {.up = pad.isHeld(Action::Up), .down = pad.isHeld(Action::Down),
             .left = pad.isHeld(Action::Left), .right = pad.isHeld(Action::Right)},
    .pressed = {.up = pad.justPressed(Action::Up), .down = pad.justPressed(Action::Down),
                .left = pad.justPressed(Action::Left), .right = pad.justPressed(Action::Right)},
};

playerTwo.walk(heroConfig, second, walls);
```

`engineInput` is the tick's `retropp::InputState`.

A fixed input, for a test or a replay:

```cpp
hero.walk(heroConfig, {.held = {.right = true}, .pressed = {.right = true}}, walls);
```

Set `pressed` only on the tick a direction goes down. If you never set it, the walker still moves by
`held`; when two directions are held and it faces neither, it faces the horizontal one.

## Give the diagonals their own sprites

Set `sprites` in the config and provide all eight animations:

```cpp
const TopDownMovement::WalkerConfig knightConfig{
    .sprites = TopDownMovement::Ways::Eight,
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight,
             .upLeft = walkUpLeft, .upRight = walkUpRight, .downLeft = walkDownLeft, .downRight = walkDownRight},
};
```

- A walker moving diagonally shows the diagonal's animation. When it stops, it shows that animation's
  frame 0.
- `facing()` still returns one of four directions.
- A diagonal animation with no frames makes `frame` and `WalkerSprite` throw when the walker faces that
  way.

On a sheet with one row per direction in `Facing` order, the row number is the `Facing` value:

```cpp
const std::size_t row = static_cast<std::size_t>(TopDownMovement::Facing::UpLeft);   // 7
```

## Turn to face something

`face` turns a walker without moving it:

```cpp
guard.face(retropp::Dir::Down);
knight.face(TopDownMovement::Facing::UpLeft);   // a diagonal, for sprites = Ways::Eight
```

To face a position, pick the axis with the larger distance:

```cpp
retropp::Dir toward(retropp::Vec2i from, retropp::Vec2i to) {
    const int dx = to.x - from.x;
    const int dy = to.y - from.y;
    if (std::abs(dx) >= std::abs(dy)) {
        return dx < 0 ? retropp::Dir::Left : retropp::Dir::Right;
    }
    return dy < 0 ? retropp::Dir::Up : retropp::Dir::Down;
}

guard.face(toward(guard.position(), hero.position()));
```

`face` does not change the walk animation's place. A walker that is walked turns again on its next tick
with a heading.

## Pin something to a walker

Give the config a table of named points in the art, then read a point in world pixels from the walker:

```cpp
constexpr std::array<retropp::Anchor, 1> kHeroAnchors{{{.label = "hand", .x = 14, .y = 6}}};

const TopDownMovement::WalkerConfig heroConfig{
    .collision = retropp::IntRect{.x = 3, .y = 8, .width = 10, .height = 8},
    .anchors = kHeroAnchors,
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight},
};

const retropp::Point hand = hero.anchor(heroConfig, "hand");   // the walker's position + (14, 6)
```

When drawing, read the same point from the sprite:

```cpp
const retropp::Sprite body = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
const retropp::Point drawnHand = body.anchor("hand", retropp::Space::Layer);
```

The anchor table must outlive the config. `anchor` throws `std::out_of_range` for a label that is not in
the table. The engine's `docs/guide/anchors-and-articulation.md` covers anchors on sprites.

## Keep a walker and its position in one value

A walker is bound to its position by address, so a plain copy of the pair leaves the copied walker bound
to the original position. A struct that holds both and binds again when copied fixes this:

```cpp
struct Player : TopDownMovement::InputWalker {
    retropp::Vec2i at;

    Player(retropp::Vec2i start = {}) : TopDownMovement::InputWalker(at), at(start) {}
    Player(const Player& other) : TopDownMovement::InputWalker(other), at(other.at) { bind(at); }
    Player& operator=(const Player& other) {
        TopDownMovement::InputWalker::operator=(other);
        at = other.at;
        bind(at);
        return *this;
    }
};
```

A game state holding a `Player` can be copied, returned and stored as a value. Each copy walks its own
position.

- The struct is safe in a `std::vector`. When the vector grows it copies the elements, and each copy
  binds to its own `at`.
- The base class is constructed before `at`. That is safe here: binding stores the address and does not
  read the position.
- For a walker that is not steered by input, derive from `TopDownMovement::Walker` instead.
