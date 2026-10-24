#pragma once

#include <cstdint>
#include <span>

#include "game/Engine.h"
#include "marks/MarkType.h"

// An icon drawn on screen over each target in range, like a quest marker: the type's own map icon, shown within a set
// distance and fading in as the player approaches.
namespace FloatingIcon {
    // Called every frame for each of our types with its targets' EntityIds and whether the player tracks that type
    // (the map legend's Track toggle); hides all of the type's icons while it is untracked, and each target's icon
    // while that target is gone, out of range or off screen. Each icon stands over its entity where it is this frame,
    // so it keeps up with a person carrying a map.
    void Update(const MarkType::Type &type, std::span<const std::uint32_t> targets, bool tracked);
} // namespace FloatingIcon
