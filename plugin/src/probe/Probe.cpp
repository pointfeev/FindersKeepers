#include "probe/Probe.h"

#include <algorithm>
#include <map>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "Log.h"
#include "game/Game.h"
#include "options/Options.h"
#include "probe/MapTable.h"

using Engine::ScriptValue;

namespace {
    constexpr const char *kProgressNames[] = {"not started", "active", "done", "failed"};

    // Per map item, whether it was read when last checked, so a map being read shows up as one log line.
    std::map<std::string_view, bool> g_read;
    // Per quest, its progress when last checked, logged when it changes.
    std::map<std::string_view, Game::QuestProgress> g_quests;
    // The destinations a quest marker covered at the last probe, logged when that changes.
    std::vector<std::uint64_t> g_covered;
    // Maps whose carving trigger could not be asked, or whose source has no inventory, logged once each.
    std::vector<std::string_view> g_loggedTrigger;
    std::vector<std::string_view> g_loggedSource;

    // The entity at a place, if it is in the loaded level.
    const Engine::Entity *Find(const MapTable::Place &place) {
        return place.guid ? Engine::GetEntityByGuid(place.guid) : nullptr;
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

    // Whether a quest's interaction trigger takes a press right now; empty for entities without one. Quest graphs
    // switch these on only while using them does something (the open world unlocked, a quest step reached) and a
    // one-shot one off once used.
    std::optional<bool> PressEnabled(const Engine::Entity *entity) {
        const Engine::ScriptTable *script = Engine::EntityScript(entity);
        ScriptValue component, enabled;
        if (!script || !Engine::TableGet(script, "interactionTrigger", component) ||
            component.type != ScriptValue::Table)
            return std::nullopt;
        if (Engine::CallMethod(static_cast<const Engine::ScriptTable *>(component.table), "IsPressEnabled", enabled) &&
            enabled.type == ScriptValue::Boolean)
            return enabled.boolean;
        return std::nullopt;
    }

    // A quest trigger that takes no press gives nothing there: its quest step is not reached (Goatskin's barrel) or it
    // was used. The two look the same, so it is never marked, whatever the keep-finished option says.
    bool Inactive(const Engine::Entity *entity) {
        const std::optional<bool> enabled = PressEnabled(entity);
        return enabled && !*enabled;
    }

    // Whether a destination has been dealt with: a hole dug (Hole.lua's nDepth is 1 once dug), a loose item taken
    // from its item slot, or a container's treasure taken (any of its loot, or all of its quest items; a container
    // without a list once emptied). Destinations with none of these (areas, triggers) stay marked while the player has
    // the map.
    bool Done(const MapTable::Place &place, const Engine::Entity *entity) {
        if (const Engine::ScriptTable *script = Engine::EntityScript(entity)) {
            if (ScriptValue depth; Engine::TableGet(script, "nDepth", depth) && depth.type == ScriptValue::Number)
                return depth.number >= 1.f;
        }
        if (const std::optional<bool> holds = Game::SlotHoldsItem(Engine::EntityId(entity)))
            return !*holds;
        const Game::Inventory *inventory = Game::EntityInventory(entity);
        if (!inventory)
            return false;
        if (place.treasure.empty())
            return Game::ItemCount(inventory) == 0;
        const auto gone = [&](const Game::ItemClassId &item) { return Game::CountOfClass(inventory, item) == 0; };
        return place.anyTaken ? std::ranges::any_of(place.treasure, gone) : std::ranges::all_of(place.treasure, gone);
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

    // Whether a point marker of the map's own quest already points at a position: within reach of it, on the ground
    // plane. Search areas never count, even the map's own quest's: an area says to search, the mark says where.
    // Other quests' markers point at their own business (The Axe from the Lake's area covers the Drowner's urn).
    bool Covered(const Engine::Vec3 &pos, const std::span<const Game::QuestMarker> markers, const char *quest,
                 const float reach) {
        return quest && std::ranges::any_of(markers, [&](const Game::QuestMarker &marker) {
                   if (marker.area || marker.quest != quest)
                       return false;
                   const float dx = marker.min.x - pos.x;
                   const float dy = marker.min.y - pos.y;
                   return dx * dx + dy * dy <= reach * reach;
               });
    }

    bool Shown(const MapTable::Category category, const Options::Values &options) {
        switch (category) {
        case MapTable::Category::Treasure:
            return options.treasureMaps;
        case MapTable::Category::Quest:
            return options.questMaps;
        case MapTable::Category::Riddle:
            return options.riddleNotes;
        }
        return true;
    }

    // Whether the player has a map: carries its item, or has examined its carving. A carving's trigger is also off
    // before its quest gate (the open world unlocking), so it only counts as examined once that quest is done.
    bool HasMap(const MapTable::Map &map, const Game::Inventory *player) {
        if (IsItem(map))
            return Game::CountOfClass(player, map.item) > 0;
        const Engine::Entity *trigger = Find(map.seen);
        if (!trigger || (map.seenAfter && ProgressOf(map.seenAfter) != Game::QuestProgress::Done))
            return false;
        const std::optional<bool> enabled = PressEnabled(trigger);
        if (!enabled && !std::ranges::contains(g_loggedTrigger, std::string_view(map.name))) {
            Log::Write("probe: could not ask {}'s trigger whether it was examined", map.name);
            g_loggedTrigger.emplace_back(map.name);
        }
        return !enabled.value_or(true);
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
    // only for a while), a chest or NPC while its inventory has the map. Chests and souls get their items when the game
    // registers or initializes them, not when first opened (C_Stash, C_InventorySoul, checked in Ghidra), so what the
    // inventory has is what is there. A carving's trigger is the map itself, found until examined.
    bool HoldsMap(const MapTable::Map &map, const Engine::Entity *entity) {
        if (const std::optional<bool> holds = Game::SlotHoldsItem(Engine::EntityId(entity)))
            return *holds;
        if (!IsItem(map))
            return true;
        if (const Game::Inventory *inventory = Game::EntityInventory(entity))
            return Game::CountOfClass(inventory, map.item) > 0;
        if (!std::ranges::contains(g_loggedSource, std::string_view(map.name))) {
            Log::Write("probe: {}'s source has no inventory to hold it", map.name);
            g_loggedSource.emplace_back(map.name);
        }
        return false;
    }

    void Add(ProbeResult &result, const MapTable::Map &map, const Engine::Entity *entity, const char *label,
             const bool source) {
        result.targets.push_back({.map = map.name,
                                  .label = label,
                                  .source = source,
                                  .entity = Engine::EntityId(entity),
                                  .wuid = Game::EntityWuid(entity),
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
    const Options::Values &options = Options::Get();
    for (const MapTable::Map &map : MapTable::kMaps) {
        if (!Shown(map.category, options) || Revealed(map, inventory))
            continue;
        const bool questOver = QuestOver(map);
        const bool retired = questOver && options.retireQuestMaps;
        const bool has = HasMap(map, inventory);
        bool known = Knows(map, has) || (!options.requireRead && has);
        if (options.requireCarried)
            known = known && has;
        if (known) {
            if (!options.mapDestinations)
                continue;
            for (const MapTable::Place &place : map.targets)
                if (place.afterQuest ? questOver : !retired)
                    if (const Engine::Entity *entity = Find(place);
                        entity && !Inactive(entity) && (options.keepFinished || !Done(place, entity)))
                        Add(result, map, entity, map.label, false);
        } else if (options.mapLocations && !has && !retired && map.source.guid) {
            if (const Engine::Entity *entity = Find(map.source); entity && HoldsMap(map, entity))
                Add(result, map, entity, map.sourceLabel, true);
        }
    }

    // A destination its map's quest already points at is left to the quest while that marker shows.
    std::vector<std::uint64_t> covered;
    if (options.hideUnderQuestMarkers && std::ranges::any_of(result.targets, [](const auto &t) { return !t.source; })) {
        const std::vector<Game::QuestMarker> markers = Game::QuestMarkers();
        std::erase_if(result.targets, [&](const ProbeResult::Target &t) {
            const MapTable::Map &map = *std::ranges::find(MapTable::kMaps, t.map, &MapTable::Map::name);
            if (t.source || !Covered(t.pos, markers, map.quest, options.questMarkerReach))
                return false;
            covered.push_back(t.wuid);
            if (!std::ranges::contains(g_covered, t.wuid))
                Log::Write("quest: a {} marker points at {}'s destination {:016x}; it is left to the quest", map.quest,
                           t.map, t.wuid);
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
    g_loggedSource.clear();
}
