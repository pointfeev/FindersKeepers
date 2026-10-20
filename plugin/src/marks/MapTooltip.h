#pragma once

#include "game/Game.h"

// Names the treasure map on our map markers' hover tooltip ("Treasure: <map>") instead of the type's name alone. The
// map movie (ApseMap.gfx) shows a marker's tooltip through _root.OnShowTooltip(uiName, marker), which hands uiName, a
// localization label ("ui_maplegend_<type>" for entity marks), to the game; the marker's roll-out hides it with the
// same m_UiName. This replaces OnShowTooltip in the loaded map movie with a native one that, for our markers, sets
// the marker's m_UiName to the mark's own tooltip label (GameMark::LabelFor, strings built per map at build time)
// before calling the original, so the game localizes it and the roll-out passes the same label.
namespace MapTooltip {
    // Patches the map movie unless it is patched already. Called before the map sends its markers.
    void Patch(const Game::Map *map);
} // namespace MapTooltip
