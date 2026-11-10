# Features

One row per feature. A feature's document, when it has one, is `docs/features/<name>.md`, named as in
the first column. Status changes are logged in `docs/features/CHANGELOG.md`.

**Status:** ⬜ planned · 🟡 in progress · 🧪 testing · ✅ complete · ❌ dropped

| Feature | Status | What it is |
|---|---|---|
| build-system | ✅ complete | CMake with `add_subdirectory(engine)`; `zorlduh-lib`, the `zorlduh` executable, one GoogleTest executable per test file; a Release-by-default, dead-stripped, symbol-stripped binary that opens a 60 Hz Game Boy Color window |
| layer-kit | ⬜ planned | The return types, `LayerOptions`, `SpriteLayer` and `TileLayer`, in `modules/layers/` |
| assets | ⬜ planned | Generated placeholder art and the struct that loads every sheet, palette and map |
| world | ⬜ planned | The world map, its collision grid and room table, on screen as one tile layer |
| player | ⬜ planned | Eight-direction movement, tile collision that slides along walls, four facings, walk animation |
| rooms-and-camera | ⬜ planned | The camera that follows the player within a room, and the scroll between rooms |
| actors | ⬜ planned | Stable actor ids, per-room spawn tables, one wandering enemy |
| combat | ⬜ planned | The sword, enemy damage and death, player damage, invulnerability, death and restart |
| hud-and-pickup | ⬜ planned | The hearts on the HUD and the heart pickup |
| sound-cues | ⬜ planned | One sound per game event, from the event stream |
