#pragma once

#include <cstdint>
#include <vector>

#include "game/Engine.h"

// What should be marked right now, worked out from the game's own state for every map in MapTable.
struct ProbeResult {
    // Where a map the player has read (carried or not) leads, or where a map the player does not have yet is found; in
    // the loaded level and not dealt with yet.
    struct Target {
        const char *map;      // the map's id in the table
        const char *label;    // the map tooltip's localization label
        bool source;          // the map's own location rather than where it leads
        std::uint32_t entity; // the marked entity's EntityId
        std::uint64_t wuid; // its WUID; 0 for an entity the game made no AI object for (an item slot nothing links to)
        Engine::Vec3 pos;
    };

    bool playerReady = false; // the player exists, with an inventory
    std::vector<Target> targets;
};

namespace Probe {
    ProbeResult Run();
    // Forgets which maps were logged as read, so the next run logs each afresh (after a load).
    void Reset();
} // namespace Probe
