# Features

One row per feature. A feature's document, when it has one, is `docs/features/<name>.md`, named as in
the first column. Status changes are logged in `docs/features/CHANGELOG.md`.

**Status:** ⬜ planned · 🟡 in progress · 🧪 testing · ✅ complete · ❌ dropped

| Feature | Status | What it is |
|---|---|---|
| build-system | ✅ complete | CMake with `add_subdirectory(engine)`; `zorlduh-lib`, the `zorlduh` executable, one GoogleTest executable per test file; a Release-by-default, dead-stripped, symbol-stripped binary that opens a 60 Hz Game Boy Color window |
| layer-kit | 🧪 testing | The return types (with `Raster`), `LayerOptions` and `RasterLayerOptions`, and one aggregate per kind of layer content — `SpriteLayer`, `TileLayer`, `RasterLayer`, `ImageLayer`: a header-only module in `modules/LayerKit/` that depends on the engine alone, with its own guide (`modules/LayerKit/docs/`) and a compiled getting-started program |
| assets | 🧪 testing | Placeholder art written by `tools/placeholder_art.py` under `assets/` — 2-bit grayscale index sheets, palette images, and viewable grayscale tile and collision maps, one pair per area — and `zorlduh::loadAssets`, which uploads every sheet and palette and decodes every area's maps, all baked into the executable |
| world | ❌ dropped | One world map of free-form rooms with a room table, `zorlduh::World` and `WorldTiles`; replaced by one map per area in `world-kit` and `areas`. The depth table and the screen layout it added (`src/render/layers.h`, `layout.h`) stay |
| top-down-movement | 🧪 testing | `TopDownMovement` in `modules/TopDownMovement/`: a `WalkerConfig` (speed, collision shape, anchors, a walk animation per facing), a `Walker` that `walk` moves a tick — a pixel at a time per axis, stopping flush at walls and carrying on along one walked into diagonally — with the facing law and the walk animation, collision shapes from boxes, engine shapes or anchors against a `TileCollision` map or any query, `readDirections`, and `WalkerSprite`; header-only, depending on the engine alone, with its own guide and a compiled getting-started program |
| player | 🧪 testing | The player as a walker: `PlayerConfig` from the player sheet's four rows, the action map (arrows, WASD, d-pad; attack, item, start), `GameState`, `Config` and `tick`, and the `Player` component on the actors layer above the world |
| camera-kit | 🧪 testing | `CameraKit` in `modules/CameraKit/`: a `View` (the screen the camera shows and where it sits on a layer), a `Camera` value with `place`, a straight `pan` over N ticks through the engine's tween, and `scroll` for a view, and the `follow` law — center the focus, stop at the bounds' edges, center bounds smaller than the view; header-only, depending on the engine alone, with its own guide and a compiled getting-started program |
| world-kit | 🧪 testing | `WorldKit` in `modules/WorldKit/`: a world of `Area`s — each its own tile map, art catalog, collision map and named `Opening`s in walls or at spots, each opening with its `Transition` and where it `leadsTo` — checked by `validate`; a `Camera` that hands out the area's solid query, follows a box by the layout's camera law, scrolls the next area in lined up with the gap or fades to the entrance, and hands back the move to apply; `WorldLayers` (one tile layer per area shown) and `Fade`; header-only on the engine, LayerKit and CameraKit, with its own guide and a compiled getting-started program |
| areas | 🧪 testing | The game's five areas (meadow, lake, field, shrine, cave), each its own map pair, joined by scrolling doors and a fading cave mouth (`src/game/areas.{h,cpp}`); the player goes between them through the tick, which records `AreaChanged`; the screen is the areas on screen, the actors and the fade |
| actors | ⬜ planned | Stable actor ids, per-area spawn tables, one wandering enemy |
| combat | ⬜ planned | The sword, enemy damage and death, player damage, invulnerability, death and restart |
| hud-and-pickup | ⬜ planned | The hearts on the HUD and the heart pickup |
| sound-cues | ⬜ planned | One sound per game event, from the event stream |
