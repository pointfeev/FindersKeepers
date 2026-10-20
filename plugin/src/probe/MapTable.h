#pragma once

#include <cstdint>
#include <span>

#include "game/Game.h"

// The maps the mod marks, generated at build time from research/treasure_maps.json (tools/treasure_data/maptable.py)
// into MapTableData.inc.
namespace MapTable {
    // A level entity: its EntityId and name (ids repeat across levels, so the name confirms the match); id 0 for none.
    struct Place {
        std::uint32_t entity = 0;
        const char *name = nullptr;
        bool afterQuest = false; // a destination marked only once the map's quest has ended, instead of until then
    };

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
    };

#include "MapTableData.inc"
} // namespace MapTable
