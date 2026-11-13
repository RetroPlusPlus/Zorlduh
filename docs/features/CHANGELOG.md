# Feature changelog

One row per feature status change, newest last.

| Date | Feature | From | To | Commit | Note |
|---|---|---|---|---|---|
| 2026-11-10 | build-system | — | ✅ complete | `4f171b6` | The build, the executable that opens a window, and the smoke tests |
| 2026-11-10 | layer-kit | ⬜ planned | 🧪 testing | `426b699` | `SpriteLayer`, `TileLayer`, `RasterLayer`, `ImageLayer`, the two option structs and the return types in `modules/layers/`, with 25 tests, its own guide and a getting-started program |
| 2026-11-10 | assets | ⬜ planned | 🧪 testing | `d6de719` | `tools/placeholder_art.py` and its 15 files under `assets/` (2-bit grayscale index sheets, palette images, grayscale tile and collision maps), `zorlduh::loadAssets`, every asset baked into the executable; 14 tests plus a check that the committed art matches the script |
| 2026-11-10 | world | ⬜ planned | 🧪 testing | `0916fd8` | The room table, `World`'s solid and room queries, the depth and layout tables, and `WorldTiles` on one finite tile layer; the start room is on screen; 14 tests |
| 2026-11-10 | top-down-movement | — | 🧪 testing | `474e395` | `TopDownMovement` in `modules/TopDownMovement/` — walkers, collision shapes, tile collision, the facing law, walk animations, the walker's sprite — with its guide and example; the layer kit's module is renamed `LayerKit`; 30 tests |
| 2026-11-10 | player | ⬜ planned | 🧪 testing | `474e395` | The player walks the world through TopDownMovement, drawn on the actors layer; 8 tests |
| 2026-11-11 | camera-kit | — | 🧪 testing | `487263a` | `CameraKit` in `modules/CameraKit/`: the camera value, the follow law, pans; with its guide and example; 10 tests |
| 2026-11-11 | world-kit | — | 🧪 testing | `487263a` | `WorldKit` in `modules/WorldKit/`: areas with their own maps and art, openings that scroll or fade, the camera within an area, the areas on screen; with its guide and example; 27 tests |
| 2026-11-11 | world | 🧪 testing | ❌ dropped | `487263a` | The single world map, room table and `World` are replaced by one map per area in `world-kit` and `areas` |
| 2026-11-11 | areas | — | 🧪 testing | `487263a` | The game's five areas, joined by scrolling doors and a fading cave mouth; the player goes between them through the tick; 8 tests (`assets` and `player` reworked: 13 and 8) |
| 2026-11-11 | world-kit | 🧪 testing | 🧪 testing | `fa745b7` | A `World` of layouts; openings lead into any layout, with an optional cell offset; the seamless transition; the camera follows only as the box moves, slides along a door's axis, holds still in an area smaller than the view; the world confined to the view; 43 tests |
| 2026-11-11 | dungeon-one | — | 🧪 testing | `fa745b7` | The dungeon layout of five rooms, down from the lake, with its doors and the hall–treasury seam; the overworld's maps under `maps/overworld/`; 6 new tests in `areas` (14 now; `assets` reworked: 14) |
| 2026-11-11 | world-kit | 🧪 testing | 🧪 testing | `b9537fa` | Areas drawn through chains of seams, as deep as the layout's `seamDepth` (0, the default, for any number); the camera's law and the solid query stay one seam; a guide recipe for putting a game's things in an area; 47 tests |
| 2026-11-11 | seamless-forest | — | 🧪 testing | `b9537fa` | The forest layout of seven rooms joined only by seams, with its own sheet and palettes, down from the field; 5 new tests in `areas` (19 now) and 2 in `assets` (16 now) |
| 2026-11-11 | world-kit | 🧪 testing | 🧪 testing | `4434f17` | An area is its map layers, each at its own depth, and its named actor slots; `WorldLayers` draws every map layer, `Actors` the game's sprites at the current area's slot, `TopZ` the world's highest depth; `validate` checks layers and slots; 55 tests |
| 2026-11-11 | areas | 🧪 testing | 🧪 testing | `4434f17` | Every area a ground layer and the `"actors"` slot at depths handed in from the game's depth table; rock over the meadow's cave mouth; 2 new tests in `areas` (21 now) and 1 in `player` (9 now) |
| 2026-11-11 | seamless-forest | 🧪 testing | 🧪 testing | `4434f17` | A path under the player's feet in the edge and a band of canopy over the player in the glade |
| 2026-11-11 | assets | 🧪 testing | 🧪 testing | `4434f17` | The overlay sheet (index 0 transparent), the canopy palette and three layer maps; 54 files; 2 new tests (18 now) |
| 2026-11-11 | text-kit | — | 🧪 testing | `3af6c87` | `TextKit` in `modules/TextKit/`: fonts from glyph sheets with the case rule, text as sprites keyed by position, wrapping, measuring, alignment, the shadow and the reveal; with its guide and an example over its own font sheet and palettes; 13 tests |
| 2026-11-11 | area-name | — | 🧪 testing | `3af6c87` | The current area's name in the HUD strip, right-aligned and typed out again on entering each area; 4 tests |
| 2026-11-11 | assets | 🧪 testing | 🧪 testing | `3af6c87` | The glyph sheet of 43 characters and the text and shadow palettes; 57 files; 3 new tests (21 now) |
| 2026-11-11 | window-kit | — | 🧪 testing | `4d92478` | `WindowKit` in `modules/WindowKit/`: borders, boxes of any size with cut pieces, windows as one layer, and the message box; with its guide and an example over its own border, font and palettes; 15 tests |
| 2026-11-11 | signs | — | 🧪 testing | `4d92478` | The meadow's wooden sign and the lake's stone rock, read with Item, each message in its own border while the world holds still; 6 tests |
| 2026-11-11 | assets | 🧪 testing | 🧪 testing | `4d92478` | The wood and stone border sheets and palettes, the sign and rock tiles and their cells in the meadow and lake; 61 files (21 tests) |
| 2026-11-11 | top-down-movement | 🧪 testing | 🧪 testing | `4d92478` | Facing follows the directions held whether or not the walker moves, so a walker pushing into a wall turns to face it; 31 tests |
| 2026-11-13 | window-animation | — | 🧪 testing | `1f15898` | `Animation`, the five styles (grow, clip, squash, fade, slide), `Style` and `Animate`, a `Window` that takes an animation and draws nothing on the opening's first tick, a `Dialog` that runs its own box's animation; with a guide page and the example cycling the styles; 23 tests, one on a GPU device |
| 2026-11-13 | window-kit | 🧪 testing | 🧪 testing | `1f15898` | Every piece of a box keyed by the box's size; a confirm finishes whatever a message is doing, the box opening and closing included; `close` plays the closing; 15 tests |
| 2026-11-13 | signs | 🧪 testing | 🧪 testing | `1f15898` | Message boxes open and close over 300 ms; six signs on a grid in the shrine, each opening its box in one of the styles; 6 tests, and 5 more on the animation |
| 2026-11-13 | assets | 🧪 testing | 🧪 testing | `1f15898` | The shrine's six sign cells on its maps; 61 files (21 tests) |
