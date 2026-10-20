# Design

How the mod decides what to mark, the options it will offer, and what was considered and left out. Per-map research is in [findings.md](findings.md).

## Scope

Treasure and what leads to it: treasure maps, the carved map in the Tunnel in the Cliffs, and riddle notes (the Strange Little Verses, the Strange Riddle).

## Rules

**State.** The mod stores nothing but its options. Every mark is worked out from game state at each probe.

**Destinations** (mark type `TreasureFinder`)
- A map's destinations are marked once the player has read it, carried or not: the game keeps reading progress per item class, and it survives the map being dropped and a reload (tested in game). A carving counts once examined, and only after Laboratores (`socky`), because its trigger is also off before the open world unlocks.
- A map that leads to several places marks each one.
- A destination clears when its hole is dug, its container is emptied or its item is taken.
- A riddle note's destination also clears once the player has, or has read, the next note.

**Map locations** (mark type `TreasureFinderMap`): where a map is found, marked while the player has neither carried nor read it, and while the container or item slot still holds it.

**Quests**
- A map tied to a quest stops being marked once the quest is done or failed. Places flagged `after_quest` are marked only from then on (the secret mint's ledger chest after Via Argentum).
- A destination hides while the map shows a tracked quest's marker within 15 m of it (a point or a search area's bounds).

**Display**
- Tooltips read "Treasure: <map name>" in each language, with its own punctuation.
- Each marked place within range gets its own in-world icon.

**Not possible under the state rule**: an unread map that leaves the inventory. Finding it again would mean remembering when it left, or searching every inventory and dropped item.

## Options

Settings live in `Mods/treasurefinder/KCSE/Plugins/TreasureFinder.ini` (`plugin/src/options`), the convention other KCSE plugins use. The mod does not ship it: the plugin writes it with the defaults when missing, re-reads it when it changes on disk, and adds keys a new version brings. The deploy step keeps it. The map legend's Track toggle per mark type still applies on top.

[Mod Configuration Menu](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3363) (MCM), when installed, shows the same settings in the pause menu. It stores nothing: the plugin hears its "build settings" and "value changed" messages through KCSE's messaging interface and writes the INI. Without MCM the messages never come and the INI alone holds the settings. MCM's limits: it needs the Address Library (1.5.6 only, not Game Pass), so it breaks on game updates until republished, and it has no controller support. Its headers are GPL-3.0, so the plugin declares the few interfaces it calls itself.

| Section   | Key                     | Default | Range   |
|-----------|-------------------------|---------|---------|
| Display      | `MapMarkers`            | on      |         |
| Display      | `FloatingIcons`         | on      |         |
| Marks        | `Destinations`          | on      |         |
| Marks        | `MapLocations`          | on      |         |
| Maps         | `TreasureMaps`, `QuestMaps`, `RiddleNotes` (each covers both marks) | on | |
| Rules        | `RequireRead`           | on      |         |
| Rules        | `RequireCarried`        | off     |         |
| Rules        | `KeepFinished`          | off     |         |
| Rules        | `RetireQuestMaps`       | on      |         |
| Rules        | `HideUnderQuestMarkers` | on      |         |
| Distances | `IconShowDistance`      | 40 m    | 10-150  |
| Distances | `IconFullDistance`      | 25 m    | 0-150   |
| Distances | `IconSize`              | 48 px   | 16-128  |
| Distances | `IconHeight`            | 1.2 m   | 0-3     |
| Distances | `QuestMarkerReach`      | 15 m    | 0-50    |

Map and compass markers come from one game entity mark (`RegisterEntityMark` builds the compass marker), so they share one switch; separating them needs RE. The compass range is the game's. Labels are English only; MCM can show localised `@` keys if they are added to the localisation table.

**Icons.** Treasure uses the game's debt-notes chest icon and map locations the map book; both read well enough for now. Custom icons are feasible without new tools (our own BC7 encoder writing into a game icon's DDS header, with undiscovered and shadow variants generated), and mock-ups exist (a wax seal with an X and one with a folded map was the favourite). Revisit only if more mark types are added and need a matching set.

**Track toggle.** Settings act first (what is created at all) and the map legend's Track toggle second (whether a type shows), so a mark needs both. With map markers off the legend lists none of our types, so the in-world icons then ignore Track.

## Extending

- **A mark of an existing kind** (another map or note): a row in `treasure_maps.json`; the build generates, validates and localises it.
- **A category:** the generator's category list, `MapTable::Category`, a settings entry (the INI and MCM follow) and a case in the probe's category check.
- **A mark type** (own icon and legend entry): an entry in `MarkType::kTypes` (naming, importance and tooltips already loop over it), its icons in `game_files.json` and its legend string. Three places still assume exactly two types: `ProbeResult::Target` has `bool source`, `GameMark` picks the type from it, and `plugin.cpp` keeps two position lists and two icon calls. Before the next type: give `Target` a `const MarkType::Type *` and loop over `kTypes` there; small, and then a type is data.
- **A non-map source** (stashes, keys): the probe only walks the map table. Before the first one, split it into providers that each return targets (the map table becomes one). Not before: one provider does not need the split.

## Open questions (need play)

- Do item slots keep their item while their part of the level is unloaded?
- Is the Tunnel in the Cliffs reachable before Laboratores?
- Is 15 m right for the quest marker check, and what does gathering quest markers every probe cost?

## Set aside

Each would be its own mark type and can be derived from game state. A worth filter could bring some back without clutter: mark a place only while what is left in it is worth having, judged by:
- a minimum total value in groschen (money plus item prices);
- or anything valuable regardless of price: an unread skill book or recipe, a unique or quest item, a dice badge.

It would read the container's live contents, falling back to its loot table in the game data while the container is not filled yet. Needs RE: walking an inventory's items, their classes and amounts, prices, and whether a recipe is known.

- **Hidden stashes:** 269 containers with the game's hand-placed loot (`inventory_poi_*`) at 112 sites. Best: Otter's Treasure (about 850 groschen), the Pschitoky buried armour, the Two Dead Trees jar, Vagrant's Hut. Left out as clutter: the Scholarship perk Explorer already reveals every site.
- **Dig sites:** 70 diggable holes, some empty (the Unmarked Grave, the mass graves). Same reason.
- **Books and sketches:** 34 skill books, 36 recipes and sketches, 42 lore books in fixed item slots. Could show skill books only while unread.
- **Keys:** the level data ties 91 keys to 70 doors and 49 chests. Marking what a carried key opens adds no clutter; lock state needs RE.
- **Dice and badges:** 8 badges and 11 dice in fixed spots.
- **Notes pointing at places:** the rest are quest-marked (Knight Konrad's diary, Bavor's travelogue, the Brada report, Hynek's letter) or hold no treasure (Feast of the Dead, A peculiar poem, Thieves' notes, the Old Kutna mine knight, the Troskowitz Chronicle).
- **Not worth it:** easter eggs (no loot, no seen state), shrines (the game marks them), nests (about 500, random and respawning), hunting and fishing spots, herb gardens, caves.
