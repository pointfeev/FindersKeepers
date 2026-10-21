# Finders Keepers

A Kingdom Come: Deliverance II mod that marks where treasure maps, quest maps and riddle notes lead, and where each one is found, on the world map, the compass and in the world. A [KCSE](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3332) plugin for game 1.5.6.

## Layout

| Path                   | What                                                                                                       |
|------------------------|------------------------------------------------------------------------------------------------------------|
| `plugin/src/`          | The plugin (C++23)                                                                                         |
| `mod/`                 | Manifest, strings, `.pak` data, and `game_files.json` (game textures copied into the `.pak` at build time) |
| `research/`            | How maps tie to their destinations; the map table (45 maps, 40 marked)                                     |
| `tools/treasure_data/` | Game data tooling (Python)                                                                                 |

## Requirements

- Visual Studio 2022+ (Build Tools) with the C++ workload, CMake 4.0+, Ninja
- [uv](https://docs.astral.sh/uv/)
- The game with KCSE installed

## Build

From an x64 Visual Studio developer shell, after `uv sync`:

```bash
cmake --preset release
cmake --build --preset release    # plugin and build/release/mod/finderskeepers
cmake --build --preset deploy     # also replaces <KCD2_DIR>/Mods/finderskeepers
cmake --build --preset package    # also zips it as build/release/FindersKeepers-<version>.zip
cmake --build --preset publish    # also publishes that zip as the GitHub release
```

Publishing the zip is in [store/README.md](store/README.md).

`debug` is the unoptimized preset. `KCD2_DIR` defaults to the Steam install. Deploy with the game closed; a new mod folder also needs a line in `Mods/mod_order.txt`. The plugin logs to `kcd.log` with a `[FindersKeepers]` prefix.

## Game data tooling

```bash
uv run treasure-data extract    # unpack the game files the tooling reads
uv run treasure-data index      # build indexes for this game build
uv run treasure-data validate   # check research/treasure_maps.json against the installed build
```

`--game <folder>` or `KCD2_DIR` picks the install; files go to `%LOCALAPPDATA%\FindersKeepers\<build>`. After a game patch, run `validate` and re-research only the maps it flags.
