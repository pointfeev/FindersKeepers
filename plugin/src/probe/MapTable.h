#pragma once

#include <cstdint>
#include <span>

#include "game/Game.h"

// The maps the mod marks, generated at build time from research/treasure_maps.json (tools/treasure_data/maptable.py)
// into MapTableData.inc.
namespace MapTable {
    // What a place is, from its level entity's class (tools/treasure_data/index.py, PLACE_KINDS), and so what the
    // probe reads from it: a hole whether it is dug, an item slot whether it holds its item, a container or a person
    // their inventory, a quest trigger whether it takes a press. An area holds nothing to read.
    enum class Kind : std::uint8_t { Area, Container, Hole, ItemSlot, Person, Trigger };

    // A level entity, by its level GUID (Engine::GetEntityByGuid): patches renumber EntityIds, not GUIDs. 0 for none.
    struct Place {
        // The items a container or person holds for the player: a destination's treasure, or the map itself at its
        // source. Empty for other places and for a container that counts as found only once empty. With anyTaken, it
        // counts as taken once any of them is gone (a destination's guaranteed loot, a source's map); without, once
        // all of them are (quest items that never refill).
        std::span<const Game::ItemClassId> treasure = {};
        std::uint64_t guid = 0;
        Kind kind = Kind::Area;
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
        Place source;                   // where the map itself is found; GUID 0 when that is not marked
        const char *sourceLabel;        // the tooltip label there
        const char *quest;              // the quest the map belongs to, whose end retires it (all but its
                                        // afterQuest places); null for none
        Game::ItemClassId reveals;      // the next note of a riddle chain, found where this one leads: once the
                                        // player has or has read it, this one is done; all zero for none
        Category category;
    };

#include "MapTableData.inc"
} // namespace MapTable
