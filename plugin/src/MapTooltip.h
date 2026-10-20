#pragma once

#include "Game.h"

// Names the treasure map on our map markers' hover tooltip instead of the mark type's "Treasure". The map movie
// (ApseMap.gfx) shows a marker's tooltip through _root.OnShowTooltip(uiName, marker), which hands uiName, a
// localization label ("ui_maplegend_<type>" for entity marks), to the game; the marker's roll-out hides it with the
// same m_UiName. This replaces OnShowTooltip in the loaded map movie with a native one that, for our markers, sets
// the marker's m_UiName to the map item's own name label (GameMark::LabelFor) before calling the original, so the
// game localizes the map's name and the roll-out passes the same label.
namespace MapTooltip {
    // Patches the map movie unless it is patched already. Called before the map sends its markers.
    void Patch(const Game::Map *map);
} // namespace MapTooltip
