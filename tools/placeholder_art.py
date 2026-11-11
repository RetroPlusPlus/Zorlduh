#!/usr/bin/env python3
"""Writes Zorlduh's placeholder art under assets/: every sheet, every palette, the world map and its
collision map.

    assets/gfx/tiles/terrain.png                  the terrain tiles, one per world id
    assets/gfx/sprites/{player,enemy,sword}.png   the sprite sheets
    assets/gfx/hud/hearts.png                     the HUD's hearts
    assets/palettes/tiles/{floor,wall,water,void}.png
    assets/palettes/sprites/{player,enemy,sword}.png
    assets/palettes/hud/hearts.png
    assets/maps/world/tiles.png                   one world id per cell
    assets/maps/world/collision.png               one collision id per cell

A sheet is atlas art: a 2-bit grayscale PNG whose samples are palette indices 0-3, so an image viewer
shows its four indices as black, dark gray, light gray and white. It carries no color. Color comes
from a palette image chosen when a tile or sprite is drawn: a 4×1 truecolor RGBA PNG, one pixel per
palette entry, dark at entry 0 to light at entry 3 like the gray levels.

A map is an 8-bit grayscale PNG whose sample value is a cell's id: a world id (WorldCell in
src/assets.h) or a collision id (CollisionCell). The ids are spread across the sample range so a map
reads by eye in an image viewer: void is black, and floor and open cells are light.

The output is deterministic. The zlib stream is written as stored (uncompressed) blocks, so the bytes
do not depend on the zlib build, and running the script again writes identical files.

    python3 tools/placeholder_art.py            write every file under assets/
    python3 tools/placeholder_art.py --check    compare the committed files with what the script
                                                writes; exit 1 and name each one that differs

Standard library only.
"""

import struct
import sys
import zlib
from pathlib import Path

ASSET_DIR = Path(__file__).resolve().parent.parent / "assets"

# ── PNG writing ──────────────────────────────────────────────────────────────────────────────────


def _chunk(kind, data):
    body = kind + data
    return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)


def _stored_zlib(data):
    """A zlib stream holding `data` in stored deflate blocks: header, blocks, Adler-32."""
    out = bytearray(b"\x78\x01")
    block = 65535
    for start in range(0, max(len(data), 1), block):
        piece = data[start:start + block]
        final = 1 if start + block >= len(data) else 0
        out += struct.pack("<BHH", final, len(piece), len(piece) ^ 0xFFFF)
        out += piece
    out += struct.pack(">I", zlib.adler32(data) & 0xFFFFFFFF)
    return bytes(out)


def _png(width, height, bit_depth, color_type, scanlines):
    header = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    raw = b"".join(b"\x00" + line for line in scanlines)  # filter type None on every row
    return (b"\x89PNG\r\n\x1a\n" + _chunk(b"IHDR", header)
            + _chunk(b"IDAT", _stored_zlib(raw)) + _chunk(b"IEND", b""))


def _rectangular(rows):
    width = len(rows[0])
    assert all(len(row) == width for row in rows), "every row of an image is the same width"
    return width


def sheet_png(rows):
    """A 2-bit grayscale PNG whose samples are `rows`, palette indices 0-3. Each row packs four
    samples to a byte, the first in the high bits, and pads its last byte with zeros."""
    width = _rectangular(rows)
    assert all(0 <= index <= 3 for row in rows for index in row), "a sheet holds indices 0-3"
    scanlines = []
    for row in rows:
        padded = row + [0] * (-len(row) % 4)
        scanlines.append(bytes((padded[i] << 6) | (padded[i + 1] << 4) | (padded[i + 2] << 2) | padded[i + 3]
                               for i in range(0, len(padded), 4)))
    return _png(width, len(rows), 2, 0, scanlines)


def map_png(rows):
    """An 8-bit grayscale PNG whose samples are `rows`, one cell id each."""
    width = _rectangular(rows)
    return _png(width, len(rows), 8, 0, [bytes(row) for row in rows])


def palette_png(palette):
    """A palette image: one truecolor RGBA pixel per entry, in a single row."""
    return _png(len(palette), 1, 8, 6, [b"".join(struct.pack("BBBB", *entry) for entry in palette)])


# ── Palettes ─────────────────────────────────────────────────────────────────────────────────────
#
# Four (r, g, b, a) entries each, dark at index 0 to light at index 3, the same order as the gray
# levels a sheet shows in an image viewer. A sprite palette's entry 0 is the sheet's transparent index;
# it is never drawn, and its alpha is 0.

UNUSED = (0, 0, 0, 0)

FLOOR_PAL = [(88, 72, 40, 255), (160, 136, 80, 255), (208, 184, 120, 255), (232, 216, 160, 255)]
WALL_PAL = [(32, 32, 48, 255), (88, 88, 112, 255), (144, 144, 168, 255), (208, 208, 216, 255)]
WATER_PAL = [(16, 48, 88, 255), (40, 104, 176, 255), (88, 168, 224, 255), (176, 224, 248, 255)]
VOID_PAL = [(0, 0, 0, 255)] * 4

PLAYER_PAL = [UNUSED, (40, 24, 32, 255), (200, 80, 56, 255), (248, 224, 192, 255)]   # outline, cloak, face
ENEMY_PAL = [UNUSED, (32, 8, 48, 255), (128, 64, 192, 255), (240, 240, 160, 255)]    # outline, body, eyes
SWORD_PAL = [UNUSED, (64, 48, 40, 255), (160, 168, 184, 255), (255, 255, 255, 255)]  # grip, edge, blade
HEARTS_PAL = [UNUSED, (48, 16, 24, 255), (224, 48, 64, 255), (255, 255, 255, 255)]   # outline, heart, shine

# ── Pixel art ────────────────────────────────────────────────────────────────────────────────────
#
# Art is drawn as strings, one character per pixel: '.' or '0' is index 0 and '1'-'3' are indices
# 1-3, dark to light. On a sprite sheet index 0 is the transparent index, so a sprite is drawn in
# 1 (its outline), 2 and 3 (its lightest parts).


def parse(art, width, height):
    rows = [[0 if ch == "." else int(ch) for ch in line] for line in art]
    assert len(rows) == height, f"art is {height} rows tall"
    assert all(len(row) == width for row in rows), f"art is {width} pixels wide"
    assert all(0 <= index <= 3 for row in rows for index in row), "art uses indices 0-3"
    return rows


def mirrored(rows):
    return [list(reversed(row)) for row in rows]


def side_by_side(*frames):
    """One row of frames, left to right, as a single image."""
    return [sum((frame[y] for frame in frames), []) for y in range(len(frames[0]))]


def stacked(*strips):
    return [row for strip in strips for row in strip]


# Terrain, one 8×8 tile per world id, in WorldCell order. Each tile is drawn through the palette of
# its own kind. The terrain sheet has no transparent index, so index 0 draws.

FLOOR = parse([
    "22222222",
    "22212222",
    "22222232",
    "22222222",
    "21222222",
    "22222222",
    "22223222",
    "22222212",
], 8, 8)

WALL = parse([
    "33303333",
    "22202222",
    "22202221",
    "00000000",
    "33333330",
    "22222220",
    "12222220",
    "00000000",
], 8, 8)

WATER = parse([
    "22222222",
    "22333222",
    "23222322",
    "22222221",
    "22222222",
    "33222233",
    "22322322",
    "21222222",
], 8, 8)

DOORWAY = parse([
    "11111111",
    "12121212",
    "11111111",
    "21212121",
    "11111111",
    "12121212",
    "11111111",
    "21212121",
], 8, 8)

VOID = parse(["00000000"] * 8, 8, 8)

TILES = side_by_side(FLOOR, WALL, WATER, DOORWAY, VOID)

# The player: a hooded traveler in a cloak. Rows Down, Up, Left, Right; two walk frames each. The
# second Down and Up frames are the first mirrored, and Right is Left mirrored.

PLAYER_DOWN = parse([
    "................",
    ".....111111.....",
    "....12222221....",
    "...1222222221...",
    "...1223333221...",
    "...1231331321...",
    "...1233333321...",
    "....12333321....",
    "...1112222111...",
    "..122221122221..",
    "..132222222231..",
    "..133222222331..",
    "...1122222211...",
    "....12222221....",
    "....111..11.....",
    "....111.........",
], 16, 16)

PLAYER_UP = parse([
    "................",
    ".....111111.....",
    "....12222221....",
    "...1222222221...",
    "...1222222221...",
    "...1222222221...",
    "...1122222211...",
    "....11222211....",
    "...1112222111...",
    "..122222222221..",
    "..132222222231..",
    "..133222222331..",
    "...1122222211...",
    "....12222221....",
    "....111..11.....",
    "....111.........",
], 16, 16)

PLAYER_LEFT_A = parse([
    "................",
    "......111111....",
    ".....12222221...",
    "....1222222221..",
    "...13332222221..",
    "...13133222221..",
    "...13333222221..",
    "....1333322221..",
    "....1112222211..",
    "...12222222221..",
    "...13222222221..",
    "...13322222221..",
    "....1122222211..",
    ".....12222221...",
    ".....111..111...",
    "..........111...",
], 16, 16)

PLAYER_LEFT_B = parse([
    "................",
    "......111111....",
    ".....12222221...",
    "....1222222221..",
    "...13332222221..",
    "...13133222221..",
    "...13333222221..",
    "....1333322221..",
    "....1112222211..",
    "...12222222221..",
    "...13222222221..",
    "...13322222221..",
    "....1122222211..",
    ".....12222221...",
    ".....111..111...",
    ".....111........",
], 16, 16)

PLAYER = stacked(
    side_by_side(PLAYER_DOWN, mirrored(PLAYER_DOWN)),
    side_by_side(PLAYER_UP, mirrored(PLAYER_UP)),
    side_by_side(PLAYER_LEFT_A, PLAYER_LEFT_B),
    side_by_side(mirrored(PLAYER_LEFT_A), mirrored(PLAYER_LEFT_B)),
)

# The enemy: a horned blob, two frames, the second squashed. Both rest on the same bottom row.

ENEMY_A = parse([
    "................",
    "................",
    "......1..1......",
    ".....121121.....",
    "....12222221....",
    "...1222222221...",
    "..122332233221..",
    "..123132231321..",
    "..122332233221..",
    "..122222222221..",
    "..122211112221..",
    "..122222222221..",
    "...1222222221...",
    "....11222211....",
    "......1111......",
    "................",
], 16, 16)

ENEMY_B = parse([
    "................",
    "................",
    "................",
    "................",
    "......1..1......",
    ".....121121.....",
    "...1122222211...",
    "..122332233221..",
    ".12231322313221.",
    ".12223322332221.",
    ".12222222222221.",
    ".12222111122221.",
    "..122222222221..",
    "...1122222211...",
    ".....111111.....",
    "................",
], 16, 16)

ENEMY = side_by_side(ENEMY_A, ENEMY_B)

# The sword, pointing up: blade, crossguard, grip, pommel.

SWORD = parse([
    "...11...",
    "..1331..",
    "..1321..",
    "..1321..",
    "..1321..",
    "..1321..",
    "..1321..",
    "..1321..",
    "..1321..",
    "..1321..",
    ".111111.",
    "12222221",
    ".111111.",
    "...11...",
    "...11...",
    "..1221..",
], 8, 16)

# The HUD's hearts: full, half, empty. An empty heart's inside is transparent.

HEART_FULL = parse([
    "........",
    ".11.11..",
    "1321221.",
    "1222221.",
    ".12221..",
    "..121...",
    "...1....",
    "........",
], 8, 8)

HEART_HALF = parse([
    "........",
    ".11.11..",
    "1321..1.",
    "1222..1.",
    ".122.1..",
    "..121...",
    "...1....",
    "........",
], 8, 8)

HEART_EMPTY = parse([
    "........",
    ".11.11..",
    "1..1..1.",
    "1.....1.",
    ".1...1..",
    "..1.1...",
    "...1....",
    "........",
], 8, 8)

HUD = side_by_side(HEART_FULL, HEART_HALF, HEART_EMPTY)

# ── The world ────────────────────────────────────────────────────────────────────────────────────
#
# A 60×32-cell map, one pixel per cell. Rooms are rectangles (x, y, w, h) in cells; each is ringed
# by wall except its openings, and a cell outside every room is void.

WORLD_WIDTH = 60
WORLD_HEIGHT = 32

# World ids, as WorldCell in src/assets.h, spread from black (void) to white (floor).
VOID_ID, WALL_ID, WATER_ID, DOORWAY_ID, FLOOR_ID = 0, 64, 128, 192, 255

# Collision ids, as CollisionCell in src/assets.h: solid is black, open is white.
SOLID_ID, OPEN_ID = 0, 255

ROOMS = [
    (0, 0, 20, 16),    # room 0: exactly one play area; the start room
    (20, 0, 40, 16),   # room 1: two play areas wide
    (0, 16, 30, 16),   # room 2: one and a half play areas wide
    (30, 16, 12, 10),  # room 3: smaller than the play area on both axes
]

# The openings, as the doorway cells on both sides of each shared edge.
OPENINGS = (
    [(x, y) for x in (19, 20) for y in range(6, 10)]       # room 0 ↔ room 1
    + [(x, y) for x in range(8, 12) for y in (15, 16)]     # room 0 ↔ room 2
    + [(x, y) for x in (29, 30) for y in range(19, 23)]    # room 2 ↔ room 3
)

# The pond in room 1's interior: rows of (first column, last column), from its top row down.
POND_TOP = 5
POND_ROWS = [(38, 41), (37, 42), (36, 43), (36, 43), (37, 42), (38, 41)]


def world_cells():
    cells = [[VOID_ID] * WORLD_WIDTH for _ in range(WORLD_HEIGHT)]
    for (rx, ry, rw, rh) in ROOMS:
        for y in range(ry, ry + rh):
            for x in range(rx, rx + rw):
                border = x in (rx, rx + rw - 1) or y in (ry, ry + rh - 1)
                cells[y][x] = WALL_ID if border else FLOOR_ID
    for (x, y) in OPENINGS:
        cells[y][x] = DOORWAY_ID
    for row, (first, last) in enumerate(POND_ROWS):
        for x in range(first, last + 1):
            cells[POND_TOP + row][x] = WATER_ID
    return cells


def collision_cells(world):
    solid = {WALL_ID, WATER_ID, VOID_ID}
    return [[SOLID_ID if cell in solid else OPEN_ID for cell in row] for row in world]


# ── Output ───────────────────────────────────────────────────────────────────────────────────────


def files():
    """Every file the script writes, by path under assets/."""
    world = world_cells()
    return {
        "gfx/tiles/terrain.png": sheet_png(TILES),
        "gfx/sprites/player.png": sheet_png(PLAYER),
        "gfx/sprites/enemy.png": sheet_png(ENEMY),
        "gfx/sprites/sword.png": sheet_png(SWORD),
        "gfx/hud/hearts.png": sheet_png(HUD),
        "palettes/tiles/floor.png": palette_png(FLOOR_PAL),
        "palettes/tiles/wall.png": palette_png(WALL_PAL),
        "palettes/tiles/water.png": palette_png(WATER_PAL),
        "palettes/tiles/void.png": palette_png(VOID_PAL),
        "palettes/sprites/player.png": palette_png(PLAYER_PAL),
        "palettes/sprites/enemy.png": palette_png(ENEMY_PAL),
        "palettes/sprites/sword.png": palette_png(SWORD_PAL),
        "palettes/hud/hearts.png": palette_png(HEARTS_PAL),
        "maps/world/tiles.png": map_png(world),
        "maps/world/collision.png": map_png(collision_cells(world)),
    }


def main(argv):
    check = "--check" in argv[1:]
    differing = []
    for name, data in files().items():
        path = ASSET_DIR / name
        if check:
            if not path.is_file() or path.read_bytes() != data:
                differing.append(name)
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(data)
    if differing:
        print("differs from what tools/placeholder_art.py writes: " + ", ".join(differing))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
