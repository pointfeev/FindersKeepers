#pragma once

#include <cstdint>
#include <span>

#include "game/Game.h"

// The maps the mod marks, generated at build time from research/treasure_maps.json (tools/treasure_data/maptable.py)
// into MapTableData.inc.
namespace MapTable {
    // A level entity: its EntityId and name (ids repeat across levels, so the name confirms the match); id 0 for none.
    struct Place {
        const char *name = nullptr;
        // A container's treasure items, empty for other places and for a container that counts as found only once
        // empty. With anyTaken, the container's guaranteed loot: it counts as found once any of it is gone. Without,
        // quest items that never refill: found once all of them are gone.
        std::span<const Game::ItemClassId> treasure = {};
        std::uint32_t entity = 0;
        bool afterQuest = false; // a destination marked only once the map's quest has ended, instead of until then
        bool anyTaken = false;
    };

    // The table's categories, each switched by its own option.
    enum class Category { Treasure, Quest, Riddle };

    struct Map {
        const char *name;               // the map's id in the table: its item's name, or the carving's
        Game::ItemClassId item;         // the map item's class; all zero for a map that is not an item
        Place seen;                     // the trigger of a map the player examines rather than carries
        const char *seenAfter;          // the quest that must be done before examining counts; null for none
        const char *label;              // the tooltip label on its destinations
        std::span<const Place> targets; // every place it leads to
        Place source;                   // where the map itself is found; id 0 when that is not marked
        const char *sourceLabel;        // the tooltip label there
        const char *quest;              // the quest the map belongs to, whose end retires it (all but its
                                        // afterQuest places); null for none
        Game::ItemClassId reveals;      // the next note of a riddle chain, found where this one leads: once the
                                        // player has or has read it, this one is done; all zero for none
        Category category;
    };

#include "MapTableData.inc"
} // namespace MapTable
