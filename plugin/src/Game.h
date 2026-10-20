#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>

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
    Flash::Player *MapPlayer(const Map *map);
    // Whether marks of a type show (the legend's Track toggle): m_poiFilterVisible, by filter id.
    bool IsTypeVisible(const Map *map, std::uint32_t type);

    // A map/compass mark on an object (S_EntityMapMark), shared between the map and its creator; only the fields
    // the plugin reads.
    struct EntityMark {
        std::int32_t id;
        std::uint32_t type;
    };
    // Finds or creates the mark of `type` on `object` and registers it, which shows it while its type is visible.
    // Null when the game returns none.
    std::shared_ptr<EntityMark> AddEntityMark(Map *map, std::uint32_t type, void *object, std::uint32_t param);
    // Undoes one AddEntityMark: the last one removes the mark and its compass marker. Does not touch the marked
    // object, so it is safe once the object is gone.
    void RemoveEntityMark(Map *map, const std::shared_ptr<EntityMark> &mark);

    // The linkable object (an AI object's link node) for a WUID, which marks track; null when unknown.
    void *FindLinkable(std::uint64_t wuid);

    // An item class GUID as the game keeps it (CryGUID: the Windows GUID layout, Data1..3 little-endian, then the
    // last eight bytes in order).
    using ItemClassId = std::array<std::uint8_t, 16>;
    // Parses "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"; empty when malformed.
    std::optional<ItemClassId> ParseItemClassId(std::string_view text);
    // How far the player has read an item class, 0..1: a document counts as read at 1 (its inventory "new" marker
    // goes then).
    float ReadProgress(const ItemClassId &item);

    // The AI object manager's listener (I_ObjectManagerBaseListener): its vtable is this one function, called on the
    // main thread with kObjectRemoved when the watched object is destroyed (a load, a level unload, the object going
    // away). A listener may unwatch itself from the call.
    class ObjectListener {
      public:
        virtual void OnObjectEvent(int event) = 0;

      protected:
        ~ObjectListener() = default;
    };
    constexpr int kObjectRemoved = 1;
    // Starts and stops telling `listener` about the object with `wuid`; the WUID must outlive the watch. Unwatch only
    // what is watched: the game does not check.
    void WatchObject(const std::uint64_t *wuid, ObjectListener *listener);
    void UnwatchObject(const std::uint64_t *wuid, ObjectListener *listener);
} // namespace Game
