#pragma once

#include <cstdint>

// A marker for one entity on the compass and the world map, through the game's own entity marks (the mechanism
// quests use to point at an object: C_UIMap::GetOrCreateEntityMark + RegisterEntityMark), of our own mark type
// (MarkType).
namespace GameMark {
    // Registers the mark once the entity's WUID is known, the map UI exists and gameplay has started. Whether it shows
    // follows the type's filter flag: on by default, switched by the legend's Track toggle and kept in saves. Called
    // every frame until it has registered.
    void Update(uint64_t wuid);
} // namespace GameMark
