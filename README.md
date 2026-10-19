# treasure-finder

A Kingdom Come: Deliverance II mod (KCSE plugin) that marks where the treasure maps you carry lead, on the world map and in the world, and optionally where the maps themselves are found.

Targets game 1.5.6 with [KCSE](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3332) and [libKCD2](https://github.com/JerryYOJ/libKCD2).

## Layout

| Path | What |
| --- | --- |
| `research/findings.md` | How maps tie to their destinations, and the per-map table |
| `research/treasure_maps.json` | The curated map table: destination and source entity for each of the 38 map items |
| `tools/treasure_data/` | Game data tooling (Python, standard library only) |
| `.cache/<build>/` | Files unpacked from the game and the indexes built from them (not tracked) |

## Game data tooling

Needs [uv](https://docs.astral.sh/uv/). The game folder defaults to the Steam install; pass `--game <folder>` or set `KCD2_DIR` otherwise.

```bash
uv run treasure-data extract    # unpack item tables, quest graphs, Lua, English strings and level entities
uv run treasure-data index      # maps.json, containers.csv, pois.csv under .cache/<build>/index
uv run treasure-data validate   # check research/treasure_maps.json against the installed build
```

Each game build gets its own cache folder, named after the build string KCSE logs (for example `release_1_5-15693`).

After a game patch, run `validate`. It reports new or removed map items, and destination or source entities that are gone, renumbered, renamed or moved more than `--tolerance` metres (default 2). Entities are identified by level, entity id and level GUID: names repeat within a level, so they only confirm a match. Anything it flags needs re-researching for that map only.
