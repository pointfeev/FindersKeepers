#pragma once

#include <cstdint>

// Our own map/compass mark type. The game names mark types in one function (a switch from E_MarkType to strings
// such as "GeneralPoi"; unknown values become "?"), and the map and compass derive everything from that name: icon
// textures (Libs/UI/Textures/Icons/Map/<Name>_icon.dds), the legend/tooltip string (ui_maplegend_<Name>) and, on the
// map, the marker's importance. Install() patches that function so kId is named kName, plus the places that treat
// types by a fixed list or table: the name -> filter id mapper behind the legend's Track toggle and saved filter
// state, the legend's check for its top group, and the map marker's stacking priority.
namespace MarkType {
    // An E_MarkType value the game leaves unnamed (one of the gaps in 0..0x60; ids above 0x60 fail the map's POI
    // filter check).
    constexpr uint32_t kId = 60;
    constexpr const char kName[] = "TreasureFinder";

    // Finds the four functions by signature and redirects each through a stub that answers for kId/kName and runs
    // the original for everything else. Returns false, changing nothing, when the naming function is not found or
    // kId is already named by the game; the other three are logged and skipped when not found.
    bool Install();
} // namespace MarkType
