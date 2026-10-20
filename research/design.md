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

## Options (planned, not built)

INI or MCM is undecided: it depends on how KCD2's MCM works and whether requiring it is acceptable. The map legend's Track toggle per mark type already works and is saved; options would sit under it.

| Option                                         | Default | Today                                 |
|------------------------------------------------|---------|---------------------------------------|
| World map markers                              | on      | always                                |
| Compass markers                                | on      | always (separate toggle needs RE)     |
| In-world icons                                 | on      | always                                |
| Map locations                                  | off     | always on                             |
| Require carrying the map                       | off     | never required                        |
| Require having read the map                    | on      | always                                |
| Keep finished destinations                     | off     | always removed                        |
| Per category (treasure, quest, area, riddle)   | on      | all                                   |
| Hide ours under a tracked quest marker         | on      | always                                |
| Retire quest maps when their quest ends        | on      | always                                |
| In-world icon shown within (m)                 | 40      | `kShowDistance`, FloatingIcon.cpp     |
| In-world icon fully opaque within (m)          | 25      | `kFullDistance`, FloatingIcon.cpp     |
| In-world icon size at 1080p (px)               | 48      | `kSizeAt1080`, FloatingIcon.cpp       |
| In-world icon height above the target (m)      | 1.2     | `kHeightAbove`, FloatingIcon.cpp      |
| Quest marker covers ours within (m)            | 15      | `kQuestMarkerReach`, Probe.cpp        |

**Icons.** Treasure uses the game's debt-notes chest icon and map locations the map book; both read well enough for now. Custom icons are feasible without new tools (our own BC7 encoder writing into a game icon's DDS header, with undiscovered and shadow variants generated), and mock-ups exist (a wax seal with an X and one with a folded map was the favourite). Revisit only if more mark types are added and need a matching set.

Map and compass markers come from one game entity mark (`RegisterEntityMark` builds the compass marker), so toggling them apart needs RE. The compass range is the game's.

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
