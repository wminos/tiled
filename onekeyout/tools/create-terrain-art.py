#!/usr/bin/env python3
"""Original 32 px terrain atlas recipe; refuses to overwrite authored assets.

Produces art and a semantic catalog, never TMX or TSX. Tiled owns Wang setup
and terrain painting. Usage: python3 tools/create-terrain-art.py OUTPUT_DIR
"""
import importlib.util
import json
from pathlib import Path
import sys

spec = importlib.util.spec_from_file_location("original_art", Path(__file__).with_name("create-assets.py"))
art = importlib.util.module_from_spec(spec)
spec.loader.exec_module(art)


def create(output):
    output.mkdir(parents=True, exist_ok=True)
    targets = [output / name for name in ("water.png", "boardwalk.png", "terrain-catalog.json")]
    if any(path.exists() for path in targets):
        raise SystemExit("Refusing to overwrite existing terrain art or catalog.")
    water, boards = art.Canvas(128, 128), art.Canvas(128, 128)
    catalog = {}
    for kind, canvas in (("water", water), ("boardwalk", boards)):
        assignments, properties = [], []
        for mask in range(16):
            ox, oy = mask % 4 * 32, mask // 4 * 32
            bits = [(mask >> i) & 1 for i in range(4)]
            wang = [0] * 8
            for i, bit in enumerate(bits):
                wang[2 * i + (1 if kind == "water" else 0)] = bit
            assignments.append({"tileId": mask, "wangId": wang})
            properties.append({"tileId": mask, "name": "waterCorners" if kind == "water" else "boardwalkEdges", "value": mask})
            for y in range(32):
                for x in range(32):
                    if kind == "water":
                        u, v = (x + .5) / 32, (y + .5) / 32
                        tr, br, bl, tl = bits
                        coverage = tl * (1-u) * (1-v) + tr * u * (1-v) + bl * (1-u) * v + br * u * v
                        if coverage < .5:
                            continue
                        color = "407d80" if coverage < .57 else "28636e" if coverage < .66 else "205564"
                        # Tile-periodic ripples; all matching edges share the same pixels.
                        if coverage > .68 and ((y == 10 and 6 <= x < 16) or (y == 23 and 19 <= x < 26)):
                            color = "4a8b93"
                        if coverage > .68 and (x * 13 + y * 7) % 59 == 0:
                            color = "2b6270"
                    else:
                        north, east, south, west = bits
                        inside = (9 <= x < 23 and 9 <= y < 23) or (north and 9 <= x < 23 and y < 16) or (south and 9 <= x < 23 and y >= 16) or (east and x >= 16 and 9 <= y < 23) or (west and x < 16 and 9 <= y < 23)
                        if not inside:
                            continue
                        color = "a98553"
                        if y % 8 == 0:
                            color = "4b4035"
                        elif y % 8 == 1:
                            color = "cfaa70"
                        elif (x + 2*y) % 19 == 0:
                            color = "987548"
                        if x in (10, 21) and y % 8 == 3:
                            color = "e1c58a"
                    canvas.rect(ox + x, oy + y, 1, 1, color)
        catalog[kind] = {"definition": {"name": "Shallow water" if kind == "water" else "Dry boardwalk", "type": "corner" if kind == "water" else "edge", "colors": [{"name": "Water" if kind == "water" else "Boardwalk"}], "assignments": assignments}, "properties": properties}
    targets[0].write_bytes(water.png())
    targets[1].write_bytes(boards.png())
    targets[2].write_text(json.dumps(catalog, indent=2) + "\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    create(Path(sys.argv[1]))
