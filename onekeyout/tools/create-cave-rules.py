#!/usr/bin/env python3
"""Rebuild original cave art and native Tiled rule maps, never a playable map.

Usage: python3 tools/create-cave-rules.py OUTPUT_DIRECTORY
Outputs use paths relative to an onekeyout-shaped directory. Refuses overwrite.
"""
from pathlib import Path
import importlib.util
import sys
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location("art", Path(__file__).with_name("create-assets.py"))
art = importlib.util.module_from_spec(spec)
spec.loader.exec_module(art)


def canonical(mask):
    for diagonal, a, b in ((16, 1, 2), (32, 2, 4), (64, 4, 8), (128, 8, 1)):
        if not (mask & a and mask & b):
            mask &= ~diagonal
    return mask


def props(node, **values):
    p = ET.SubElement(node, "properties")
    for name, value in values.items():
        kind = "bool" if isinstance(value, bool) else "int" if isinstance(value, int) else "string"
        ET.SubElement(p, "property", name=name, type=kind, value=str(value).lower() if kind == "bool" else str(value))


def write_xml(path, root):
    path.parent.mkdir(parents=True, exist_ok=True)
    ET.indent(root, space=" ")
    path.write_bytes(ET.tostring(root, encoding="UTF-8", xml_declaration=True) + b"\n")


def rules_map(path, rules):
    width, height = 48, ((len(rules) + 7) // 8) * 7
    root = ET.Element("map", version="1.10", tiledversion="1.12.2", orientation="orthogonal",
                      renderorder="right-down", width=str(width), height=str(height), tilewidth="32", tileheight="32", infinite="0")
    props(root, MatchOutsideMap=True, IgnoreLock=True, AutomappingRadius=3)
    ET.SubElement(root, "tileset", firstgid="1", source="../assets/tilesets/cave.tsx")
    layers = {}
    for i, rule in enumerate(rules):
        ox, oy = (i % 8) * 6 + 1, (i // 8) * 7 + 2
        for name, cells in rule.items():
            data = layers.setdefault(name, [0] * (width * height))
            for (x, y), tile in cells.items():
                data[(oy+y)*width + ox+x] = tile + 1
    for i, (name, data) in enumerate(layers.items(), 1):
        layer = ET.SubElement(root, "layer", id=str(i), name=name, width=str(width), height=str(height))
        ET.SubElement(layer, "data", encoding="csv").text = "\n" + ",\n".join(
            ",".join(map(str, data[y*width:(y+1)*width])) for y in range(height)) + "\n"
    write_xml(path, root)


def create(out):
    paths = [out / "assets/tilesets/cave.tsx", out / "assets/images/cave.png"]
    paths += [out / "automapping" / name for name in ("00-clear.tmx", "10-rock.tmx", "20-faces.tmx", "rules.txt")]
    if any(p.exists() for p in paths):
        raise SystemExit("Refusing to overwrite cave assets or rules; use an empty output directory.")
    masks = sorted({canonical(i) for i in range(256)})
    assert len(masks) == 47
    # 47 roof tiles, 9 upper faces, 9 lower faces, solid marker, Empty, NonEmpty, Ignore.
    solid, empty, occupied, ignore = 65, 66, 67, 68
    canvas = art.Canvas(256, 288)
    tileset = ET.Element("tileset", version="1.10", tiledversion="1.12.2", name="Cave wall - two tile height",
                         tilewidth="32", tileheight="32", tilecount="69", columns="8")
    ET.SubElement(tileset, "image", source="../images/cave.png", width="256", height="288")
    for tile_id in range(69):
        ox, oy = (tile_id % 8) * 32, (tile_id // 8) * 32
        def rect(x, y, w, h, c):
            canvas.rect(ox+x, oy+y, w, h, c)
        tile = ET.SubElement(tileset, "tile", id=str(tile_id))
        if tile_id < 47:
            mask = masks[tile_id]
            props(tile, cavePart="roof", neighborMask=mask, sortY=96)
            rect(0, 0, 32, 32, "20383e")
            # Irregular rock strata, seamless fill without per-cell frames.
            rect(3, 8, 12, 2, "284449")
            rect(18, 22, 9, 2, "182e35")
            rect(8, 26, 4, 1, "365155")
            for bit, box, light in ((1, (0, 0, 32, 3), "547471"), (2, (29, 0, 3, 32), "142b33"),
                                    (4, (0, 29, 32, 3), "6b8b7d"), (8, (0, 0, 3, 32), "476963")):
                if not mask & bit:
                    rect(*box, light)
            for diag, a, b, x, y in ((16,1,2,27,0), (32,2,4,27,27), (64,4,8,0,27), (128,8,1,0,0)):
                if mask & a and mask & b and not mask & diag:
                    rect(x, y, 5, 5, "688878")
                    rect(x + (2 if x == 0 else 0), y + (2 if y == 0 else 0), 3, 3, "35565a")
        elif tile_id < 65:
            upper = tile_id < 56
            variant = (tile_id - 47) % 9
            left, right = variant // 3, variant % 3
            props(tile, cavePart="upper" if upper else "lower", edgeVariant=variant, sortY=64 if upper else 32)
            rect(0, 0, 32, 32, "405d61" if upper else "304950")
            rect(0, 0, 32, 3, "6b8b7d" if upper else "263e47")
            rect(3, 5, 13, 10, "4c6c6b" if upper else "3b575b")
            rect(17, 4, 2, 19, "2b454e")
            rect(0, 18, 15, 2, "304950" if upper else "263e47")
            rect(21, 21, 11, 2, "355158" if upper else "20383f")
            rect(8, 21, 2, 10, "2b454e" if upper else "223b43")
            if not upper:
                rect(0, 28, 32, 4, "142b33")
                rect(5, 26, 7, 2, "526f61")
                rect(23, 25, 5, 3, "48675c")
            for side, x in ((left, 0), (right, 28)):
                if side == 1:  # Exposed, convex wall end / doorway jamb.
                    rect(x, 0, 4, 32, "719080" if x == 0 else "1c333c")
                    rect(x+1, 8, 2, 13, "526f6c" if x == 0 else "29464e")
                elif side == 2:  # Concave return into adjoining rock.
                    rect(x, 0, 4, 32, "172e37")
                    rect(x, 0, 4, 3, "4b6969")
        else:
            if tile_id == solid:
                props(tile, caveSolid=True)
            else:
                props(tile, MatchType={empty:"Empty", occupied:"NonEmpty", ignore:"Ignore"}[tile_id])
            rect(0, 0, 32, 32, {solid:"a06cbe", empty:"be5862", occupied:"e4bf73", ignore:"567c90"}[tile_id])
            rect(3, 3, 26, 26, "263c40")
            rect(10, 10, 12, 12, "a06cbe" if tile_id == solid else "e4bf73")
    paths[1].parent.mkdir(parents=True, exist_ok=True)
    paths[1].write_bytes(canvas.png())
    write_xml(paths[0], tileset)
    clear = {"input1_CaveShape": {(0,0): occupied}, "input2_CaveShape": {(0,0): empty}}
    clear.update({"output_"+n: {(0,0): empty} for n in ("Walls", "CaveRoof", "CaveUpper", "CaveLower")})
    rules_map(paths[2], [clear])
    roof_rules = []
    directions = [(0,-1,1), (1,0,2), (0,1,4), (-1,0,8), (1,-1,16), (1,1,32), (-1,1,64), (-1,-1,128)]
    for tile_id, mask in enumerate(masks):
        inputs = {(0,0): solid}
        for x, y, bit in directions:
            irrelevant = bit >= 16 and canonical((mask & 15) | bit) == (mask & 15)
            inputs[x,y] = ignore if irrelevant else solid if mask & bit else empty
        roof_rules.append({"input_CaveShape": inputs, "output_Walls": {(0,0):solid},
                           "output_CaveRoof": {(0,-2):tile_id}})
    rules_map(paths[3], roof_rules)
    faces = []
    for left in range(3):
        for right in range(3):
            inputs = {(0,0):solid, (0,1):empty}
            for x, side in ((-1,left), (1,right)):
                inputs[x,0] = empty if side == 1 else solid
                inputs[x,1] = ignore if side == 1 else solid if side == 2 else empty
            variant = left*3 + right
            faces.append({"input_CaveShape": inputs, "output_CaveUpper": {(0,-1):47+variant},
                          "output_CaveLower": {(0,0):56+variant}})
    rules_map(paths[4], faces)
    paths[5].write_text("# Native Tiled Automapping; order is significant.\n00-clear.tmx\n10-rock.tmx\n20-faces.tmx\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    create(Path(sys.argv[1]))
