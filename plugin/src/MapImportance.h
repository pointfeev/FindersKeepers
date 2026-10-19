#pragma once

// Makes our mark type a high-importance map marker, like quest givers and fast travel points: always shown and only
// shrinking slowly when the map is zoomed out. The world map decides importance by marker type name in ActionScript
// (MEnum.GetPoiImportance in ApseMap.gfx); this replaces that function in the loaded map movie with a native one that
// answers "high" for MarkType::kName and asks the original for every other name.
namespace MapImportance {
    // Hooks the map screen's sending of its POI markers, which patches the map movie first whenever a freshly loaded
    // one is not patched yet. Called once, after MarkType::Install.
    void Install();
} // namespace MapImportance
