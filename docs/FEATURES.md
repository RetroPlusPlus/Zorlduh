# Features

One row per feature. A feature's document, when it has one, is `docs/features/<name>.md`, named as in
the first column. Status changes are logged in `docs/features/CHANGELOG.md`.

**Status:** ⬜ planned · 🟡 in progress · 🧪 testing · ✅ complete · ❌ dropped

| Feature | Status | What it is |
|---|---|---|
| build-system | ✅ complete | CMake with `add_subdirectory(engine)`; `zorlduh-lib`, the `zorlduh` executable, one GoogleTest executable per test file; a Release-by-default, dead-stripped, symbol-stripped binary that opens a 60 Hz Game Boy Color window |
| layer-kit | 🧪 testing | The return types (with `Raster`), `LayerOptions` and `RasterLayerOptions`, and one aggregate per kind of layer content — `SpriteLayer`, `TileLayer`, `RasterLayer`, `ImageLayer`: a header-only module in `modules/layers/` that depends on the engine alone, with its own guide (`modules/layers/docs/`) and a compiled getting-started program |
| assets | 🧪 testing | Placeholder art written by `tools/placeholder_art.py` under `assets/` — 2-bit grayscale index sheets, palette images, and viewable grayscale tile and collision maps — and `zorlduh::loadAssets`, which uploads every sheet and palette and decodes both maps, all baked into the executable |
| world | 🧪 testing | The room table (`src/game/rooms.h`); `zorlduh::World`, which answers whether a pixel or a box is solid and which room holds a point, in world pixels; the depth and layer-key table and the screen layout (`src/render/layers.h`, `layout.h`); and `WorldTiles`, the whole map assembled through the terrain catalog onto one finite tile layer, showing the start room under the HUD strip |
| player | ⬜ planned | Eight-direction movement, tile collision that slides along walls, four facings, walk animation |
| rooms-and-camera | ⬜ planned | The camera that follows the player within a room, and the scroll between rooms |
| actors | ⬜ planned | Stable actor ids, per-room spawn tables, one wandering enemy |
| combat | ⬜ planned | The sword, enemy damage and death, player damage, invulnerability, death and restart |
| hud-and-pickup | ⬜ planned | The hearts on the HUD and the heart pickup |
| sound-cues | ⬜ planned | One sound per game event, from the event stream |
