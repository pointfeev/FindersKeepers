#pragma once

#include <span>

#include "Engine.h"

// The treasure map icon drawn on screen over the nearest treasure, like a quest marker: shown within a set distance
// and fading in as the player approaches.
namespace FloatingIcon {
    // Called every frame with the treasures' positions and whether the player tracks the treasure marker type (the
    // map legend's Track toggle); hides the icon while it is untracked, there is no treasure in range or the nearest
    // is off screen.
    void Update(std::span<const Engine::Vec3> treasures, bool tracked);
} // namespace FloatingIcon
