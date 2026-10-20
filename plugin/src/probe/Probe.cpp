#include "probe/Probe.h"

#include <algorithm>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Log.h"
#include "game/Game.h"
#include "probe/MapTable.h"

using Engine::ScriptValue;

namespace {
    // How close to a quest marker a destination counts as already marked by the quest, in metres.
    constexpr float kQuestMarkerReach = 15.f;
    constexpr const char *kProgressNames[] = {"not started", "active", "done", "failed"};

    // Per map item, whether it was read when last checked, so a map being read shows up as one log line.
    std::map<std::string_view, bool> g_read;
    // Per quest, its progress when last checked, logged when it changes.
    std::map<std::string_view, Game::QuestProgress> g_quests;
    // The destinations a quest marker covered at the last probe, logged when that changes.
    std::vector<std::uint64_t> g_covered;
    // Maps whose carving trigger could not be asked, logged once each.
    std::vector<std::string_view> g_loggedTrigger;

    // The entity at a place, if it is in the loaded level.
    const Engine::Entity *Find(const MapTable::Place &place) {
        const Engine::Entity *entity = place.entity ? Engine::GetEntity(place.entity) : nullptr;
        return entity && std::string_view(Engine::EntityName(entity)) == place.name ? entity : nullptr;
    }

    bool IsSet(const Game::ItemClassId &item) {
        return std::ranges::any_of(item, [](const std::uint8_t byte) { return byte != 0; });
    }

    bool IsItem(const MapTable::Map &map) { return IsSet(map.item); }

    // Whether the player has found the next note of a riddle chain (carries it or has read it), which lies where this
    // one leads: this one is then done.
    bool Revealed(const MapTable::Map &map, const Game::Inventory *player) {
        return IsSet(map.reveals) &&
               (Game::CountOfClass(player, map.reveals) > 0 || Game::ReadProgress(map.reveals) >= 1.f);
    }

    // Whether a destination has been dealt with: a hole dug (Hole.lua's nDepth is 1 once dug), a loose item taken
    // from its item slot, or a container emptied. Destinations with none of these (areas, triggers) stay marked while
    // the map is had.
    bool Done(const MapTable::Place &place, const Engine::Entity *entity) {
        if (const Engine::ScriptTable *script = Engine::EntityScript(entity)) {
            if (ScriptValue depth; Engine::TableGet(script, "nDepth", depth) && depth.type == ScriptValue::Number)
                return depth.number >= 1.f;
        }
        if (const std::optional<bool> holds = Game::SlotHoldsItem(place.entity))
            return !*holds;
        const Game::Inventory *inventory = Game::EntityInventory(entity);
        return inventory && Game::ItemCount(inventory) == 0;
    }

    // A quest's progress, logging each change so quest gates show up in the log.
    Game::QuestProgress ProgressOf(const char *quest) {
        const Game::QuestProgress progress = Game::GetQuestProgress(quest);
        if (const auto it = g_quests.find(quest); it == g_quests.end() || it->second != progress) {
            g_quests[quest] = progress;
            Log::Write("quest: {} is {}", quest, kProgressNames[static_cast<int>(progress)]);
        }
        return progress;
    }

    // Whether a map's quest is over (done or failed): the quest has had its use of the map, so only the places left
    // worth a visit afterwards (afterQuest) are marked, and not where the map is found.
    bool QuestOver(const MapTable::Map &map) {
        if (!map.quest)
            return false;
        const Game::QuestProgress progress = ProgressOf(map.quest);
        return progress == Game::QuestProgress::Done || progress == Game::QuestProgress::Failed;
    }

    // Whether a quest marker on the map already points at a position: it lies within reach of the marker's point or
    // inside (or within reach of) its search area, on the ground plane.
    bool Covered(const Engine::Vec3 &pos, const std::span<const Game::QuestMarker> markers) {
        return std::ranges::any_of(markers, [&](const Game::QuestMarker &marker) {
            const float dx = std::max({marker.min.x - pos.x, 0.f, pos.x - marker.max.x});
            const float dy = std::max({marker.min.y - pos.y, 0.f, pos.y - marker.max.y});
            return dx * dx + dy * dy <= kQuestMarkerReach * kQuestMarkerReach;
        });
    }

    // Whether a carving's one-shot quest trigger still takes a press: it stops once the carving has been examined
    // (and is also off before the open world unlocks).
    bool PressEnabled(const MapTable::Map &map, const Engine::Entity *trigger) {
        const Engine::ScriptTable *script = Engine::EntityScript(trigger);
        ScriptValue component, enabled;
        if (script && Engine::TableGet(script, "interactionTrigger", component) &&
            component.type == ScriptValue::Table &&
            Engine::CallMethod(static_cast<const Engine::ScriptTable *>(component.table), "IsPressEnabled", enabled) &&
            enabled.type == ScriptValue::Boolean)
            return enabled.boolean;
        if (!std::ranges::contains(g_loggedTrigger, std::string_view(map.name))) {
            Log::Write("probe: could not ask {}'s trigger whether it was examined", map.name);
            g_loggedTrigger.emplace_back(map.name);
        }
        return true;
    }

    // Whether the player has a map: carries its item, or has examined its carving. A carving's trigger is also off
    // before its quest gate (the open world unlocking), so it only counts as examined once that quest is done.
    bool HasMap(const MapTable::Map &map, const Game::Inventory *player) {
        if (IsItem(map))
            return Game::CountOfClass(player, map.item) > 0;
        const Engine::Entity *trigger = Find(map.seen);
        return trigger && (!map.seenAfter || ProgressOf(map.seenAfter) == Game::QuestProgress::Done) &&
               !PressEnabled(map, trigger);
    }

    // Whether the player knows where a map leads: a map item once read (its reading progress, which the game keeps per
    // item class, reaches 1, when the inventory's "new" marker goes), whether or not it is still carried; a carving
    // once examined. Logs each map item read, and each one that stops counting as read.
    bool Knows(const MapTable::Map &map, const bool has) {
        if (!IsItem(map))
            return has;
        const bool read = Game::ReadProgress(map.item) >= 1.f;
        if (const auto it = g_read.find(map.name); it == g_read.end() ? read : it->second != read)
            Log::Write("read: {} is {}", map.name, read ? "read" : "no longer read");
        g_read[map.name] = read;
        return read;
    }

    // Whether the object a map is found on still holds it: an item slot while it holds an item (a quest may fill it
    // only for a while), an inventory that has been filled unless it lacks the map (taken and sold, say). An empty
    // inventory may simply not be filled yet, and objects with neither (a carving's trigger) have no say; both count
    // as holding it while the player does not have the map.
    bool HoldsMap(const MapTable::Map &map, const Engine::Entity *entity) {
        if (const std::optional<bool> holds = Game::SlotHoldsItem(map.source.entity))
            return *holds;
        const Game::Inventory *inventory = IsItem(map) ? Game::EntityInventory(entity) : nullptr;
        return !inventory || Game::ItemCount(inventory) == 0 || Game::CountOfClass(inventory, map.item) > 0;
    }

    void Add(ProbeResult &result, const MapTable::Map &map, const Engine::Entity *entity, const char *label,
             const bool source) {
        if (const std::uint64_t wuid = Game::EntityWuid(entity))
            result.targets.push_back({.map = map.name,
                                      .label = label,
                                      .source = source,
                                      .wuid = wuid,
                                      .pos = Engine::EntityPosition(entity)});
    }
} // namespace

ProbeResult Probe::Run() {
    ProbeResult result;
    const Engine::Entity *player = Engine::PlayerEntity();
    const Game::Inventory *inventory = player ? Game::EntityInventory(player) : nullptr;
    if (!inventory)
        return result;
    result.playerReady = true;
    for (const MapTable::Map &map : MapTable::kMaps) {
        if (Revealed(map, inventory))
            continue;
        const bool questOver = QuestOver(map);
        const bool has = HasMap(map, inventory);
        if (Knows(map, has)) {
            for (const MapTable::Place &place : map.targets)
                if (place.afterQuest == questOver)
                    if (const Engine::Entity *entity = Find(place); entity && !Done(place, entity))
                        Add(result, map, entity, map.label, false);
        } else if (!has && !questOver && map.source.entity) {
            if (const Engine::Entity *entity = Find(map.source); entity && HoldsMap(map, entity))
                Add(result, map, entity, map.sourceLabel, true);
        }
    }

    // A destination a quest marker already points at is left to the quest while that marker shows.
    std::vector<std::uint64_t> covered;
    if (std::ranges::any_of(result.targets, [](const auto &t) { return !t.source; })) {
        const std::vector<Game::QuestMarker> markers = Game::QuestMarkers();
        std::erase_if(result.targets, [&](const ProbeResult::Target &t) {
            if (t.source || !Covered(t.pos, markers))
                return false;
            covered.push_back(t.wuid);
            if (!std::ranges::contains(g_covered, t.wuid))
                Log::Write("quest: a quest marker points at {}'s destination {:016x}; it is left to the quest", t.map,
                           t.wuid);
            return true;
        });
    }
    for (const std::uint64_t wuid : g_covered)
        if (!std::ranges::contains(covered, wuid) &&
            std::ranges::any_of(result.targets, [&](const auto &t) { return !t.source && t.wuid == wuid; }))
            Log::Write("quest: no quest marker points at destination {:016x} any more; it is marked again", wuid);
    g_covered = std::move(covered);
    return result;
}

void Probe::Reset() {
    g_read.clear();
    g_quests.clear();
    g_covered.clear();
    g_loggedTrigger.clear();
}
