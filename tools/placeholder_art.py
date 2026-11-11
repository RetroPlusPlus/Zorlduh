#!/usr/bin/env python3
"""Writes Zorlduh's placeholder art under assets/: every sheet, every palette, and each area's tile map
and collision map.

    assets/gfx/tiles/terrain.png                  the terrain tiles, one per world id
    assets/gfx/sprites/{player,enemy,sword}.png   the sprite sheets
    assets/gfx/hud/hearts.png                     the HUD's hearts
    assets/palettes/tiles/{floor,wall,water}.png
    assets/palettes/sprites/{player,enemy,sword}.png
    assets/palettes/hud/hearts.png
    assets/maps/<layout>/<area>/tiles.png         one world id per cell, for each area of each layout
    assets/maps/<layout>/<area>/collision.png     one collision id per cell, for each area of each layout

A sheet is atlas art: a 2-bit grayscale PNG whose samples are palette indices 0-3, so an image viewer
shows its four indices as black, dark gray, light gray and white. It carries no color. Color comes
from a palette image chosen when a tile or sprite is drawn: a 4×1 truecolor RGBA PNG, one pixel per
palette entry, dark at entry 0 to light at entry 3 like the gray levels.

A map is an 8-bit grayscale PNG whose sample value is a cell's id: a world id (WorldCell in
src/assets/assets.h) or a collision id (CollisionCell). The ids are spread across the sample range so
a map reads by eye in an image viewer: stairs are near black, and floor and open cells are light. Each
area is its own pair of maps; nothing outside an area exists.

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

STAIRS = parse([
    "11111111",
    "10000001",
    "11111111",
    "10000001",
    "11111111",
    "10000001",
    "11111111",
    "10000001",
], 8, 8)

TILES = side_by_side(FLOOR, WALL, WATER, DOORWAY, STAIRS)

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

# ── The areas ────────────────────────────────────────────────────────────────────────────────────
#
# Each area is its own map, one pixel per cell, ringed by wall except its doorways. The openings
# between areas are declared in src/game/areas.cpp; the doorway and stairs cells here are where those
# openings say they are. The areas are grouped by layout: the overworld, and the rooms of DungeonOne.

# World ids, as WorldCell in src/assets/assets.h, spread from near black (stairs) to white (floor).
STAIRS_ID, WALL_ID, WATER_ID, DOORWAY_ID, FLOOR_ID = 32, 64, 128, 192, 255

# Collision ids, as CollisionCell in src/assets/assets.h: solid is black, open is white.
SOLID_ID, OPEN_ID = 0, 255


def column(x, first, last):
    return [(x, y) for y in range(first, last + 1)]


def row(y, first, last):
    return [(x, y) for x in range(first, last + 1)]


# The pond in the lake's interior: rows of (first column, last column), from its top row down.
POND_TOP = 5
POND_ROWS = [(18, 21), (17, 22), (16, 23), (16, 23), (17, 22), (18, 21)]

LAYOUTS = {
    "overworld": {
        # The start: exactly one play area, with a door east, a road south and the cave mouth inside.
        "meadow": dict(size=(20, 16), doorways=column(19, 6, 9) + row(15, 8, 11), stairs=[(12, 4), (13, 4)]),
        # Two play areas wide, with a pond; the camera follows across it. The dungeon door, past the
        # pond, fades down into DungeonOne.
        "lake": dict(size=(40, 16), doorways=column(0, 6, 9), stairs=[(30, 2), (31, 2)], pond=True),
        # One and a half play areas wide: not a multiple of the screen.
        "field": dict(size=(30, 16), doorways=row(0, 8, 11) + column(29, 3, 6)),
        # Smaller than the play area on both axes: centered on screen.
        "shrine": dict(size=(12, 10), doorways=column(0, 3, 6)),
        # Reached by a fade from the meadow's cave mouth; the stairs lead back up.
        "cave": dict(size=(12, 10), stairs=[(5, 4), (6, 4)]),
    },
    "dungeon-one": {
        # The way in: the door back up to the lake, and a door north into the hall.
        "entrance": dict(size=(20, 16), doorways=row(0, 8, 11), stairs=[(9, 13), (10, 13)]),
        # Doors south, west and east; its whole north wall open, joined seamlessly to the treasury.
        "hall": dict(size=(20, 16), doorways=row(15, 8, 11) + column(0, 6, 9) + column(19, 6, 9) + row(0, 1, 18)),
        # Each wing is smaller than the play area, with its door back to the hall.
        "west-wing": dict(size=(12, 10), doorways=column(11, 3, 6)),
        "east-wing": dict(size=(12, 10), doorways=column(0, 3, 6)),
        # One and a half play areas wide; its south wall open over the hall's width, centered.
        "treasury": dict(size=(30, 16), doorways=row(15, 6, 23)),
    },
}


def area_cells(size, doorways=(), stairs=(), pond=False):
    width, height = size
    cells = [[WALL_ID if x in (0, width - 1) or y in (0, height - 1) else FLOOR_ID for x in range(width)]
             for y in range(height)]
    for (x, y) in doorways:
        cells[y][x] = DOORWAY_ID
    for (x, y) in stairs:
        cells[y][x] = STAIRS_ID
    if pond:
        for r, (first, last) in enumerate(POND_ROWS):
            for x in range(first, last + 1):
                cells[POND_TOP + r][x] = WATER_ID
    return cells


def collision_cells(area):
    solid = {WALL_ID, WATER_ID}
    return [[SOLID_ID if cell in solid else OPEN_ID for cell in r] for r in area]


# ── Output ───────────────────────────────────────────────────────────────────────────────────────


def files():
    """Every file the script writes, by path under assets/."""
    out = {
        "gfx/tiles/terrain.png": sheet_png(TILES),
        "gfx/sprites/player.png": sheet_png(PLAYER),
        "gfx/sprites/enemy.png": sheet_png(ENEMY),
        "gfx/sprites/sword.png": sheet_png(SWORD),
        "gfx/hud/hearts.png": sheet_png(HUD),
        "palettes/tiles/floor.png": palette_png(FLOOR_PAL),
        "palettes/tiles/wall.png": palette_png(WALL_PAL),
        "palettes/tiles/water.png": palette_png(WATER_PAL),
        "palettes/sprites/player.png": palette_png(PLAYER_PAL),
        "palettes/sprites/enemy.png": palette_png(ENEMY_PAL),
        "palettes/sprites/sword.png": palette_png(SWORD_PAL),
        "palettes/hud/hearts.png": palette_png(HEARTS_PAL),
    }
    for layout, areas in LAYOUTS.items():
        for name, spec in areas.items():
            area = area_cells(**spec)
            out[f"maps/{layout}/{name}/tiles.png"] = map_png(area)
            out[f"maps/{layout}/{name}/collision.png"] = map_png(collision_cells(area))
    return out


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
