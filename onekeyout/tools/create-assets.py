#!/usr/bin/env python3
"""First-use authoring recipe. Refuses to overwrite art or maps; no dependencies."""
from pathlib import Path
import binascii
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]


class Canvas:
    def __init__(self, width, height):
        self.width, self.height = width, height
        self.pixels = bytearray(width * height * 4)

    def rect(self, x, y, w, h, color):
        color = bytes.fromhex(color.lstrip("#"))
        if len(color) == 3:
            color += b"\xff"
        for yy in range(max(0, y), min(self.height, y + h)):
            for xx in range(max(0, x), min(self.width, x + w)):
                offset = (yy * self.width + xx) * 4
                self.pixels[offset:offset + 4] = color

    def png(self):
        def chunk(kind, data):
            return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", binascii.crc32(kind + data))
        stride = self.width * 4
        raw = b"".join(b"\0" + self.pixels[y * stride:(y + 1) * stride] for y in range(self.height))
        return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", self.width, self.height, 8, 6, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b"")


def create():
    tiles = Canvas(256, 32)
    for i in range(3):
        x = i * 32
        tiles.rect(x, 0, 32, 32, "263c40")
        tiles.rect(x + 1, 1, 30, 30, ["30484a", "2e4548", "31494b"][i])
        tiles.rect(x + 2, 2, 28, 1, "3a5353")
        tiles.rect(x + 2, 30, 28, 1, "243b3f")
        tiles.rect(x + 7 + i * 4, 9 + i * 3, 3, 1, "3b5454")
        tiles.rect(x + 18 - i * 3, 23, 4, 1, "263d40")
    for i in (3, 4):
        x = i * 32
        tiles.rect(x, 0, 32, 32, "14292f")
        tiles.rect(x + 1, 1, 30, 25, "406064")
        tiles.rect(x + 2, 2, 28, 2, "61807c")
        tiles.rect(x + 2, 5, 28, 8, "4a696b")
        tiles.rect(x + 15, 4, 2, 9, "304c52")
        tiles.rect(x + 2, 14, 28, 1, "243f47")
        tiles.rect(x + 2, 16, 28, 8, "38565d")
        tiles.rect(x + 8, 16, 2, 8, "263f49")
        tiles.rect(x + 2, 25, 28, 2, "243c46")
        tiles.rect(x + 2, 28, 28, 3, "10232b")
        if i == 4:
            tiles.rect(x + 20, 6, 8, 3, "648071")
            tiles.rect(x + 24, 9, 4, 4, "4c6d5f")
    tiles.rect(160, 0, 32, 32, "30484a")
    tiles.rect(162, 1, 28, 30, "725747")
    tiles.rect(164, 1, 24, 30, "a08056")
    tiles.rect(166, 1, 20, 30, "795d48")
    for y in (6, 14, 22):
        tiles.rect(172, y, 8, 2, "a88b5f")
        tiles.rect(175, y - 2, 2, 6, "a88b5f")
    # Transparent decorative floor plants and a bronze lantern.
    for x, y, w, h, color in [(8, 23, 17, 5, "172e32"), (11, 19, 11, 8, "7c6252"),
                              (9, 17, 15, 3, "b7956b"), (15, 7, 3, 12, "66866c"),
                              (10, 9, 6, 5, "80a17b"), (18, 12, 7, 4, "648d77"),
                              (16, 4, 5, 6, "a0b38a")]:
        tiles.rect(192 + x, y, w, h, color)
    for x, y, w, h, color in [(11, 25, 12, 4, "152b31"), (14, 17, 6, 9, "967447"),
                              (10, 7, 14, 13, "b69558"), (12, 9, 10, 9, "edc775"),
                              (14, 10, 6, 6, "fff0b5"), (13, 5, 8, 2, "7a6343")]:
        tiles.rect(224 + x, y, w, h, color)

    actors = Canvas(256, 32)
    for frame in range(5):
        x = frame * 32
        shift = 1 if frame in (1, 4) else 0
        def r(xx, yy, w, h, c):
            actors.rect(x + xx, yy + shift, w, h, c)
        r(9, 5, 14, 14, "182c32")
        r(10, 4, 12, 13, "e2d5ad")
        r(12, 3, 8, 2, "f7eac3")
        r(8, 15, 16, 10, "1a3036")
        r(10, 15, 12, 9, "4a7975")
        r(11, 18, 9, 3, "64978a")
        r(8, 16, 3, 7, "d7c49f")
        r(22, 16, 3, 7, "d7c49f")
        r(11, 24, 4, 5 - (frame == 1), "22333a")
        r(18, 24, 4, 5 - (frame == 2), "22333a")
        r(10, 28 - (frame == 1), 5, 2, "ae8b69")
        r(18, 28 - (frame == 2), 5, 2, "ae8b69")
        r(10, 14, 13, 3, "c07851")
        r(20, 17, 3, 6, "e09763")
        if frame < 3:
            r(12, 8, 10, 6, "8a7160")
            r(13, 9, 8, 4, "eacba1")
            r(13, 10, 2, 2, "24343a")
            r(19, 10, 2, 2, "24343a")
        else:
            r(12, 7, 8, 7, "b6b28e")
            r(14, 5, 4, 3, "ece1b9")
    for x, y, w, h, c in [(9, 7, 12, 3, "745a3e"), (7, 10, 16, 7, "745a3e"),
                          (9, 8, 12, 3, "ffe2a0"), (8, 11, 4, 5, "efbd63"),
                          (18, 11, 4, 5, "d49a47"), (10, 16, 10, 3, "f0be62"),
                          (14, 18, 4, 10, "f4cc79"), (18, 23, 4, 3, "e5ac53"),
                          (18, 27, 4, 3, "e5ac53")]:
        actors.rect(160 + x, y, w, h, c)
    for i in (6, 7):
        x = i * 32
        actors.rect(x + 2, 1, 28, 31, "142b31")
        actors.rect(x + 4, 3, 24, 27, "69877d")
        actors.rect(x + 7, 5, 18, 25, "233b3e" if i == 6 else "3d8c7d")
        actors.rect(x + 9, 7, 14, 22, "29484a" if i == 6 else "69b49a")
        actors.rect(x + 4, 3, 24, 2, "93b6a0")
        actors.rect(x + 5, 30, 22, 2, "c9c69c")
        if i == 6:
            for yy in (9, 17, 25):
                actors.rect(x + 8, yy, 16, 2, "42615e")
            actors.rect(x + 14, 15, 5, 7, "e9bf70")
            actors.rect(x + 16, 16, 1, 4, "6b513b")
        else:
            actors.rect(x + 12, 9, 8, 20, "b1dfb7")
            actors.rect(x + 15, 8, 3, 22, "e9efc3")

    rows = [
        "########################",
        "#......#.......#.......#",
        "#..S...#.......#...E...#",
        "#......#.......#.......#",
        "#......#.......#.......#",
        "#......#...............#",
        "#......#.......#.......#",
        "#......#####.###.......#",
        "#......................#",
        "#......#.......#####.###",
        "#......#.......#.......#",
        "#......#.......#.......#",
        "#......#...K...#.......#",
        "#..............#.......#",
        "#......#.......#.......#",
        "########################",
    ]
    # Keep the outer boundary at exactly 24 columns.
    rows = [row if len(row) == 24 else row[:-1] + "." * (24 - len(row)) + "#" for row in rows]
    assert all(len(row) == 24 for row in rows)
    ground, walls, foreground = [], [], []
    locations = {}
    for y, row in enumerate(rows):
        for x, char in enumerate(row):
            ground.append(1 + (x * 7 + y * 13) % 3)
            walls.append((5 if (x + y * 3) % 7 == 0 else 4) if char == "#" else 0)
            foreground.append(0)
            if char in "SKE":
                locations[char] = (x, y)
    for x, y in [(2, 1), (5, 1), (9, 1), (13, 1), (17, 1), (21, 1), (2, 14), (21, 14)]:
        foreground[y * 24 + x] = 7 if x % 2 == 0 else 8
    for x, y in [(11, 11), (11, 12), (11, 13), (19, 2), (19, 3), (19, 4)]:
        if not walls[y * 24 + x]:
            ground[y * 24 + x] = 6

    def layer(index, name, cells):
        lines = [",".join(map(str, cells[y * 24:(y + 1) * 24])) for y in range(16)]
        return f' <layer id="{index}" name="{name}" width="24" height="16">\n  <data encoding="csv">\n' + ",\n".join(lines) + '\n  </data>\n </layer>\n'

    sx, sy = locations["S"]
    kx, ky = locations["K"]
    ex, ey = locations["E"]
    tmx = '<?xml version="1.0" encoding="UTF-8"?>\n<map version="1.10" tiledversion="1.12.2" orientation="orthogonal" renderorder="right-down" width="24" height="16" tilewidth="32" tileheight="32" infinite="0" nextlayerid="5" nextobjectid="4">\n'
    tmx += ' <properties><property name="title" value="THE LANTERN VAULT"/></properties>\n <tileset firstgid="1" source="../tilesets/dungeon.tsx"/>\n'
    tmx += layer(1, "Ground", ground) + layer(2, "Walls", walls)
    tmx += f' <objectgroup id="3" name="Objects">\n  <object id="1" name="Start" type="PlayerSpawn" x="{sx * 32 + 16}" y="{sy * 32 + 16}"><point/></object>\n  <object id="2" name="Golden key" type="Key" x="{kx * 32 + 6}" y="{ky * 32 + 6}" width="20" height="20"/>\n  <object id="3" name="Way out" type="Exit" x="{ex * 32 + 2}" y="{ey * 32 + 2}" width="28" height="28"/>\n </objectgroup>\n'
    tmx += layer(4, "Foreground", foreground) + '</map>\n'
    tsx = '<?xml version="1.0" encoding="UTF-8"?>\n<tileset version="1.10" tiledversion="1.12.2" name="dungeon" tilewidth="32" tileheight="32" tilecount="8" columns="8">\n <image source="../images/dungeon.png" width="256" height="32"/>\n</tileset>\n'
    fixture = tmx.replace('../tilesets/dungeon.tsx', '../../assets/tilesets/dungeon.tsx')
    outputs = {
        "assets/images/dungeon.png": tiles.png(),
        "assets/images/actors.png": actors.png(),
        "assets/tilesets/dungeon.tsx": tsx.encode(),
        "assets/maps/first-room.tmx": tmx.encode(),
        "tests/fixtures/valid-room.tmx": fixture.encode(),
    }
    existing = [name for name in outputs if (ROOT / name).exists()]
    if existing:
        raise SystemExit("Refusing to overwrite authored assets: " + ", ".join(existing))
    for name, content in outputs.items():
        path = ROOT / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(content)
    print("Created original pixel art and the first room. Edit the TMX in Tiled from here.")


if __name__ == "__main__":
    create()
