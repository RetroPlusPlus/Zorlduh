# Feature changelog

One row per feature status change, newest last.

| Date | Feature | From | To | Commit | Note |
|---|---|---|---|---|---|
| 2026-11-10 | build-system | — | ✅ complete | `4f171b6` | The build, the executable that opens a window, and the smoke tests |
| 2026-11-10 | layer-kit | ⬜ planned | 🧪 testing | `426b699` | `SpriteLayer`, `TileLayer`, `RasterLayer`, `ImageLayer`, the two option structs and the return types in `modules/layers/`, with 25 tests, its own guide and a getting-started program |
| 2026-11-10 | assets | ⬜ planned | 🧪 testing | `d6de719` | `tools/placeholder_art.py` and its 15 files under `assets/` (2-bit grayscale index sheets, palette images, grayscale tile and collision maps), `zorlduh::loadAssets`, every asset baked into the executable; 14 tests plus a check that the committed art matches the script |
| 2026-11-10 | world | ⬜ planned | 🧪 testing | `0916fd8` | The room table, `World`'s solid and room queries, the depth and layout tables, and `WorldTiles` on one finite tile layer; the start room is on screen; 14 tests |
