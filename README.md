# treasure-finder

A Kingdom Come: Deliverance II mod that marks where the treasure maps you carry lead, on the world map, the compass and in the world. A [KCSE](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3332) plugin for game 1.5.6.

## Layout

| Path | What |
| --- | --- |
| `plugin/src/` | The plugin (C++23) |
| `mod/` | Manifest, strings, `.pak` data, and `game_files.json` (game textures copied into the `.pak` at build time) |
| `research/` | How maps tie to their destinations; the 38-map table |
| `tools/treasure_data/` | Game data tooling (Python) |

## Requirements

- Visual Studio 2022+ (Build Tools) with the C++ workload, CMake 4.0+, Ninja
- [uv](https://docs.astral.sh/uv/)
- The game with KCSE installed

## Build

From an x64 Visual Studio developer shell, after `uv sync`:

```bash
cmake --preset release
cmake --build --preset release    # plugin and build/release/mod/treasurefinder
cmake --build --preset deploy     # also replaces <KCD2_DIR>/Mods/treasurefinder
```

`debug` is the unoptimized preset. `KCD2_DIR` defaults to the Steam install. Deploy with the game closed; a new mod folder also needs a line in `Mods/mod_order.txt`. The plugin logs to `kcd.log` with a `[TreasureFinder]` prefix.

## Game data tooling

```bash
uv run treasure-data extract    # unpack the game files the tooling reads
uv run treasure-data index      # build indexes for this game build
uv run treasure-data validate   # check research/treasure_maps.json against the installed build
```

`--game <folder>` or `KCD2_DIR` picks the install; files go to `%LOCALAPPDATA%\treasure-finder\<build>`. After a game patch, run `validate` and re-research only the maps it flags.
