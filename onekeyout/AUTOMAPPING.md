# Two-tile cave walls

`assets/maps/cave-vault.tmx` is the default playable room. It preserves the
Flooded Vault's footprint, water, boardwalk, props and objectives, with two rows
of headroom added above it. The visible wall face is **64 px high** (two 32 px
tiles); the roof surface occupies another tile above the face. This is visual
height, not a second walkable floor.

## Authoring in Tiled

1. Open `onekeyout.tiled-project`, then `assets/maps/cave-vault.tmx`.
2. Show and select `CaveShape`. Stamp the purple `caveSolid` marker from
   `assets/tilesets/cave.tsx` (local tile ID 65). Erase markers to open passages.
3. Keep the first two rows empty so the projected roof fits within the map.
4. Run **Map > AutoMap** with no selection to regenerate the whole map.
5. Hide `CaveShape` for a clean preview, save, and run **Play One Key Out**.

The generated `Walls`, `CaveRoof`, `CaveUpper` and `CaveLower` layers are locked.
The rules explicitly allow writing these locked layers. Keep their names,
dimensions and zero offsets; edit `CaveShape` instead of generated cells.
Full-map AutoMap is the supported workflow; selective/live drawing is not yet
part of the verification contract. Tiled's layer preview does not reproduce the
game's per-wall ground-Y sorting. Check occlusion in the game.

`automapping/project.txt` applies rules only to `cave-*.tmx`. This protects the
original `first-room.tmx` and other maps with hand-authored `Walls`. A new cave
map also needs integer map property `caveHeight = 2` and the five cave layers.

## Native rule pipeline

| Rule map | Input | Output |
| --- | --- | --- |
| `00-clear.tmx` | Every empty or occupied CaveShape cell | Erase all four generated layers |
| `10-rock.tmx` | Rock plus cardinal/diagonal neighborhood | Collision at the source cell; roof two cells above |
| `20-faces.tmx` | Rock with open floor to the south; adjacent wall returns | Upper face one cell above; lower face at source cell |

There are 47 distinct roof neighborhoods, including concave notches, and nine
face combinations: each end may continue, terminate at an exposed corner or
door jamb, or turn into an inner corner. One boundary therefore produces
spatially offset tiles on multiple layers. These are ordinary native Tiled
Automapping rule maps, not Wang sets or a replacement rule solver.

The rules are deterministic. The reset stage removes old faces when a wall is
erased, a doorway is opened, or a corner changes. The unfiltered `rules.txt`
is for the dedicated authoring tool; normal editor use goes through the
filename-filtered `project.txt`.

## Collision and draw order

`Walls` contains exactly the occupied cells of `CaveShape`; projected wall art
does not enlarge that footprint. `CaveShape` and `Walls` may be hidden in cave
maps. The game ignores their appearance and validates generated occupancy.

Each roof/upper/lower tile carries a `cavePart` and integer `sortY` of 96/64/32
respectively. Those offsets give all pieces of a column the same ground anchor.
The renderer sorts them with props, gate, key and player by ground Y. A player
behind a tall wall can be occluded; a player in front covers the wall correctly.
The front-facing wall extends upward, never two extra collision cells into the
passage. Existing water movement and boardwalk masks keep their original rules.

The loader rejects missing parts, incorrect anchors, transformed art and stale
collision/face occupancy. Detailed corner choices are audited by the native
Automapping verification tool, not regenerated at runtime.

## Reproduction and verification

From the repository root:

```sh
./onekeyout/run.sh --automap  # Regenerate cave-vault.tmx using native Tiled code.
./onekeyout/run.sh --check    # Check saved output, native rules, and gameplay.
./onekeyout/run.sh
```

`tools/automap.cpp` loads a `MapDocument` and calls Tiled's public `AutoMapper`
API, following the upstream Automapping tests and full-map rule sequence.
It runs in a separate process with an isolated application identity. The
runner accepts a flat list of TMX rule maps; the filename-filtered project list
is handled by the Tiled editor. It is not a GUI/Undo test and does not connect
to tiled-ai. Only this authoring
tool links `libtilededitor`; the shipped game still links `libtiled` and Qt.
The first `--automap` or `--check` build consequently also builds editor code.

Checks cover all 256 raw eight-neighbor combinations, map edges, repeated
generation, erase/restore, and preservation of authored layers and objects.
Editor UI Undo/Redo and live painting are not covered by this command.
Game tests cover unchanged collision,
shared depth anchors, stale-output rejection, water/boardwalk movement and a
keyboard route through the doorway to the key and exit. Evidence images are in
`build/evidence/12-cave-start.png` through `16-cave-complete.png`.

The original cave art and rule definitions are reproducible without external
dependencies:

```sh
python3 onekeyout/tools/create-cave-rules.py onekeyout/build/new-cave-source
```

This recipe refuses to overwrite its output, writes only art/TSX/rule maps,
and never generates the playable map's output layers. Compare its files with
the committed assets before intentionally adopting a change. Local asset
collection candidates were checked; this atlas was authored specifically for
the game's 32 px palette and projection contract.

References: [Tiled Automapping](https://doc.mapeditor.org/en/stable/manual/automapping/),
[RPG cliffs](https://doc.mapeditor.org/en/stable/manual/automapping/#rpg-cliffs).
