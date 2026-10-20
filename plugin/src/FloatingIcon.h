#pragma once

#include <span>

#include "Engine.h"
#include "MarkType.h"

// One icon per mark type drawn on screen over its nearest target, like a quest marker: the type's own map icon, shown
// within a set distance and fading in as the player approaches.
namespace FloatingIcon {
    // Called every frame for each of our types with its targets' positions and whether the player tracks that type
    // (the map legend's Track toggle); hides the type's icon while it is untracked, there is no target in range or
    // the nearest is off screen.
    void Update(const MarkType::Type &type, std::span<const Engine::Vec3> targets, bool tracked);
} // namespace FloatingIcon
