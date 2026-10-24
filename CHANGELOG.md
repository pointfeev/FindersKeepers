# Changelog

Each version's section becomes its GitHub release notes and its Nexus Mods changelog, where every line is one entry, so keep each entry on one line.

## 1.1.1

- With Only maps you carry (`RequireCarried`) on, putting a read map away now clears all of its marks, so the mod page's workaround for keeping refilling chests off the map works: its location used to be marked again once its source refilled a copy of the map.
- Keep finished places is now Keep marks once found (`KeepFound`), and also keeps a found map's location marked while a copy lies there. A settings file with the old name carries over.

## 1.1.0

- Some loose items in the world never got a mark or an in-world icon; they now do.
- The Map from chest by Sedletz now marks the book in the Love nest, and the Map of game near Slatego the bow on its hunting ground.
- The Key from Below Trosky is now marked beside the first treasure map's chest, and the Map of the Zhelejov marshes where it lies.
- A chest now clears once you take any of its loot, rather than only once it is empty, so junk or loot you leave behind no longer keeps it marked.
- A chest holding quest items that never come back (Brunswick's armour, the Storage Yard's red gold, asphalt and axe recipe, the warlock's books and recipes) clears once you take those, and stays clear when the rest of it refills.
- The mod page now explains that chests which refill about a week after you loot them are marked again once their loot is back.
- A mark now steps aside only for its own quest's marker on the same spot, never for a search area, so the Drowner's urn no longer hides under The Axe from the Lake's.

## 1.0.1

- Goatskin's barrel (Map of Kuttenberg underground) is marked only while it gives Master Schindel's items, from Goatskin's step until you take them.
- Black Bartosch's map location follows Bartosch instead of his dice table, and clears once he no longer has the map.
- A chest or NPC that no longer has its map no longer shows a map location.
- Keep finished spots no longer keeps a quest's interaction spot once it has been used.

## 1.0.0

First release.

- Marks where every treasure map, quest map and riddle note leads, and where each one is found, on the world map, the compass and in the world.
- A mark clears once its spot is dug, emptied or looted, and steps aside while a tracked quest marker points there.
- Settings live in an INI file, or in game with Mod Configuration Menu.
