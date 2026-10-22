# One Key Out dependencies

The game links the existing Tiled `src/libtiled` library and Qt Core, Gui and
Widgets. The upstream editor and its export plugins are not linked into the game.

- **libtiled**: copyright the Tiled contributors, BSD 2-Clause. The repository's
  `LICENSE.BSD` and notices in `src/libtiled` contain the terms and authorship.
  The packaging script includes the BSD text with the app.
- **Qt**: copyright The Qt Company and other contributors. Qt is dynamically
  linked. Module and third-party license information is available at
  <https://doc.qt.io/qt-6/licensing.html> and <https://doc.qt.io/qt-6/licenses-used-in-qt.html>.
  The packaging script uses the Qt installation selected by the build environment.

The dungeon tiles and actor sprites are original pixel art authored for this
game using `tools/create-assets.py`. The water and boardwalk atlases are original
art from `tools/create-terrain-art.py`, imported into native Tiled terrain sets
through tiled-ai. No external game assets were imported. Both recipes refuse to
overwrite existing art. See `TERRAIN.md` for the terrain authoring contract.
The UI requests the system-installed Avenir Next font, with Qt's platform font
fallback. No font files are distributed.

`dist/One Key Out.app` is an ad-hoc-signed local development package, not a
notarized distribution release.
