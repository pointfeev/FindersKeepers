# Treasure maps in KCD2: research notes

Game build: Steam 1.5.6 (`release_1_5-15693`). Per-map data (entity ids, positions, sources) is in [treasure_maps.json](treasure_maps.json); this file keeps what we learned about each map and the game rules the mod relies on. Below, "data" is the extracted game files and "guides" are the linked walkthroughs.

## How a map ties to a place

- A map is an item in `item.xml` (`<Document Type="4">`) with images and text, never a location.
- Some maps are tied through quest data: the quest graph names the map's GUID and a `StashAsset`, and a `SmartObjectHolder` in the level binds that asset to a `Stash` entity. The rest are tied only by the picture; for those, guides gave the place and loot table, layer and position matched it to an entity.
- `poi_X_map` names say where a map is found, not where it leads.
- Coordinates are world metres, X east, Y north.

## Game rules

- **Quest markers:** only tracked quests (three at most) put markers on the map and compass ([TheGamer](https://www.thegamer.com/kingdom-come-deliverance-2-find-side-quests-tipsters-how-to/)). The game's own gather (0x180DC5E68) matches: tracked quests, plus every quest while the quest log page is open. Areas are yellow search areas of unstated size; treasure maps never get a quest marker.
- **Stash loot:** a chest gets its preset loot when the game registers it, or when a save is loaded, never on opening (Ghidra, C_Stash). Opening only records the robbed value the owners' AI reads (`GetStashRobbedValue`); nothing records that the player opened it.
- **NPC loot:** a soul's inventory is filled from its presets once, when the soul is initialized or reset, guarded by a flag in its data (Ghidra, C_InventorySoul vtable slot 2); getting the inventory never fills it. A spawned NPC therefore carries its items, and an empty inventory has been stripped. Quests add or move items on top (`AddQuestItem`, `AddSoulDefaultItem`) while their node is active.
- **Restocking:** stashes, souls, shops and item slots restock on a schedule (`FullyRestock`, `wh_ent_RestockStartTimeMinutes`). A stash or item slot restocks its preset some days after its items are taken (`nRestockPeriodDays`, 7 by default in AnimStash.lua and ItemSlot.lua, 0 disables). Of the destination chests, Brunswick II-IV, the Zhelejov marsh purse, the woodcutters' pay chest, the smugglers' dugout, the deceased's stash, Boruta's chest and the carving's chest and sack set 0; the rest keep 7. Since 1.5, "Chests no longer restock themselves if all their owners are dead" ([patch notes](https://kingdomcomedeliverance2.wiki.fextralife.com/Patch_Notes)); whether that stops ownerless chests is unconfirmed. Marks follow the contents, so a restocked chest is marked again.
- **Holes:** stay dug since 1.3 ("Holes dug by the player now stay dug out after level switch").
- **Loose items:** an item slot restocks like a stash (`nRestockPeriodDays`, 7 by default; multi-class slots never). Slots also spawn and despawn their item's entity with distance (`wh_item_ItemSlotDynSpawning*`), but the slot keeps its item while despawned: the despawn removes only the item's entity (Ghidra, C_ItemSlotDynamicSpawner). So the slot's item, which the mod reads like the quest check IsItemInSlot does, holds from any distance.
- **Quest items:** an `AddQuestItem` exists only while its quest state is active. Some quests hand over a normal copy when they end (`CreatePlayerReward` on an `_afterQuest` state).
- **Failure:** quests have a Failed state (patch notes). Main quests do not fail; losing a key NPC is a game over.
- **Open world:** `socky` (M03) is Laboratores (game string `socky_CpmD`); its end opens the Trosky region ([kcd2.org](https://kcd2.org/en/post/kcd2-walkthrough)).

## Maps

### Treasure maps

**Treasure map - First** (`treasureHunter_map1`)
- Found: the wandering treasure hunter in the "Seeking Man" road encounter (buy, steal or loot).
- Leads to: the locked chest in the cave under Trosky (223 groschen, 2 Frankfurt steel). Its key, "Key from Below Trosky", is in ItemSlot271 (44518) on a sack prop 7 m away ("a cracked skull on a sack"). The other sack in the room (44523) holds 18 groschen, shoes and a gambeson. [Patch Crazy](https://patchcrazy.co.uk/?p=7260), [kcd2.org](https://kcd2.org/en/post/kcd2-treasure-map)

**Treasure map - Second** (`treasureHunter_map2`)
- Found: as First.
- Leads to: the sack beside the skeleton in the ravine below the Devil's Finger (183 groschen, two silver rings, a silver chalice).
- The 281 groschen by the cross on top of the Devil's Finger (Stash19 33640) is a separate container guides call "not the main treasure"; one guide's "second sack nearby" is a doubled write-up of the skeleton's sack. [Patch Crazy](https://patchcrazy.co.uk/treasure-map-second-kcd2-treasure-location/)

**Treasure Map - Third** (`treasureHunter_map3`): as First; a small sack at a carved pillar in the Kuttenberg underground, entered through the ruined house south of the city.

**Treasure Map - Fourth** (`treasureHunter_map4`): as First; a chest in the stream bed south of the Millstone Quarry.

**Brunswick's maps I-IV** (`map_bruncvik_01..04`)
- Off by one: map I leads to the 2nd armour stash, II to the 3rd, III to the 4th, IV to the 5th. The 1st stash (11107, needs a spade) has no map.
- Found: quest rewards; I and II together when the first piece is taken, III and IV after the next pieces.
- III leads to the shrine (Stash27 11898) and the bag by the skeleton (11970). [PowerPyx](https://www.powerpyx.com/kingdom-come-deliverance-2-the-lions-crest-walkthrough/)
- The quest shows no map markers for these.

**Krizhan's and the Bandit's maps** (`map_u49_mapakpokladu`, `_bandit`), quest X Marks the Spot (`mapaKPokladu`)
- Forgeries: three empty dig spots in the mine at the treasure hunters' camp near Lorec (holes 952782-952784). Digging is optional; the quest moves on through the bandit leader. A fourth spot the bandits dig is possible. [Game-Checklists](https://game-checklists.com/kcd2/guides/x-marks-the-spot-quest/)
- Krizhan's is a quest item; after the quest, copies of the Bandit's map are added to five NPCs.
- Marks retire with the quest, since the holes may never be dug.

**Vidlak bandit's map** (`poi_vyplavenyPoklad_map`)
- Found: the Bukovina bandit leader, met in Lackey.
- Leads to: the hollow tree (a tin plate and silver cup at its roots, ItemSlot42 28484 and ItemSlot41 28485), then the washed-up chest (15785; about 300 groschen and a golden crucifix). [Selphie](https://selphie1999gaming.com/game-guides/kcd2-kingdom-come-deliverance-2/kingdom-come-deliverance-2-vidlak-bandits-map-treasure/)
- Only the chest is marked. In game (2026-10-20) the plate and cup had sunk into the stream bed and could not be picked up, so a mark on them would never clear; this may be why Gamer Guides says the treasure is "no longer here".

**Map from chest by Sedletz** (`poi_mnichovaSchovka_map`)
- Found: the chest at the foot of the monastery wall, which also holds the shed's key.
- Leads to: loose items (silver chalice, silver jug, Art of Demosthenes III, food) in the locked Love nest shed; no container. We mark ItemSlot198 (106110).

**Map of game near Slatego** (`poi_nedostavenaChalupa_huntingSpotsMap`)
- The sketch shows one red spot: the wolf hunting ground SE of the unfinished cottage, a skeleton with a hazel longbow (ItemSlot115 31712), maybe some arrows; no container. [GameRant](https://gamerant.com/kingdom-come-deliverance-2-kcd2-map-game-near-slatego-treasure-solution/)

**Map left by the deceased** (`poi_troskovickyHrbitov_map`): from the Troskowitz mortuary chest; a stash under stones NW of the Zhelejov Wagoners' Inn (45592), not the cemetery.

**Map of the Zhelejov marshes** (`poi_kostlivecVMocalech_map`): in the skeleton's hand; a purse and sword at the foot of a tree towards the rocks, not on the islands.

**Drowner's map** (`poi_vodnik_treasureMap`): from the sack on the Bathing Spot pier; the urn on the south shore of Rocktower Pond, 125 groschen. Only the ordinary "Water Goblin's Treasure" POI appears once close. [Gamer Guides](https://www.gamerguides.com/kingdom-come-deliverance-ii/guide/free-form-quests/trosky/drowners-map-water-goblins-treasure-walkthrough)

**Caught thief's map** (`poi_MapkaZlodeje_map`): Black Bartosch at Trosky (buy, dice or pickpocket); the thief's camp chest east of Nebakov Mill.

**Lower Semine woodcutters' map** (`poi_podseminstiDrevorubci_map`): the camp hut chest; the pay chest at the fresh clearing, key in a bird's nest.

**Hired hand Ventza's map** (`zbranePanaSemina_vasekMap`), quest The Jaunt: the sack in the western cave of the Tunnel in the Cliffs.

**Carving in the Tunnel in the Cliffs** (`tunelVeSkalach_carving`)
- Not an item: examined on the east cave wall (trigger 20483), one-shot; its prompt goes off once examined.
- Leads to: the "Hidden treasure" north of Trosky Castle, a chest (16919) and a sack (16775). [Gamer Guides](https://www.gamerguides.com/kingdom-come-deliverance-ii/guide/free-form-quests/trosky/apollonia-cave-wall-map-treasure-location-guide)
- The trigger is also off before the open world; we count it only after Laboratores. Whether the cave is reachable earlier is undocumented.

**Bohuta's map** (`sedmStatecnychDva_borutsMap`), The Devil's Pack: a buried armour set SW of Pschitoky (needs a spade). Guides say the map's roads are wrong.

**Darko's map** (`taboryUCesty_darkosTreasureMap`): Black Darko in a random roadside camp, Kuttenberg region only; a wolf-den chest in the Wolf Hills (17374).

**Footpad's hideout map, Old hideout map** (`nemcuvPoklad_mapStashLoot_1`, `_2`), The German's Treasure: both behind the tavern cellar wall; a sunken chest SE of Kuttenberg (key in a nest) and a buried sack NE of Bohunovice.

**Smugglers's map** (`poi_tretiKrizovatkaVPodzemi_smugglersMap`): on a dead smuggler in the Kuttenberg underground; the locked dugout north of Old Kutna.

**Map of Kuttenberg underground** (`andelStrazny_underground_map`)
- Found: sewn into hanged Angel's coat, or Striped Tonies' reward.
- Leads to: Goatskin's barrel (Master Schindel's stolen items), interaction trigger 139871. In Master Schindel's toys (`u66__andel_strazny`, `quest_itemy.xml`) it takes a press only from the step where Goatskin tells about the map until the items are collected, and only once; using it grants the items. The mark follows that switch, so it shows only while the barrel gives something. The quest marks no stash. [wotpack](https://wotpack.ru/en/igrushki-mastera-shindelya-v-kingdom-come-deliverance-2-gde-nayti-ukradennye-veschi-v-podzemnom-taynike/)

**Storage Yard Plan** (`dlc2_mrtvyNemluvi_mapStash`), Legacy of the Forge: from the dying bandit Ondrej; the steel chest in the Rabstein ruin.

### Quest maps

Their destinations are areas or triggers with no done state. Each retires when its quest ends (done or failed), except places flagged `after_quest`, which are marked only from then on. A destination is hidden while a tracked quest marker covers it.

**Map to the secret mint** (`kralovskeStribro_mapToSecretMint`), main quest Via Argentum
- Found: Volkmar in Grund, after the fight or from his body. A quest item; if carried when the quest ends, the game gives a normal copy.
- Leads to: the adit SE of Grund (40419), at the southern edge of the quest's search area. [PowerPyx](https://www.powerpyx.com/kingdom-come-deliverance-2-via-argentum-walkthrough/)
- The quest cannot fail. The mint's chests can be left behind after it. [Gamer Guides](https://www.gamerguides.com/kingdom-come-deliverance-ii/guide/main-quests/kuttenberg/via-argentum-quest-walkthrough)
- Once the quest ends, the mark moves from the entrance to the ledger chest (91882: 205 groschen, schnapps, shot recipes; locked, key from Buresh's men) and clears when it is emptied. The mint's other containers hold generic pantry and bedside loot.

**Mysterious map** (`zakopanyZitrek_mysteriousMap`), The Heirloom
- Found: the quest puts it in Drat's chest when the Drat route needs it. A quest item, returned as a normal copy when the quest ends.
- Leads to: the ambush crossroads SW of Bylany (60522), not the heirloom.
- No guide mentions the item; every guide gets the crossroads by talking to Drat. Data only. The quest can fail.

**Crypt map** (`nakaza_mapOfCrypt`), Silent Witnesses (Mysteria Ecclesiae)
- Found: on an open tomb to the right of the crypt entrance, apart from the parchment on the altar. The quest puts it in its slot when "search the crypt" starts and removes it when the quest ends. [Fextralife](https://kingdomcomedeliverance2.wiki.fextralife.com/Crypt+Map)
- Leads to: the crypt itself (382). Later quests return there with their own markers. No hard failure found.

**Old cellar plans** (`puvodNemoci_planOfBasement`), Anamnesis (Mysteria Ecclesiae)
- Found: Jaroslav's chest in his cell above the library; the quest's "check the translator's stand and chest" objective is that chest.
- Leads to: the monastery cellar (3716) and, by the wine footprints and scaffold, the hidden scriptorium under the library stairs. The quest marks the cellar only once it is the last objective. [PowerPyx](https://www.powerpyx.com/kingdom-come-deliverance-2-anamnesis-walkthrough/)
- Fails only with the whole DLC questline (serious crimes in the monastery).

**Maleshov fort map, Map of Maleshov tower** (`zachranaPtacka_malesovExterior`, `_malesovInterior`), Taking French Leave
- Found: Rosa hands both over in the "Předání plánků" module of Rosa's Book (`rozinaKniha`), as normal items.
- Leads to: the fortress (85482) and its main tower (42424); Capon is on the top floor and is not marked on the map. The quest's chapel step is a yellow search area. [Game-Checklists](https://game-checklists.com/kcd2/guides/taking-french-leave-quest/)
- No failure. The fortress returns in Dancing with the Devil, with its own markers.

### Riddle notes

Notes (Document Type 5), not maps, found by auditing every non-map document's text (2026-10-20). Each is marked like a map once read.

**Strange little verse I-V** (`andelStrazny_riddles`, `_2`..`_5`), Kuttenberg
- Verse I is in Goatskin's barrel (Master Schindel's Toys) and holds every verse's text. Each verse leads to a stash (preset `inventory_andelStraznyRiddle1`..`5`) holding the next: the fountain on Vintners' Street (81346), the north-wall scarecrow (103832), the stash the town-hall clock's shadow points at (92947), a nest under the Emperor Charles tavern roof (103896), and the last cache with Scholarship IV and Alchemy IV (103959). [wotpack](https://wotpack.ru/en/strannye-virshi-v-kingdom-come-deliverance-2-reshenie-vseh-zagadok/)
- No quest marks them. A verse's stash counts as done once the player has or has read the next verse (a nest may keep no inventory).

**Strange riddle** (`dlc2cernokneznik_letterCipher`), Legacy of the Forge activity `cernokneznik` (Warlock's Riddle)
- Found in the alchemist's room (item slot 49291); leads to his stash (946897), which exists only while the activity's room is streamed. The objective "solve the riddle" has no quest marker. Retires with the activity.

Other notes that point at places are quest-marked (Knight Konrad's diary, Bavor's travelogue, the Brada report, Hynek's letter) or lore only.

### Area maps

**Poacher's map** (`hunting_map_suchdol`): hunting grounds around Suchdol and Ratborsch; no place or loot. Not marked.

**Diagrams of mounted archery tracks** (`hadkaKonaru_archeryTracks`): Bibrek's horse-archery courses; no place or loot.

### Not location maps

**Filthy parchment from the crypt**, **Boleslav Bavor's coat of arms sketch**: quest clues, every step quest-marked.

### Unobtainable

**Map hidden at Vostatek** (`poi_skryseSPitim_map`): only in `item.xml`; no guide shows how to get it. Most likely shows Vostatek's five booze stashes in the Vidlak woods: one-shot interaction triggers (a monologue) from the open world on, `vostkovoPiti/triggery_na_mistech_s_pitim.xml`, so their marks would clear once used.

**Miners' Underground Map** (`kuttenberg_underground_map`): only in `item.xml`; no source, no target.

## Other mods

- **Minimap** ([Nexus 3330](https://www.nexusmods.com/kingdomcomedeliverance2/mods/3330)) marks buried treasure for maps you own, with options to keep, mark done or hide dug-up spots. No in-world icon, no area maps. Its decoy marker "stayed on the map forever" until v1.22.
- No other mod found marks treasure-map destinations.

## Open

- Whether an item slot keeps its item while its part of the level is not loaded (in-game test).
- Guide claims have not been checked in the game.
