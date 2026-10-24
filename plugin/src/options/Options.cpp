#include "options/Options.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <string>

#include "Log.h"

namespace {
    using Options::Setting;
    using Options::Values;

    constexpr Setting kSettings[] = {
        {.id = "MapMarkers",
         .section = "Display",
         .label = "Map and compass markers",
         .tooltip = "Mark places on the world map and the compass (the game shows both from one mark). The map "
                    "legend's Track toggle can also hide each kind of mark in a save.",
         .toggle = &Values::mapMarkers},
        {.id = "FloatingIcons",
         .section = "Display",
         .label = "Icons in the world",
         .tooltip = "Draw an icon over each marked place within range. Icons follow the map legend's Track toggle, "
                    "except while map and compass markers are off.",
         .toggle = &Values::floatingIcons},
        {.id = "MapDestinations",
         .section = "Marks",
         .label = "Map destinations",
         .tooltip = "Mark the places the maps you know lead to.",
         .toggle = &Values::mapDestinations},
        {.id = "MapLocations",
         .section = "Marks",
         .label = "Map locations",
         .tooltip = "Mark where maps you have not found yet lie.",
         .toggle = &Values::mapLocations},
        {.id = "TreasureMaps",
         .section = "Maps",
         .label = "Treasure maps",
         .tooltip = "Mark treasure maps: where they lead and where they are found.",
         .toggle = &Values::treasureMaps},
        {.id = "QuestMaps",
         .section = "Maps",
         .label = "Quest maps",
         .tooltip = "Mark maps from quests: where they lead and where they are found.",
         .toggle = &Values::questMaps},
        {.id = "RiddleNotes",
         .section = "Maps",
         .label = "Riddle notes",
         .tooltip = "Mark riddle notes (the Strange Little Verses, the Strange Riddle): where they lead and where "
                    "they are found.",
         .toggle = &Values::riddleNotes},
        {.id = "RequireRead",
         .section = "Rules",
         .label = "Only maps you have read",
         .tooltip = "Show where a map leads only once you have read it.",
         .toggle = &Values::requireRead},
        {.id = "RequireCarried",
         .section = "Rules",
         .label = "Only maps you carry",
         .tooltip = "Hide where a map leads once it leaves your inventory, even if you have read it.",
         .toggle = &Values::requireCarried},
        {.id = "KeepFound",
         .section = "Rules",
         .label = "Keep marks once found",
         .tooltip =
             "Keep marking places after you find what they hold: a hole dug, a container looted, an item taken, a "
             "map found.",
         .toggle = &Values::keepFound},
        {.id = "RetireQuestMaps",
         .section = "Rules",
         .label = "Retire quest maps",
         .tooltip = "Stop marking a quest's maps once the quest is done or failed.",
         .toggle = &Values::retireQuestMaps},
        {.id = "HideUnderQuestMarkers",
         .section = "Rules",
         .label = "Defer to quest markers",
         .tooltip = "Hide a mark while its own quest's tracked marker points at the same place.",
         .toggle = &Values::hideUnderQuestMarkers},
        {.id = "IconShowDistance",
         .section = "Distances",
         .label = "Icon range",
         .tooltip = "Show an icon in the world over places within this distance.",
         .slider = &Values::iconShowDistance,
         .min = 10.f,
         .max = 150.f,
         .step = 5.f,
         .suffix = " m"},
        {.id = "IconFullDistance",
         .section = "Distances",
         .label = "Icon fade",
         .tooltip = "Icons fade in from the range above and are fully opaque within this distance.",
         .slider = &Values::iconFullDistance,
         .min = 0.f,
         .max = 150.f,
         .step = 5.f,
         .suffix = " m"},
        {.id = "IconSize",
         .section = "Distances",
         .label = "Icon size",
         .tooltip = "Icon size up close on a 1080-line screen, scaled with the screen height.",
         .slider = &Values::iconSize,
         .min = 16.f,
         .max = 128.f,
         .step = 4.f,
         .suffix = " px"},
        {.id = "IconFarScale",
         .section = "Distances",
         .label = "Far icon scale",
         .tooltip =
             "Icon size at the edge of the icon range, as a share of the icon size. Icons are full size within 5 m "
             "and shrink steadily beyond, like what they mark, down to this. 100% keeps them one size.",
         .slider = &Values::iconFarScale,
         .min = 10.f,
         .max = 100.f,
         .step = 5.f,
         .suffix = "%"},
        // Not in kRenamed: the older IconHeight measured from the object's base, so its value means something else.
        {.id = "IconHeightAboveTop",
         .section = "Distances",
         .label = "Icon height",
         .tooltip = "How far above the top of the marked object the icon's bottom edge floats.",
         .slider = &Values::iconHeight,
         .min = 0.f,
         .max = 3.f,
         .step = 0.1f,
         .suffix = " m"},
        {.id = "QuestMarkerReach",
         .section = "Distances",
         .label = "Quest marker reach",
         .tooltip = "A quest marker this close to a place counts as pointing at it.",
         .slider = &Values::questMarkerReach,
         .min = 0.f,
         .max = 50.f,
         .step = 1.f,
         .suffix = " m"},
    };

    Values g_values;
    std::uint32_t g_version = 0;
    std::filesystem::path g_path;
    // The file's time stamp as last read or written, so our own writes are not read back as edits.
    std::filesystem::file_time_type g_seen{};

    std::filesystem::path IniPath() {
        HMODULE self = nullptr;
        GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                           reinterpret_cast<LPCWSTR>(&IniPath), &self);
        wchar_t path[MAX_PATH * 2];
        const DWORD length = GetModuleFileNameW(self, path, std::size(path));
        return std::filesystem::path(std::wstring(path, length)).replace_filename(L"FindersKeepers.ini");
    }

    // Applies a value to one setting (sliders clamped to their range); true when it changed.
    bool Apply(Values &values, const Setting &setting, const double value) {
        if (setting.toggle) {
            const bool on = value != 0.0;
            const bool changed = values.*setting.toggle != on;
            values.*setting.toggle = on;
            return changed;
        }
        const float clamped = std::clamp(static_cast<float>(value), setting.min, setting.max);
        const bool changed = values.*setting.slider != clamped;
        values.*setting.slider = clamped;
        return changed;
    }

    std::string Format(const Values &values, const Setting &setting) {
        return setting.toggle ? (values.*setting.toggle ? "1" : "0") : std::format("{:g}", values.*setting.slider);
    }

    // The default as players read it: "on", "off", or the value with its unit ("40 m").
    std::string DefaultText(const Setting &setting) {
        static constexpr Values defaults;
        return setting.toggle ? (defaults.*setting.toggle ? "on" : "off")
                              : std::format("{:g}{}", defaults.*setting.slider, setting.suffix);
    }

    bool SameKey(const std::string_view a, const std::string_view b) {
        return std::ranges::equal(a, b, [](const char x, const char y) { return std::tolower(x) == std::tolower(y); });
    }

    const Setting *FindSetting(const std::string_view id) {
        const auto it =
            std::ranges::find_if(kSettings, [&](const Setting &setting) { return SameKey(id, setting.id); });
        return it == std::end(kSettings) ? nullptr : &*it;
    }

    // Keys an earlier version wrote, read as the setting that replaced them so a player's choice carries over; the
    // file is then rewritten under the current key.
    struct Renamed {
        const char *old;
        const char *id;
    };
    constexpr Renamed kRenamed[] = {
        {.old = "KeepFinished", .id = "KeepFound"}, // 1.1.0 and before
    };

    const Setting *FindRenamed(const std::string_view key) {
        const auto it =
            std::ranges::find_if(kRenamed, [&](const Renamed &renamed) { return SameKey(key, renamed.old); });
        return it == std::end(kRenamed) ? nullptr : FindSetting(it->id);
    }

    void Save() {
        std::ofstream out(g_path, std::ios::trunc);
        out << "; Finders Keepers settings. Change them in game with Mod Configuration Menu, or here: edits apply\n"
               "; within a moment while the game runs. Delete this file to restore the defaults.\n";
        const char *section = nullptr;
        for (const Setting &setting : kSettings) {
            if (!section || std::string_view(section) != setting.section) {
                section = setting.section;
                out << "\n[" << section << "]\n";
            }
            out << "; " << setting.label << ": " << setting.tooltip;
            if (setting.toggle)
                out << " 1 = on, 0 = off.";
            else
                out << std::format(" {:g} to {:g}{}.", setting.min, setting.max, setting.suffix);
            out << " Default: " << DefaultText(setting) << ".\n"
                << setting.id << " = " << Format(g_values, setting) << "\n";
        }
        out.close();
        std::error_code error;
        g_seen = std::filesystem::last_write_time(g_path, error);
        if (!out || error)
            Log::Write("options: could not write {}", g_path.string());
    }

    // Reads the file over the defaults. Rewrites it in full when it was absent, lacks keys (a new version's settings)
    // or has lines it ignored or read under an old name (an old version's settings, a bad value), so it always shows
    // what is in effect.
    void Load() {
        std::ifstream in(g_path);
        if (!in) {
            Save();
            Log::Write("options: wrote the defaults to {}", g_path.string());
            return;
        }
        Values values;
        std::array<bool, std::size(kSettings)> found{};
        bool ignored = false;
        for (std::string line; std::getline(in, line);) {
            const auto trim = [](const std::string_view text) {
                const auto first = text.find_first_not_of(" \t\r");
                return first == std::string_view::npos ? std::string_view{}
                                                       : text.substr(first, text.find_last_not_of(" \t\r") - first + 1);
            };
            const std::string_view text = trim(line);
            const auto equals = text.find('=');
            if (text.empty() || text.front() == ';' || text.front() == '#' || text.front() == '[' ||
                equals == std::string_view::npos)
                continue;
            const std::string_view key = trim(text.substr(0, equals));
            const std::string_view raw = trim(text.substr(equals + 1));
            const Setting *setting = FindSetting(key);
            if (!setting) {
                setting = FindRenamed(key);
                ignored = ignored || setting != nullptr;
            }
            double value = 0.0;
            if (!setting || std::from_chars(raw.data(), raw.data() + raw.size(), value).ec != std::errc{}) {
                Log::Write("options: ignored '{}' in {}", text, g_path.filename().string());
                ignored = true;
                continue;
            }
            Apply(values, *setting, value);
            found[setting - kSettings] = true;
        }
        in.close();
        std::error_code error;
        g_seen = std::filesystem::last_write_time(g_path, error);
        for (const Setting &setting : kSettings)
            if (Format(values, setting) != Format(g_values, setting))
                Log::Write("options: {} = {}", setting.id, Format(values, setting));
        if (values != g_values) {
            g_values = values;
            ++g_version;
        }
        if (ignored || !std::ranges::all_of(found, std::identity{}))
            Save();
    }
} // namespace

std::span<const Setting> Options::Settings() { return kSettings; }

const char *Options::Tooltip(const Setting &setting) {
    static const auto tooltips = [] {
        std::array<std::string, std::size(kSettings)> texts;
        for (std::size_t i = 0; i < texts.size(); ++i)
            texts[i] = std::format("{} Default: {}.", kSettings[i].tooltip, DefaultText(kSettings[i]));
        return texts;
    }();
    return tooltips[&setting - kSettings].c_str();
}

void Options::Install() {
    g_path = IniPath();
    Load();
}

void Options::Refresh() {
    std::error_code error;
    const auto stamp = std::filesystem::last_write_time(g_path, error);
    if (error) {
        // Deleted while the game runs: write it again with what is in effect.
        Save();
        return;
    }
    if (stamp != g_seen)
        Load();
}

const Values &Options::Get() { return g_values; }

bool Options::Set(const std::string_view id, const double value) {
    const Setting *setting = FindSetting(id);
    if (!setting)
        return false;
    if (Apply(g_values, *setting, value)) {
        ++g_version;
        Log::Write("options: {} = {} (menu)", setting->id, Format(g_values, *setting));
        Save();
    }
    return true;
}

std::uint32_t Options::Version() { return g_version; }
