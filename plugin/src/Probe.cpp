#include "Probe.h"

#include <algorithm>
#include <map>
#include <string_view>
#include <vector>

#include "Game.h"
#include "Log.h"
#include "MapTable.h"

using Engine::ScriptValue;

namespace {
    // Per map item, whether it was read when last checked, so a map being read shows up as one log line.
    std::map<std::string_view, bool> g_read;
    // Maps whose carving trigger could not be asked, logged once each.
    std::vector<std::string_view> g_loggedTrigger;

    // The entity at a place, if it is in the loaded level.
    const Engine::Entity *Find(const MapTable::Place &place) {
        const Engine::Entity *entity = place.entity ? Engine::GetEntity(place.entity) : nullptr;
        return entity && std::string_view(Engine::EntityName(entity)) == place.name ? entity : nullptr;
    }

    bool IsItem(const MapTable::Map &map) {
        return std::ranges::any_of(map.item, [](const std::uint8_t byte) { return byte != 0; });
    }

    // Whether a destination has been dealt with: a hole dug (Hole.lua's nDepth is 1 once dug) or a container emptied.
    // Destinations with neither (loose items in item slots, areas, triggers) stay marked while the map is had.
    bool Done(const Engine::Entity *entity) {
        if (const Engine::ScriptTable *script = Engine::EntityScript(entity)) {
            if (ScriptValue depth; Engine::TableGet(script, "nDepth", depth) && depth.type == ScriptValue::Number)
                return depth.number >= 1.f;
        }
        const Game::Inventory *inventory = Game::EntityInventory(entity);
        return inventory && Game::ItemCount(inventory) == 0;
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

    // Whether the player has a map: carries its item, or has examined its carving.
    bool HasMap(const MapTable::Map &map, const Game::Inventory *player) {
        if (IsItem(map))
            return Game::CountOfClass(player, map.item) > 0;
        const Engine::Entity *trigger = Find(map.seen);
        return trigger && !PressEnabled(map, trigger);
    }

    // Whether the player has read a map item (its reading progress reaches 1, when the inventory's "new" marker goes);
    // a carving counts as read once examined.
    bool Read(const MapTable::Map &map) {
        if (!IsItem(map))
            return true;
        const bool read = Game::ReadProgress(map.item) >= 1.f;
        if (const auto it = g_read.find(map.name); it == g_read.end() || it->second != read) {
            g_read[map.name] = read;
            Log::Write("read: {} is {}", map.name, read ? "read" : "not read yet; its destinations wait");
        }
        return read;
    }

    // Whether the object a map is found on still holds it: an inventory that has been filled and lacks the map no
    // longer does (taken and sold, say). An empty inventory may simply not be filled yet, and objects without one (an
    // item slot, a carving's trigger) have no say; both count as holding it while the player does not have the map.
    bool HoldsMap(const MapTable::Map &map, const Engine::Entity *entity) {
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
        if (HasMap(map, inventory)) {
            if (!Read(map))
                continue;
            for (const MapTable::Place &place : map.targets)
                if (const Engine::Entity *entity = Find(place); entity && !Done(entity))
                    Add(result, map, entity, map.label, false);
        } else if (map.source.entity) {
            if (const Engine::Entity *entity = Find(map.source); entity && HoldsMap(map, entity))
                Add(result, map, entity, map.sourceLabel, true);
        }
    }
    return result;
}

void Probe::Reset() {
    g_read.clear();
    g_loggedTrigger.clear();
}
