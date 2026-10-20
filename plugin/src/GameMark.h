#pragma once

#include <cstdint>
#include <span>

// Markers on the compass and the world map, through the game's own entity marks (the mechanism quests use to point at
// an object: C_UIMap::GetOrCreateEntityMark + RegisterEntityMark), of our own mark type (MarkType). Whether they show
// follows the type's filter flag: on by default, switched by the legend's Track toggle and kept in saves.
namespace GameMark {
    // Marks exactly these objects: adds marks for new WUIDs whose objects exist, removes the rest. A mark also goes
    // when the game destroys its object, and comes back on a later call once the object exists again. Called during
    // gameplay only: marks registered while loading lose their compass marker.
    void Sync(std::span<const std::uint64_t> wuids);
    // Removes every mark, before a load replaces the world.
    void Clear();
} // namespace GameMark
