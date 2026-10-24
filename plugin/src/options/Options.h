#pragma once

#include <cstdint>
#include <span>
#include <string_view>

// The player's settings, kept in FindersKeepers.ini next to the plugin: the only thing the mod stores. The file is
// written with the defaults when missing (it is not shipped, so a mod update keeps the player's choices), re-read when
// it changes on disk, and rewritten when a setting changes in game (Mcm).
namespace Options {
    struct Values {
        // What shows.
        bool mapMarkers = true;      // world map and compass markers (one game mark, so one switch)
        bool floatingIcons = true;   // icons in the world over places in range
        bool mapDestinations = true; // where maps lead
        bool mapLocations = true;    // where maps are found
        // What counts.
        bool requireRead = true;     // a map's destinations show only once it is read
        bool requireCarried = false; // ... and only while it is carried
        bool keepFound = false;      // dug, emptied or taken places, and found maps' locations, stay marked
        bool retireQuestMaps = true; // a quest's maps stop showing once the quest is over
        bool hideUnderQuestMarkers = true;
        // Which maps (both their marks).
        bool treasureMaps = true;
        bool questMaps = true;
        bool riddleNotes = true;
        // Distances and sizes.
        float iconShowDistance = 40.f; // metres
        float iconFullDistance = 25.f; // metres; fully opaque within
        float iconSize = 48.f;         // pixels on a 1080-line screen
        float iconHeight = 1.2f;       // metres above the place
        float questMarkerReach = 15.f; // metres

        bool operator==(const Values &) const = default;
    };

    // A setting as both the INI and the in-game menu show it.
    struct Setting {
        const char *id; // the INI key and the menu's setting id
        const char *section;
        const char *label;
        const char *tooltip;
        bool Values::*toggle;
        float Values::*slider;
        float min, max, step;
        const char *suffix;
    };
    std::span<const Setting> Settings();
    // A setting's tooltip with its default added, for the in-game menu; valid for the plugin's lifetime.
    const char *Tooltip(const Setting &setting);

    // Reads the file, writing it with the defaults when it is missing. Called once at load.
    void Install();
    // Re-reads the file if it changed on disk; called on every probe.
    void Refresh();
    const Values &Get();
    // Changes one setting (from the in-game menu) and rewrites the file; false for an unknown id.
    bool Set(std::string_view id, double value);
    // Increases whenever a value changes, so callers can react once.
    std::uint32_t Version();
} // namespace Options
