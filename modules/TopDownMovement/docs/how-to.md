# How-to

Recipes for the questions a game asks once it has more than one room and more than one walker.

## Contents

- [Switch maps](#switch-maps)
- [Combine solids](#combine-solids)
- [Give different walkers different rules](#give-different-walkers-different-rules)
- [Walk slower than a pixel a tick](#walk-slower-than-a-pixel-a-tick)
- [Pin something to a walker](#pin-something-to-a-walker)
- [Keep a walker and its position in one value](#keep-a-walker-and-its-position-in-one-value)
- [Patrol a route](#patrol-a-route)
- [Break off a route and come back](#break-off-a-route-and-come-back)
- [Wander against walls](#wander-against-walls)
- [Move in four directions only](#move-in-four-directions-only)
- [Give the diagonals their own sprites](#give-the-diagonals-their-own-sprites)
- [Turn to face something](#turn-to-face-something)

## Switch maps

A walker holds no map; each `walk` takes whatever solid query it is given. A game with an overworld,
dungeon floors and house interiors keeps which map the player is on in its state, and passes that map:

```cpp
hero.walk(heroConfig, move, maps[state.currentMap].solids());
```

Moving to another map is putting the walker at the entrance, in the new map's pixels, and passing the
new map from then on:

```cpp
state.currentMap = door.leadsTo;
hero.place(door.arrival);
```

## Combine solids

One query can ask several things. A collision map, the doors that are shut, and a block the player has
pushed:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return walls.solidBox(box) || doors.solidBox(box) || overlaps(box, state.block);
};
hero.walk(heroConfig, move, solids);
```

Every collision shape works against the combination, because the walker only ever asks it about
rectangles.

## Give different walkers different rules

The query is passed per walker, so two walkers on one map can see it differently. A swimmer crosses
water that stops everyone else:

```cpp
const auto landSolids = [&](retropp::IntRect box) { return walls.solidBox(box) || water.solidBox(box); };
const auto swimmerSolids = [&](retropp::IntRect box) { return walls.solidBox(box); };

hero.walk(heroConfig, move, landSolids);
fish.walk(fishConfig, fishHeading, swimmerSolids);
```

## Walk slower than a pixel a tick

`speed` is whole pixels per tick. A walker that goes slower takes a route, whose pace is in pixels per
second and can be any fraction of a pixel a tick:

```cpp
retropp::SpritePath stroll = TopDownMovement::routeFrom(ghost, {
    {.move = retropp::SpritePathMove::to({120.0f, 40.0f}), .pacing = retropp::PathPacing::speed(20.0f)},
});
TopDownMovement::follow(ghost, ghostConfig, stroll, retropp::PlaybackMode::single());   // each tick
```

The walker walks on every tick the route moves, so its walk animation plays at its own pace however slow
the route is ([routes.md](routes.md#follow)). A route does not collide; a slow walker that must be stopped
by walls walks its heading on some ticks and heads nowhere on the others:

```cpp
bat.walk(batConfig, tick % 2 == 0 ? batHeading : TopDownMovement::Directions{}, solids);
```

On the ticks it heads nowhere its stride resets and it shows frame 0.

## Pin something to a walker

Give the config the art's anchors, and read them in world pixels from the walker or in layer pixels from
its sprite:

```cpp
constexpr std::array<retropp::Anchor, 1> kHeroAnchors{{{.label = "hand", .x = 14, .y = 6}}};
const TopDownMovement::WalkerConfig heroConfig{.anchors = kHeroAnchors, ...};

const retropp::Point hand = hero.anchor(heroConfig, "hand");          // in the simulation
const retropp::Sprite body = TopDownMovement::WalkerSprite("hero", hero, heroConfig);
const retropp::Point drawnHand = body.anchor("hand", retropp::Space::Layer);  // when drawing
```

## Keep a walker and its position in one value

A walker is bound to a position by address, so whatever holds both and is copied binds the copy to its
own position. A struct that is a walker and holds the position it walks does that in its copy
constructor and its copy assignment:

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

The base is bound to `at` before `at` has its value; nothing reads the position until the constructor
has run. A game state holding a `Player` then copies, returns and saves as a plain value, each copy
walking its own position.

## Patrol a route

A patrol is a route that loops, with a wait node where the walker stops:

```cpp
retropp::SpritePath patrol = TopDownMovement::routeFrom(guard, {
    {.label = "east", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::speed(30.0f)},
    {.label = "wait", .move = retropp::SpritePathMove::to({136.0f, 120.0f}), .pacing = retropp::PathPacing::eased(500ms)},
    {.label = "west", .move = retropp::SpritePathMove::to({16.0f, 120.0f}),  .pacing = retropp::PathPacing::speed(30.0f)},
});

TopDownMovement::follow(guard, guardConfig, patrol);   // each tick; the default mode loops
```

`patrol.currentNode()->label` says which leg the guard is on, for anything the game does at a corner. A
curved leg is `SpritePathMove::through(points)` or `hermite(…)` (the engine's
`docs/guide/sprite-path.md`).

## Break off a route and come back

Interrupt the route with a detour; the walker walks it on the next `follow`, and the route takes up again
when it ends:

```cpp
patrol.interrupt({{.move = retropp::SpritePathMove::to(noise), .pacing = retropp::PathPacing::speed(60.0f)}},
                 retropp::PlaybackMode::single(), retropp::ResumePolicy::Return);
```

`ResumePolicy::Return` puts the walker back where it broke off; `Continue` carries the route on from
where the detour left it ([routes.md](routes.md#detours)). A detour that should last until the game says
so is played under `loopIndefinitely()` and ended with `patrol.popInterrupt()`.

## Wander against walls

A walker that wanders, chases or flees is stopped by walls, so it takes the free pathway: a heading of the
game's own choosing, walked against the solids. `moving()` says whether the heading got it anywhere:

```cpp
const std::array<TopDownMovement::Directions, 4> kClockwise{{
    {.right = true}, {.down = true}, {.left = true}, {.up = true},
}};

wanderer.walk(wandererConfig, kClockwise[heading], walls);
if (!wanderer.moving()) {
    heading = (heading + 1) % kClockwise.size();   // blocked: turn
}
```

A chase is a heading toward the target, worked out each tick from the two positions:

```cpp
const retropp::Vec2i me = bat.position();
bat.walk(batConfig, {.up = target.y < me.y, .down = target.y > me.y, .left = target.x < me.x, .right = target.x > me.x},
         solids);
```

## Move in four directions only

Set the config's `movement`:

```cpp
const TopDownMovement::WalkerConfig heroConfig{.movement = TopDownMovement::Ways::Four, ...};
```

Held on two axes, an input walker moves along the one pressed more recently and faces that way; letting
go of it moves the walker along the other. A heading walker given a diagonal moves along the axis it
faces, or the horizontal one. Routes are not held to it: a route goes where it is drawn.

## Give the diagonals their own sprites

Set the config's `sprites` and give the four diagonals animations:

```cpp
const TopDownMovement::WalkerConfig knightConfig{
    .sprites = TopDownMovement::Ways::Eight,
    .walk = {.up = walkUp, .down = walkDown, .left = walkLeft, .right = walkRight,
             .upLeft = walkUpLeft, .upRight = walkUpRight, .downLeft = walkDownLeft, .downRight = walkDownRight},
};
```

A diagonal heading shows its diagonal, and a walker that stops after one stands on that diagonal's frame
0. The walker's own facing stays one of the four, for everything that asks which way it faces. A sheet
with a row per direction in `Facing`'s order builds the eight animations from
`static_cast<std::size_t>(facing)`.

## Turn to face something

`face` turns a walker without moving it:

```cpp
guard.face(retropp::Dir::Down);                 // toward the player below him
knight.face(TopDownMovement::Facing::UpLeft);   // the sprite only, under sprites = Ways::Eight
```

The stride keeps its place, so a walker turned while walking carries on its step in the new direction.
A walker the pad steers turns again the next time a direction is held.
