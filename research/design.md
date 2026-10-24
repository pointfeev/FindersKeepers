# Design

How the mod decides what to mark, the options it will offer, and what was considered and left out. Per-map research is in [findings.md](findings.md).

## Scope

Treasure and what leads to it: treasure maps, quest maps, the carved map in the Tunnel in the Cliffs, and riddle notes (the Strange Little Verses, the Strange Riddle).

## Rules

### State

The mod stores nothing but its options. Every mark is worked out from game state at each probe.

### Map destinations

Mark type `FindersKeepersMapDestination`.

- A map's destinations are marked once the player has read it, carried or not: the game keeps reading progress per item class, and it survives the map being dropped and a reload (tested in game). A carving counts once examined, and only after Laboratores (`socky`): its trigger is off until that quest ends, which would read as examined.
- A map that leads to several places marks each one.
- A destination clears when its hole is dug, its item is taken, or its container's treasure is taken. A container's treasure items are listed in `treasure_maps.json` with a rule. Most clear once any of them is gone (`any_taken`): the list is exactly the items its loot preset always gives, worth more than nothing, so a missing one means the player found the place and took something; what they leave, and junk rolled on top, does not matter. Where a quest adds items that never refill (`DisableRestock`: Brunswick's armour, the Storage Yard's red gold, asphalt and axe recipe, the warlock's books), those alone are the treasure and the place clears once all of them are gone (`all_taken`), for good. A container with no list clears once empty. Value judgements ("is what is left worth a visit") are left to the worth filter under Set aside. A quest's interaction trigger is marked only while it takes a press: from the quest step that switches it on until it is used. `KeepFound` does not keep a used one, because it looks the same as one whose step is not reached yet.
- A riddle note's destination also clears once the player has, or has read, the next note.

### Map locations

Mark type `FindersKeepersMapLocation`. Where a map is found, marked while the player neither carries it nor has read it, its quest has not ended, and the container, NPC or item slot there still holds it, the same check as a destination's treasure with the map as the one item. The carving's location shows while its trigger takes a press, so not before Laboratores. A map read and put down stays found under `RequireCarried` too: its source refills a copy like any other loot, so marking where a copy lies would bring the mark back about a week after every visit. With `KeepFound` it stays marked while a copy lies there.

### Quests

- A map tied to a quest stops being marked once the quest is done or failed. Places flagged `after_quest` are marked only from then on (the secret mint's ledger chest after Via Argentum).
- A destination hides while a tracked point marker of its map's own quest is within `QuestMarkerReach` of it. Search areas never hide one, not even its own quest's: an area says where to search, the mark says where to dig. Other quests' markers point at their own business (The Axe from the Lake's area covers the Drowner's urn).

### Display

- Tooltips read "Map destination: <map name>" or "Map location: <map name>" in each language, with its own punctuation; the legend and the settings use the same two names.
- On the map our marks stack above every other POI, the quest giver's new-quest mark included (priority 0; the map stacks by priority alone, and QuestGiver, without a `poi_type` row, has 4). The compass is left as the game draws it: quest markers in layers above all POIs, and POIs, ours among them, nearest on top.
- Each marked place within range gets its own in-world icon over its position, its bottom edge `IconHeightAboveTop` above the top of what it draws (its render box), or above its position when it draws nothing (a trigger, an item slot). Anchoring the bottom edge keeps the icon clear of the object however large it gets on screen, and only the box's height is used, since its middle sways with a walking person's limbs. It follows the object every frame, so it keeps up with a person carrying a map. Between the fade distance and the edge of its range it fades out. Its size is `IconSize` within 5 m and beyond shrinks at every distance with a perspective-like curve (1 / (1 + k * beyond)), down to `IconFarScale` (30% by default) at the edge of its range. At one size an icon looks massive over a small, far object, while true perspective would leave far icons unreadable.
- Icons hide while the game's cutscene player plays anything and while the player is held in a conversation, not while someone passing by says a line. Both come from the game's own state (the GUI module's cutscene player; the dialogue manager, asked the way actors ask it of themselves), not from what the HUD happens to show, which many other things change.

### Not possible under the state rule

An unread map that leaves the inventory. Finding it again would mean remembering when it left, or searching every inventory and dropped item.

A refilled destination. Most treasure containers and loose items refill 7 days after they are taken (18 marked destinations: 15 containers and 3 item slots; see findings, Restocking), and the game keeps no lasting record that one was looted. A container's save record holds its inventory as a difference from its loot preset, its lock state, its restock state, its quest item links and its phase model (Ghidra, StashSaveRecord). The preset is generated from a seed tied to the container, and a refill gives each item back its original id. So once the container is full again, its difference is empty and its restock record is reset: in the save it is the same as one never touched, and it is marked again. Under `any_taken` it stays clear until the refill completes.

Two traces outlast a refill, but neither covers enough to clear a place by:
- A container locked in the level stays unlocked once picked or opened with a key; only quests and the tutorial chest relock one. Unlocked containers have no such trace, and opening is not looting.
- A container whose phase changes when the player takes an item (`esStashPhaseChangeEvent` PlayerTakesItem or StashIsEmpty) keeps its emptied model through a refill. Few containers have one.

A region switch's save (LevelSwitchSave) drops refilling containers whole, so both traces are gone once the player changes region. Nothing else records a looted container: the game's statistics keep unique sets for documents and dice opponents but not containers, and the AI logs looting only as a crime at an owned container.

Keeping a place clear therefore means storing that it was looted, and every place to store it but the first has a catch:
- A free mark type's name (best): the map's save writes every mark type's name as a string of any length, and its load passes each saved name through the name mapper, which the plugin already detours. Name a free id with the list of found places and the list rides every save, while a game without the mod ignores the unknown name. The game asks for a free id's name only in that save loop. Open: a save without the list, and a new game, must start with an empty list. The map's filter bytes, which the game resets on load and when it builds the map, can anchor that; a test build settles it.
- The map's read progress cannot hold it: maps are not timed documents, so the game keeps their read state as a GUID set, not a number (Ghidra, C_RPGMinigames). Adding made-up GUIDs to that set risks a crash in any code that looks them up as item classes.
- A field on the container's script needs the game's stash scripts replaced, as Loot Info does, which clashes with Loot Info and with patches to those scripts.
- Loot Info's own `bSearchedAllready` (set on opening, saved per container) would give back-progress, but couples the mod to another mod's internals, already renamed once.
- A file of our own per save drifts when an older save is loaded.
- A token item in the inventory, as Loot Info stores its settings, is visible clutter that can be sold, dropped or confiscated.
- A hidden permanent buff per place: a save holding a buff whose definition is gone (the mod removed) loads it without one, and whether the game survives that is untraced.
- A quest of our own: a new quest must be wired into a vanilla parent graph, and setting its variables from C++ needs much more RE.
- Spare map legend flags: 12 mark type ids are unnamed (33, 35, 40, 41, 54, 62-68), each saved as a raw byte under the name we give it and skipped once unknown, so three hold the 18 places. The legend's Track all fills every flag byte, so it needs a hook that keeps ours.
- The player's `AI` script table: `Player:OnSaveAI` and `OnLoadAI` save and restore it whole, and no vanilla script reads keys inside it, so a `player.AI.FindersKeepers` table would ride the save, invisible, and be ignored without the mod. Untested: that KCD2's save reaches this CryEngine AI script path for the player, and that nested tables survive. A throwaway build that writes a value, then reads it after a save and reload, settles it.
- A discovered POI only says the player was near.

If this comes back, the condition is the one marks already use: a place counts as found the first time the probe sees it done (treasure taken, hole dug, slot emptied), and it stays clear from then on, refill or not, for every place alike. Places looted before then, or without the mod running, can't be known and stay marked until looted again.

## Options

Settings live in `Mods/finderskeepers/KCSE/Plugins/FindersKeepers.ini` (`plugin/src/options`), the convention other KCSE plugins use. The mod does not ship it: the plugin writes it with the defaults when missing, re-reads it when it changes on disk, and adds keys a new version brings. The deploy step keeps it. The map legend's Track toggle per mark type still applies on top.

[Mod Configuration Menu](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3363) (MCM), when installed, shows the same settings in the pause menu. It stores nothing. The plugin hears its "build settings" and "value changed" messages through KCSE's messaging interface and writes the INI. Without MCM the messages never come, and the INI alone holds the settings. MCM has no controller support. Its headers are GPL-3.0, so the plugin declares the few interfaces it calls itself.

KCSE needs the Address Library for KCSE, according to its Nexus page. The library covers 1.5.6 only, not Game Pass. So the mod as a whole waits on it after each game update, even though the plugin finds its own game code by pattern.

| Section   | Key                     | Default | Range   |
|-----------|-------------------------|---------|---------|
| Display      | `MapMarkers`            | on      |         |
| Display      | `FloatingIcons`         | on      |         |
| Marks        | `MapDestinations`       | on      |         |
| Marks        | `MapLocations`          | on      |         |
| Maps         | `TreasureMaps`, `QuestMaps`, `RiddleNotes` (each covers both marks) | on | |
| Rules        | `RequireRead`           | on      |         |
| Rules        | `RequireCarried`        | off     |         |
| Rules        | `KeepFound`             | off     |         |
| Rules        | `RetireQuestMaps`       | on      |         |
| Rules        | `HideUnderQuestMarkers` | on      |         |
| Distances | `IconShowDistance`      | 40 m    | 10-150  |
| Distances | `IconFullDistance`      | 25 m    | 0-150   |
| Distances | `IconSize`              | 48 px   | 16-128  |
| Distances | `IconFarScale`          | 30%     | 10-100  |
| Distances | `IconHeightAboveTop`    | 0.2 m   | 0-3     |
| Distances | `QuestMarkerReach`      | 15 m    | 0-50    |

Map and compass markers come from one game entity mark (`RegisterEntityMark` builds the compass marker), so they share one switch; separating them needs RE. An entity with no AI object gets its mark through a stand-in ([findings](findings.md#game-rules)). The compass range is the game's. Labels are English only; MCM can show localised `@` keys if they are added to the localisation table.

### Icons

Treasure uses the game's debt-notes chest icon and map locations the map book; both read well enough for now. Custom icons are feasible without new tools (our own BC7 encoder writing into a game icon's DDS header, with undiscovered and shadow variants generated), and mock-ups exist (a wax seal with an X and one with a folded map was the favourite). Revisit only if more mark types are added and need a matching set.

### Track toggle

Settings act first (what is created at all) and the map legend's Track toggle second (whether a type shows), so a mark needs both. With map markers off the legend lists none of our types, so the in-world icons then ignore Track.

## Extending

- Another map or note of an existing kind is a row in `treasure_maps.json`, with each place's kind (what the probe reads from it, by its entity class) and the treasure items and rule of each container it leads to. The build generates, validates and localises it; `validate` checks each kind against the entity's class, every treasure item against what the game puts in that container, and an `any_taken` list against the loot preset's guaranteed items.
- A new category needs the generator's category list, `MapTable::Category`, a settings entry (the INI and MCM follow) and a case in the probe's category check.
- A new mark type, with its own icon and legend entry, needs an entry in `MarkType::kTypes` (naming, importance and tooltips already loop over it), its icons in `game_files.json` and its legend string. Three places still assume exactly two types: `ProbeResult::Target` has `bool source`, `GameMark` picks the type from it, and `plugin.cpp` keeps two entity lists and two icon calls. Before the next type, give `Target` a `const MarkType::Type *` and loop over `kTypes` there. It's a small change, and after it a type is just data. In-world icons are neither merged nor sorted: a place two maps lead to (the two u49 maps' dig sites, the wall stash holding both hideout maps) draws two identical icons on one spot, and a far icon can draw over a near one. Harmless with two types; with more, draw one icon per object and type, nearest on top.
- A source that is not a map (stashes, keys) doesn't fit yet, since the probe only walks the map table. Before the first one, split the probe into providers that each return targets, with the map table as one of them. Not before: one provider does not need the split.

## Surviving game updates

What the build bakes in from `treasure_maps.json` and the game files: each map's item GUID, its destinations and source by level GUID and kind, quest names, the carving's trigger, the next riddle note, categories, treasure items and rules, localised map names and the icons. What the plugin reads live at every probe: the entities behind those GUIDs, what the player carries and has read, quest progress, whether a hole is dug, a slot holds its item or a container its treasure, whether a quest trigger takes a press, positions, WUIDs and quest markers. Its hold on the game code is about 56 signature patterns and 30 struct offsets and vtable slots, found by pattern at start-up.

A patch can break the signatures and offsets (the log says the plugin did not start; KCSE and its Address Library have to update first anyway), or change level entities, quests, map items and loot (`uv run treasure-data validate` reports each against the new build, so run it after every patch; it needs the installed game, so not in CI). The game-state logic rarely breaks.

Decision: keep the data compiled. A patch needs a check and a release either way, and `validate` turns every data change into a report in seconds, while code that reads the same data from the game at runtime is more RE that a patch can break silently. The one exception is the entity lookup: places are found by their level GUID (`EntitySystem::FindIdByGuid`, entity system slot 47), which saves rely on and patches leave alone, so a renumbered entity keeps its mark without a release; `validate` still checks every GUID, also across levels. Considered and kept compiled:
- `any_taken` treasure from the loot preset at runtime: needs RE on the preset database and the game's preset rules rebuilt in C++, for tables that rarely change and that `validate` checks exactly. The same goes for a riddle note's `reveals`.
- `all_taken` quest items from the stash's default items: needs quest-system internals for six chests.
- Tooltips composed from the game's localised map names: needs the localisation lookup, for a cosmetic gain (a renamed map shows its new name before the next release).
- A container's refill period, for a `refills` flag: only if a looted record is ever stored, and then a plain script read either way.
- Which map leads where, map sources, quest names, the carving's trigger and its quest gate, `after_quest` places: research results no probe can read. The icons are copies of game textures; a patch can only make them stale, not break them.

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
