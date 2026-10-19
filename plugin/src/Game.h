#pragma once

#include <cstdint>
#include <memory>

#include "Flash.h"

// Warhorse's game objects the plugin uses, declared from WHGame.dll 1.5.6.
namespace Game {
    // Finds the game functions below. Called once WHGame.dll is loaded; false (logged) when one is not found.
    bool Install();

    // Whether the game has finished loading into gameplay (C_GameplayManager's flag, read by IsGameplayStarted).
    bool GameplayStarted();

    // The "hud" Flash movie (compass, prompts), whose _root is in screen pixels; null while it is not loaded.
    Flash::Player *HudPlayer();

    // The world map screen (C_UIMap); null until the GUI has built its screens.
    struct Map;
    Map *GetMap();
    // Its "ApseMap" Flash movie; null while it is not loaded.
    Flash::Player *MapPlayer(Map *map);
    // Whether marks of a type show (the legend's Track toggle): m_poiFilterVisible, by filter id.
    bool IsTypeVisible(Map *map, std::uint32_t type);

    // A map/compass mark on an object (S_EntityMapMark), shared between the map and its creator; only the fields
    // the plugin reads.
    struct EntityMark {
        std::int32_t id;
        std::uint32_t type;
    };
    // Finds or creates the mark of `type` on `object` and registers it, which shows it while its type is visible.
    // Null when the game returns none.
    std::shared_ptr<EntityMark> AddEntityMark(Map *map, std::uint32_t type, void *object, std::uint32_t param);

    // The linkable object (an AI object's link node) for a WUID, which marks track; null when unknown.
    void *FindLinkable(std::uint64_t wuid);
} // namespace Game
