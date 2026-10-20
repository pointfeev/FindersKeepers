#pragma once

#include <array>
#include <cstdint>
#include <string_view>

// Our own map/compass mark types. The game names mark types in one function (a switch from E_MarkType to strings
// such as "GeneralPoi"; unknown values become "?"), and the map and compass derive everything from that name: icon
// textures (Libs/UI/Textures/Icons/Map/<Name>_icon.dds), the legend/tooltip string (ui_maplegend_<Name>) and, on the
// map, the marker's importance. Install() patches that function so our ids get our names, plus the places that treat
// types by a fixed list or table: the name -> filter id mapper behind the legend's Track toggle and saved filter
// state, the legend's check for its top group, and the map marker's stacking priority. Each type gets its own legend
// entry and Track toggle.
namespace MarkType {
    // An E_MarkType value the game leaves unnamed (one of the gaps in 0..0x60; ids above 0x60 fail the map's POI
    // filter check) and the name we give it.
    struct Type {
        std::uint32_t id;
        const char *name;
    };
    // Where the treasure maps the player has lead.
    inline constexpr Type kDestination{.id = 60, .name = "TreasureFinder"};
    // Where treasure maps the player does not have yet are found.
    inline constexpr Type kSource{.id = 61, .name = "TreasureFinderMap"};
    inline constexpr std::array kTypes{kDestination, kSource};

    constexpr const Type *Find(const std::uint32_t id) {
        for (const Type &type : kTypes)
            if (type.id == id)
                return &type;
        return nullptr;
    }
    constexpr const Type *Find(const std::string_view name) {
        for (const Type &type : kTypes)
            if (name == type.name)
                return &type;
        return nullptr;
    }

    // Finds the four functions by signature and redirects each through a hook that answers for our types and runs
    // the original for everything else. Returns false, changing nothing, when the naming function is not found or one
    // of our ids is already named by the game; the other three are logged and skipped when not found.
    bool Install();
} // namespace MarkType
