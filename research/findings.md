# Treasure maps in KCD2 1.5.6: where each one leads

Research for the treasure-finder KCSE mod. Game build: Steam 1.5.6 (release_1_5-15693). Full per-map data, including sources (URLs), alternates and notes, is in [treasure_maps.json](treasure_maps.json).

## How a map is tied to its destination

- A map is an item in `Tables.pak → Libs/Tables/item/item.xml` (`<Document Type="4">`). It carries only images and text, never a location.
- Some maps are linked through quest data: a quest graph (`Scripts.pak → Quests/Final/...`) holds the map's GUID and declares a `StashAsset`, and a `SmartObjectHolder` entity in the level binds that asset to a real `Stash` entity (`asset['chest'] → EntityId`). Example: Vidlak bandit's map → Stash 15785.
- The rest are linked only by the picture. For those, the destination was found from guides and matched to the level entity by loot table, layer and position.
- `poi_X_map` names say where the map is **found**, not where it leads.
- Coordinates are world metres, X east, Y north (checked against known places).

## Summary

- 38 map items: 26 treasure, 3 area, 6 quest (the quest already marks the way), 3 not location maps.
- 27 verified (data and guides agree), 10 likely, 1 unresolved.
- Every destination and source entity ID was checked against the extracted level data and lies within 2 m of the reported position.

## Notable findings

- **Brunswick's maps are off by one.** Map I leads to the 2nd armour stash, II to the 3rd, III to the 4th, IV to the 5th (entity 9534). Maps I and II are both given when you take the first piece; that first stash (11107, needs a shovel) has no map. The quest shows no map markers for these.
- **Krizhan's and the Bandit's treasure maps are forgeries.** Both lead to three empty holes at the treasure hunters' camp near Lorec. After the quest, copies of the Bandit's map are added to five other NPCs.
- **Map left by the deceased** (found in the Troskowitz mortuary chest) leads to a stash under stones near Zhelejov (45592), not to the cemetery.
- **Darko's map** spawns only in the Kuttenberg region and leads to a wolf-den chest there (17374).
- **Treasure maps 1 and 2** lead to ordinary POI loot (Cave under Trosky chest, skeleton's sack below the Devil's Finger), so the treasure can be looted without the map.
- **Mysterious map** (Buried Tomorrow) marks the ambush crossroads, not the heirloom.
- **Two maps seem unobtainable in 1.5.6:** Map hidden at Vostatek and Miners' Underground Map exist only in `item.xml` (no loot table, trader, slot or quest).
- Destinations that are not chests: Map from chest by Sedletz (loose items in a locked shed), Map of Kuttenberg underground (Goatskin's barrel trigger), the area maps (hunting spots).

## Table

Rows grouped by category. Level / entity / pos = destination entity id and its X,Y.

| Map (in game) | Item | Category | Leads to | Level / entity / pos | Conf. | Obtained from |
|---|---|---|---|---|---|---|
| Bandit's treasure map | `map_u49_mapakpokladu_bandit` | treasure | Identical copy of Krizhan's forged map: the same abandoned mine at the treasure hunters' camp near Lorec, w... | kutnohorsko / 952782 / 3788,1147 | verified | Carried by the treasure-hunter (bandit) leader at the camp/mine near Lorec. He hands it... |
| Bohuta's map | `sedmStatecnychDva_borutsMap` | treasure | Bohuta's buried equipment: dig spot inside a broken fence under a cluster of trees in the fields SW of Psch... | kutnohorsko / 65169 / 2068,1925 | verified | Carried by Bohuta (Borut) in Grund once The Devil's Pack starts. If you agree to pray w... |
| Darko's map | `taboryUCesty_darkosTreasureMap` | treasure | Kuttenberg region, Wolf Hills forest east of Sigismund's camp, at a 'Wolf hunting spot' den: a locked chest... | kutnohorsko / 17374 / 1997,2591 | verified | Black Darko, a Hungarian/Cuman mercenary in the 'Camp by the road' random event (tabory... |
| Footpad's hideout map | `nemcuvPoklad_mapStashLoot_1` | treasure | Sunken locked chest in the marsh under a birch tree south-east of Kuttenberg, near Sedletz. The key is in a... | kutnohorsko / 99806 / 3421,269 | verified | Hidden compartment behind a breakable brick wall in the cellar of the All Saints tavern... |
| Krizhan's treasure map | `map_u49_mapakpokladu` | treasure | Abandoned mine shaft at the treasure hunters' (bandit) camp near Lorec, north-east of Kuttenberg toward Sed... | kutnohorsko / 952782 / 3788,1147 | verified | Bought from hired hand Krizhan in Pschitoky (about 100 groschen), or stolen from him or... |
| Map from chest by Sedletz | `poi_mnichovaSchovka_map` | treasure | 'Love nest' (Hnizdecko lasky): a small locked drying shed across the pond west of Sedletz monastery. The lo... | kutnohorsko / 106110 / 3532,565 | verified | Chest (inventory_poi_prchajiciMnich_chest) at the foot of the Sedletz monastery outer w... |
| Map of Kuttenberg underground  | `andelStrazny_underground_map` | treasure | Goatskin's (Kozina's) hideout in the Kuttenberg underground on the east side of the city, below Leflirska s... | kutnohorsko / 139871 / 3233,686 | verified | Sewn into the coat of the hanged thief Angel at the Kuttenberg gallows/execution site s... |
| Old hideout map | `nemcuvPoklad_mapStashLoot_2` | treasure | Buried sack in a fresh grave under a two-forked tree, in the forest north-east of Bohunovice near Opatovice... | kutnohorsko / 140292 / 2197,3281 | verified | Same hidden wall compartment in the All Saints tavern cellar, Kuttenberg ('The German's... |
| Smugglers's map | `poi_tretiKrizovatkaVPodzemi_smugglersMap` | treasure | 'Locked dugout' north of Old Kutna, between Old Kutna and Grund: a smugglers' storehouse behind a medium-lo... | kutnohorsko / 25805 / 3099,1632 | verified | Carried by the dead smuggler kkut_deadSmuggler_2 (preset inventory_kkut_deadSmuggler_2)... |
| Storage Yard Plan | `dlc2_mrtvyNemluvi_mapStash` | treasure | Legacy of the Forge guild job 'In the Service of the Guild' (A203 mrtviNemluvi): the stolen guild steel is ... | kutnohorsko / 108724 / 675,1110 | verified | Carried by the dying bandit (Ondrej) near the dead bandits' camp in the forest west of ... |
| Treasure Map - Fourth | `treasureHunter_map4` | treasure | Chest in the wooded stream bed south of the Millstone Quarry (south of Bylany, NE of Maleshov). From the qu... | kutnohorsko / 113020 / 1030,409 | verified | Carried by the wandering treasure hunter (soul pocestny_treasureHunter_4) in the 'Seeki... |
| Treasure Map - Third | `treasureHunter_map3` | treasure | Kuttenberg underground: enter through the ruined house by the road and river south of the city. Inside, tak... | kutnohorsko / 56766 / 2835,835 | verified | Carried by the wandering treasure hunter (soul pocestny_treasureHunter_3) in the 'Seeki... |
| Brunswick's map I | `map_bruncvik_01` | treasure | Second Brunswick piece (plate legs): a sack behind the two small trees above the wolf den ('Vlci doupe') no... | trosecko / 36209 / 1928,3000 | verified | Granted by the quest (CreatePlayerReward) when the player takes the first armour piece ... |
| Brunswick's map II | `map_bruncvik_02` | treasure | Third Brunswick piece (plate arms and gauntlets): a sack at the large cliffs with three crosses ('Krize na ... | trosecko / 15711 / 1308,2771 | verified | Granted by the quest (CreatePlayerReward) when the player takes the first armour piece ... |
| Brunswick's map III | `map_bruncvik_03` | treasure | Fourth Brunswick piece (cuirass): a cloth bag next to a skeleton leaning against a cross at the foot of a t... | trosecko / 11970 / 1651,2333 | verified | Granted by the quest when the player takes the plate legs from the second stash (asset ... |
| Brunswick's map IV | `map_bruncvik_04` | treasure | Fifth and final Brunswick piece (helmet/bascinet): a small stone cairn / burial mound with a stone cross at... | trosecko / 9534 / 1285,1977 | verified | Granted by the quest when the player takes the plate arms/gauntlets from the third stas... |
| Caught thief's map | `poi_MapkaZlodeje_map` | treasure | Thief's camp in the woods on the cliffs east of Nebakov Mill. Locked chest just south-east of the camp cook... | trosecko / 21924 / 2192,1329 | verified | Black Bartosch at Trosky Castle (dice tables by the combat arena, west side of the cast... |
| Drowner's map | `poi_vodnik_treasureMap` | treasure | Water Goblin's treasure: an urn against a rock face on the shore of Rocktower Pond, between the Nomad camp ... | trosecko / 29186 / 1225,2085 | verified | Sack on the pier at the Bathing Spot where the drowned treasure hunters lie ('utopenci'... |
| Hired hand Ventza's map | `zbranePanaSemina_vasekMap` | treasure | 'Tunnel in the cliffs' south of the Apollonia bandit camp. Follow the exposed white rock to two caves. In t... | trosecko / 32801 / 2804,2187 | verified | Ventza (soul alias 'pacholek'), the hired hand in side quest 'The Jaunt' (zbranePanaSem... |
| Lower Semine woodcutters' map  | `poi_podseminstiDrevorubci_map` | treasure | 'Fresh clearing' NW of the Lower Semine woodcutters' camp: the woodcutters' pay chest (iron chest, very har... | trosecko / 29527 / 919,1409 | verified | Locked chest in a hut at the Lower Semine woodcutters' camp (south of Lower Semine mill... |
| Map hidden at Vostatek | `poi_skryseSPitim_map` | treasure | Gamekeeper Vostatek's five hidden booze stashes (bottles of spirits) in the woods between Kerchak Pond and ... | trosecko / 37251 / 1387,2390 | likely | No in-game source found: the GUID and item name occur only in item.xml (no preset, Item... |
| Map left by the deceased | `poi_troskovickyHrbitov_map` | treasure | 'Dead man's buried treasure' NW of the Zhelejov Wagoners' Inn and SW of the bandit camp at Kerchak Pond: a ... | trosecko / 45592 / 1532,2341 | verified | Locked chest (difficulty 0.7, opened with the Mortuary chest key) in the mortuary/chape... |
| Map of the Zhelejov marshes | `poi_kostlivecVMocalech_map` | treasure | Zhelejov marshes (west of Zhelejov / the Wagoners' Inn). The treasure is not on the islands: it is at the f... | trosecko / 39834 / 1590,2131 | verified | Lying in the hand of the skeleton in the Zhelejov marshes (POI kostlivecVMokradech_poi ... |
| Treasure map - First | `treasureHunter_map1` | treasure | Locked chest in the cave system under Trosky Castle ('Cave under Trosky' POI). Guides call it the Cave in A... | trosecko / 44514 / 2618,2670 | likely | Carried by the wandering treasure hunter (soul pocestny_treasureHunter_1) in the 'Seeki... |
| Treasure map - Second | `treasureHunter_map2` | treasure | A sack beside a skeleton on a rock ledge in the ravine below the Devil's Finger rock pinnacle, north of Roc... | trosecko / 33800 / 967,2158 | likely | Carried by the wandering treasure hunter (soul pocestny_treasureHunter_2) in the 'Seeki... |
| Vidlak bandit's map | `poi_vyplavenyPoklad_map` | treasure | Washed-up chest, half-submerged in reeds on the NW shore of a shallow pond SW of the 'Hollow tree' landmark... | trosecko / 15785 / 1546,3002 | verified | Carried by the leader of the bandit camp at Bukovina above Vidlak Pond (NW of Tachov), ... |
| Miners' Underground Map | `kuttenberg_underground_map` | area | General map of the Kuttenberg underground tunnels (the miners' own map). Not tied to a container. | kutnohorsko / — / 2862,707 | unresolved | No in-game source found: it is not in any loot preset, trader table, level entity or qu... |
| Poacher's map | `hunting_map_suchdol` | area | Area map marking about 4-5 hunting grounds (animal spawn spots) around Suchdol and the Ratbor poachers' cam... | kutnohorsko / — / 752,3760 | likely | Medium-lock chest in the ruined building of the poachers' camp near Ratbor, north-west ... |
| Map of game near Slatego  | `poi_nedostavenaChalupa_huntingSpotsMap` | area | Hunting grounds around Slatejov. The guide follows it SE from the unfinished cottage along the rock formati... | trosecko / — / 2937,1805 | likely | Locked chest (difficulty 0.6) under an awning at the unfinished cottage near Slatejov (... |
| Crypt map | `nakaza_mapOfCrypt` | quest | Plan of the chapel crypt under the Sedletz church. It is found inside the crypt and is a navigation aid for... | klaster / 382 / 1372,1047 | likely | Picked up inside the crypt while searching it in 'Silent Witnesses' (ItemSlot nakaza_cr... |
| Old cellar plans | `puvodNemoci_planOfBasement` | quest | Quest 'Anamnesis': old plans of the monastery cellars where translator Jaroslav went at night (down the sta... | klaster / 3716 / 1287,1024 | likely | In translator Jaroslav's chest in his cell on the top floor of the monks' dormitory (me... |
| Maleshov fort map | `zachranaPtacka_malesovExterior` | quest | Main quest 'Taking French Leave': sketch of Maleshov fortress (Capon held in the main tower). Approach is v... | kutnohorsko / 85482 / 505,388 | likely | Rosa Ruthard hands over both Maleshov sketches at the Ruthard house in Kuttenberg (Crea... |
| Map of Maleshov tower | `zachranaPtacka_malesovInterior` | quest | Interior of the main tower at Maleshov fortress; Hans Capon (and Brabant) are locked on the top floor, Rosa... | kutnohorsko / 42424 / 460,374 | likely | Given together with the fort map by Rosa Ruthard in Kuttenberg. |
| Map to the secret mint | `kralovskeStribro_mapToSecretMint` | quest | The secret mint south-east of Grund (Sukov woods). A hole with a ladder (the secret adit) leads down a tunn... | kutnohorsko / 40419 / 3543,1790 | verified | Carried by smelter Volkmar (Vokrak) in Grund during Via Argentum; you get it from him a... |
| Mysterious map | `zakopanyZitrek_mysteriousMap` | quest | NOT the heirloom treasure. Drat's charcoal map marks the ambush site: the crossroads in the forest SW of By... | kutnohorsko / 60522 / 871,1330 | verified | Placed by the quest (AddQuestItem StartingLocation=dratStash) in Drat's chest at the fa... |
| Boleslav Bavor's coat of arms sketch | `artefakt_BavorCoatofArmsDrawing` | not_a_location_map | Reference sketch of Bavor's arms (arrow symbol), used to recognise his mural in the church (3rd from the ri... | klaster / — / — | verified | Given by the monastery librarian Brother Crispin in the library after you discuss your ... |
| Filthy parchment from the crypt | `nakaza_dirtyPageFromCrypt` | not_a_location_map | Drawing of a Jerusalem cross (images jerusalem_1/2, quest constant nakres_erbu), a clue rather than a map. ... | klaster / — / — | verified | Lying on the altar in the monastery crypt during 'Silent Witnesses' (ItemSlot nakaza_co... |
| Diagrams of mounted archery tracks | `hadkaKonaru_archeryTracks` | not_a_location_map | Not a treasure or area map. It is a set of diagrams of the mounted-archery contest courses Bibrek builds (H... | trosecko / 181390 / 1092,1709 | likely | Given by Bibrek in dialogue during the horse-archery contest setup (CreatePlayerReward ... |

## Open items

- Likely, not verified: Treasure maps 1 and 2 (matched on loot only), Map hidden at Vostatek (destination inferred from image names; map seems unobtainable), Poacher's map and Map of game near Slatego (which hunting spots the sketch marks), the two Maleshov sketches, Crypt map and Old cellar plans (quest maps; exact target is a judgement call), Diagrams of mounted archery tracks (not a location map).
- Unresolved: Miners' Underground Map (no source, no target).
- Map sources with no fixed position: the four treasure hunter maps and Darko's map (random road encounters), Ventza's map (quest NPC), and the Bandit's map copies handed to NPCs after X Marks the Spot.
- Nothing here has been checked in the game itself.
