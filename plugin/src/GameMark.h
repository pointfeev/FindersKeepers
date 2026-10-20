#pragma once

#include <cstdint>
#include <span>

#include "Probe.h"

// Markers on the compass and the world map, through the game's own entity marks (the mechanism quests use to point at
// an object: C_UIMap::GetOrCreateEntityMark + RegisterEntityMark), of our own mark types (MarkType: destinations and
// map locations). Whether they show follows each type's filter flag: on by default, switched by the legend's Track
// toggle and kept in saves.
namespace GameMark {
    // Marks exactly these targets' objects, each with its kind's type: adds marks for new targets whose objects exist,
    // removes the rest. A mark also goes when the game destroys its object, and comes back on a later call once the
    // object exists again. Called during gameplay only: marks registered while loading lose their compass marker.
    void Sync(std::span<const ProbeResult::Target> targets);
    // The tooltip label of the mark with this id (S_EntityMapMark's id, which the map movie gets as the marker's
    // Id); null for marks that are not ours.
    const char *LabelFor(std::int32_t markId);
    // Removes every mark, before a load replaces the world.
    void Clear();
} // namespace GameMark
