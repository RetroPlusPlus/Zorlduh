# How-to

Recipes for the questions a game asks once it has more than one room and more than one walker.

## Contents

- [Switch maps](#switch-maps)
- [Combine solids](#combine-solids)
- [Give different walkers different rules](#give-different-walkers-different-rules)
- [Walk slower than a pixel a tick](#walk-slower-than-a-pixel-a-tick)
- [Pin something to a walker](#pin-something-to-a-walker)

## Switch maps

A walker holds no map; each `walk` takes whatever solid query it is given. A game with an overworld,
dungeon floors and house interiors keeps which map the player is on in its state, and passes that map:

```cpp
walker.walk(config, move, maps[state.currentMap].solids());
```

Moving to another map is putting the walker at the entrance, in the new map's pixels, and passing the
new map from then on:

```cpp
state.currentMap = door.leadsTo;
state.player.place(door.arrival);
```

## Combine solids

One query can ask several things. A collision map, the doors that are shut, and a block the player has
pushed:

```cpp
const auto solids = [&](retropp::IntRect box) {
    return walls.solidBox(box) || doors.solidBox(box) || overlaps(box, state.block);
};
walker.walk(config, move, solids);
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
fish.walk(fishConfig, fishMove, swimmerSolids);
```

## Walk slower than a pixel a tick

`speed` is whole pixels per tick. A walker that moves a pixel every second tick calls `walk` with its
directions on even ticks and with none on odd ones:

```cpp
const TopDownMovement::DirectionalInput step = tick % 2 == 0 ? wander : TopDownMovement::DirectionalInput{};
bat.walk(batConfig, step, solids);
```

On the ticks it stands, its stride resets and it shows frame 0; a walker that should keep animating
between steps gives its walk animation frames short enough to show that.

## Pin something to a walker

Give the config the art's anchors, and read them in world pixels from the walker or in layer pixels from
its sprite:

```cpp
constexpr std::array<retropp::Anchor, 1> kHeroAnchors{{{.label = "hand", .x = 14, .y = 6}}};
const TopDownMovement::WalkerConfig hero{.anchors = kHeroAnchors, ...};

const retropp::Point hand = walker.anchor(hero, "hand");          // in the simulation
const retropp::Sprite body = TopDownMovement::WalkerSprite("hero", walker, hero);
const retropp::Point drawnHand = body.anchor("hand", retropp::Space::Layer);  // when drawing
```
