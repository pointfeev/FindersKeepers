#pragma once

#include "Engine.h"

// The treasure map icon drawn on screen over a world position, like a quest marker: shown within a set distance and
// fading in as the player approaches.
namespace FloatingIcon {
    // Called every frame with the treasure's position and whether the player tracks the treasure marker type (the map
    // legend's Track toggle); hides the icon while it is untracked, out of range or off screen.
    void Update(const Engine::Vec3 &world, bool tracked);
} // namespace FloatingIcon
