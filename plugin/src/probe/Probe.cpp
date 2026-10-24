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

    // Per map item, whether it was read when last checked, so a map being read shows up as one log line. The maps are
    // function-local: MSVC's std::map allocates when constructed, and a throw while the DLL loads fails the load.
    std::map<std::string_view, bool> &ReadState() {
        static std::map<std::string_view, bool> read;
        return read;
    }
    // Per quest, its progress when last checked, logged when it changes.
    std::map<std::string_view, Game::QuestProgress> &QuestState() {
        static std::map<std::string_view, Game::QuestProgress> quests;
        return quests;
    }
    // The destinations (EntityIds) a quest marker covered at the last probe, logged when that changes.
    std::vector<std::uint32_t> g_covered;
    // Maps whose carving trigger could not be asked, and places (GUIDs) whose entity is not the kind the table says,
    // logged once each.
    std::vector<std::string_view> g_loggedTrigger;
    std::vector<std::uint64_t> g_loggedKind;

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

    // What a place still offers the player.
    enum class State {
        Open,     // still to be found: marked
        Taken,    // found and dealt with: marked only with KeepFound
        Inactive, // nothing there to use: never marked
    };

    // A place's state by its kind. A hole is taken once dug (Hole.lua's nDepth is 1 then), an item slot once its item
    // is gone (a quest may fill it only for a while), a container or person once its items are (any or all of them,
    // by the place's rule; a container without a list once empty). Chests and souls get their items when the game
    // registers or initializes them, not when first opened (C_Stash, C_InventorySoul, checked in Ghidra), so what an
    // inventory holds is what is there. A quest trigger that takes no press gives nothing there: its quest step is not
    // reached (Goatskin's barrel) or it was used, and the two look the same, so it is inactive. An area offers
    // nothing to read and stays open.
    State StateOf(const MapTable::Place &place, const Engine::Entity *entity) {
        switch (place.kind) {
        case MapTable::Kind::Hole: {
            const Engine::ScriptTable *script = Engine::EntityScript(entity);
            ScriptValue depth;
            const bool dug = script && Engine::TableGet(script, "nDepth", depth) && depth.type == ScriptValue::Number &&
                             depth.number >= 1.f;
            return dug ? State::Taken : State::Open;
        }
        case MapTable::Kind::ItemSlot:
            if (const std::optional<bool> holds = Game::SlotHoldsItem(Engine::EntityId(entity)))
                return *holds ? State::Open : State::Taken;
            break;
        case MapTable::Kind::Container:
        case MapTable::Kind::Person:
            if (const Game::Inventory *inventory = Game::EntityInventory(entity)) {
                if (place.treasure.empty())
                    return Game::ItemCount(inventory) == 0 ? State::Taken : State::Open;
                const auto gone = [&](const Game::ItemClassId &item) {
                    return Game::CountOfClass(inventory, item) == 0;
                };
                const bool taken = place.anyTaken ? std::ranges::any_of(place.treasure, gone)
                                                  : std::ranges::all_of(place.treasure, gone);
                return taken ? State::Taken : State::Open;
            }
            break;
        case MapTable::Kind::Trigger:
            return PressEnabled(entity).value_or(true) ? State::Open : State::Inactive;
        case MapTable::Kind::Area:
            return State::Open;
        }
        // The level entity is not what the table says it is (validate checks this against each game build).
        if (!std::ranges::contains(g_loggedKind, place.guid)) {
            Log::Write("probe: entity {:016x} is not the {} the map table says", place.guid,
                       place.kind == MapTable::Kind::ItemSlot ? "item slot" : "container or person with an inventory");
            g_loggedKind.push_back(place.guid);
        }
        return State::Inactive;
    }

    // A quest's progress, logging each change so quest gates show up in the log.
    Game::QuestProgress ProgressOf(const char *quest) {
        const Game::QuestProgress progress = Game::GetQuestProgress(quest);
        std::map<std::string_view, Game::QuestProgress> &quests = QuestState();
        if (const auto it = quests.find(quest); it == quests.end() || it->second != progress) {
            quests[quest] = progress;
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
        std::map<std::string_view, bool> &states = ReadState();
        if (const auto it = states.find(map.name); it == states.end() ? read : it->second != read)
            Log::Write("read: {} is {}", map.name, read ? "read" : "no longer read");
        states[map.name] = read;
        return read;
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
    // The quest markers, gathered once a destination of a map with a quest needs them.
    std::optional<std::vector<Game::QuestMarker>> markers;
    std::vector<std::uint32_t> covered;
    for (const MapTable::Map &map : MapTable::kMaps) {
        if (!Shown(map.category, options) || Revealed(map, inventory))
            continue;
        const bool questOver = QuestOver(map);
        const bool retired = questOver && options.retireQuestMaps;
        const bool has = HasMap(map, inventory);
        const bool read = Knows(map, has);
        bool known = read || (!options.requireRead && has);
        if (options.requireCarried)
            known = known && has;
        if (known && options.mapDestinations) {
            for (const MapTable::Place &place : map.targets) {
                const Engine::Entity *entity = (place.afterQuest ? questOver : !retired) ? Find(place) : nullptr;
                if (!entity)
                    continue;
                if (const State state = StateOf(place, entity);
                    state == State::Inactive || (state == State::Taken && !options.keepFound))
                    continue;
                // A destination its map's quest already points at is left to the quest while that marker shows.
                if (options.hideUnderQuestMarkers && map.quest) {
                    if (!markers)
                        markers = Game::QuestMarkers();
                    if (Covered(Engine::EntityPosition(entity), *markers, map.quest, options.questMarkerReach)) {
                        const std::uint32_t id = Engine::EntityId(entity);
                        covered.push_back(id);
                        if (!std::ranges::contains(g_covered, id))
                            Log::Write("quest: a {} marker points at {}'s destination, entity {}; it is left to the "
                                       "quest",
                                       map.quest, map.name, id);
                        continue;
                    }
                }
                Add(result, map, entity, map.label, false);
            }
        }
        // A map carried or read has been found, whatever RequireCarried says of its destinations: its source refills a
        // copy like any other loot, which would otherwise be marked again once the map is put down.
        if (options.mapLocations && (options.keepFound || (!has && !read)) && !retired)
            if (const Engine::Entity *entity = Find(map.source); entity && StateOf(map.source, entity) == State::Open)
                Add(result, map, entity, map.sourceLabel, true);
    }
    for (const std::uint32_t id : g_covered)
        if (!std::ranges::contains(covered, id) &&
            std::ranges::any_of(result.targets, [&](const auto &t) { return !t.source && t.entity == id; }))
            Log::Write("quest: no quest marker points at destination entity {} any more; it is marked again", id);
    g_covered = std::move(covered);
    return result;
}

void Probe::Reset() {
    ReadState().clear();
    QuestState().clear();
    g_covered.clear();
    g_loggedTrigger.clear();
    g_loggedKind.clear();
}
