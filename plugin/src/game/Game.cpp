#include "game/Game.h"

#include <new>

#include "Log.h"
#include "game/Engine.h"
#include "game/Hooks.h"

namespace {
    // Most functions are found through two call sites that use several of them in a row: the dog marker
    // (0x180393678: game context, map screen, then GetOrCreateEntityMark with type 9 and RegisterEntityMark) and
    // the quest "show map marker" node (0x180C4DADC). The patterns also pin the map screen's offset, 0x1508.
    constexpr char kDogMarker[] =
        "8B DA 48 8B F9 3B 51 20 0F 84 ?? ?? ?? ?? 89 51 20 E8 ?? ?? ?? ?? 48 8B 88 E8 00 00 00 E8 ?? ?? ?? ?? "
        "48 8D B0 08 15 00 00 85 DB 74 ?? 83 FB 01 75 ?? 48 8D 5F 28 48 8B CB E8 ?? ?? ?? ?? 84 C0 75 ?? "
        "48 8B CF E8 ?? ?? ?? ?? 4C 8B C8 C7 44 24 20 02 00 00 00 41 B8 09 00 00 00 48 8D 54 24 30 48 8B CE E8";
    constexpr char kShowMapMarker[] =
        "48 8B D9 48 81 C1 90 00 00 00 E8 ?? ?? ?? ?? 48 8B F8 48 85 C0 0F 84 ?? ?? ?? ?? E8 ?? ?? ?? ?? "
        "48 8B 88 E8 00 00 00 E8 ?? ?? ?? ?? 48 8D 8B D0 00 00 00 48 8D B3 10 01 00 00 48 8D A8 08 15 00 00";

    // S_GameContext::Instance (0x1809155C8).
    constexpr Hooks::Rung kContextRungs[] = {
        {.pattern = kDogMarker, .ripDisp = 18, .ripLength = 22},
        {.pattern = kShowMapMarker, .ripDisp = 28, .ripLength = 32},
    };
    // The GUI module's apse screen (0x1805677CC, a cached rttr lookup); the map screen is at +0x1508 in it.
    constexpr Hooks::Rung kApseRungs[] = {
        {.pattern = kDogMarker, .ripDisp = 30, .ripLength = 34},
        {.pattern = kShowMapMarker, .ripDisp = 40, .ripLength = 44},
    };
    // C_UIMap::GetOrCreateEntityMark (0x180C4DBD8): (map, shared_ptr *out, type, object, param).
    constexpr Hooks::Rung kCreateMarkRungs[] = {
        {.pattern = kDogMarker, .ripDisp = 100, .ripLength = 104},
        {.pattern =
             "48 89 5C 24 10 44 89 44 24 18 55 56 57 48 8B EC 48 83 EC 60 8B 45 E4 48 8B FA 48 8B 91 A0 05 00 00"},
    };
    // C_UIMap::RegisterEntityMark (0x180C4DD5C): (map, shared_ptr by value), which releases its argument.
    constexpr Hooks::Rung kRegisterMarkRungs[] = {
        {.pattern =
             "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 40 4C 8D 89 A0 05 00 00 48 8B F9 49 8B 49 08 48 8B F2 49 8B 01 "
             "48 3B C1"},
        {.pattern = "48 8B D3 48 8D 4C 24 30 E8 ?? ?? ?? ?? 48 8B D0 48 8B CE E8 ?? ?? ?? ?? EB ?? 48 8D 57 28",
         .ripDisp = 20,
         .ripLength = 24},
    };
    // The GUI module's compass screen (0x180C4E098, a cached rttr lookup), from RegisterEntityMark and the quest
    // marker producer.
    constexpr Hooks::Rung kCompassRungs[] = {
        {.pattern = "E8 ?? ?? ?? ?? 83 CB FF 84 C0 0F 84 ?? ?? ?? ?? 41 8B D2 E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? "
                    "4C 8B 06 48 8D 4C 24 20 4D 8D 48 08 49 8D 50 04 4D 8B 40 10 E8 ?? ?? ?? ?? 48 8B 4F 08 E8",
         .ripDisp = 62,
         .ripLength = 66},
        {.pattern = "48 81 EC A0 00 00 00 48 8B FA 49 8B D8 49 8B D0 48 8B F1 E8 ?? ?? ?? ?? 84 C0 0F 84 ?? ?? ?? ?? "
                    "48 8B 4E 08 E8 ?? ?? ?? ?? 48 8D 53 10 4C 8B F8",
         .ripDisp = 37,
         .ripLength = 41},
    };
    // The quest "show map marker" node's teardown (0x180A19B80): XGenAI's object manager, unwatching the marked
    // object, then RemoveEntityMark. Its setup (0x180C4DADC) watches the object the same way.
    constexpr char kMarkerTeardown[] =
        "E8 ?? ?? ?? ?? 48 8B 90 60 01 00 00 48 8B 8A A8 00 00 00 48 8B 01 FF 90 F0 00 00 00 48 8B 0B "
        "4C 8D 87 88 00 00 00 48 8B 51 10 48 8B C8 48 83 C2 08 E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 88 E8 00 00 00 "
        "E8 ?? ?? ?? ?? 48 8B D3 48 8D 4C 24 20 4C 8D 80 08 15 00 00 E8 ?? ?? ?? ?? 48 8B D0 49 8B C8 E8";
    // C_UIMap::RemoveEntityMark (0x180A182F0): (map, shared_ptr by value), which releases its argument.
    constexpr Hooks::Rung kRemoveMarkRungs[] = {
        {.pattern = kMarkerTeardown, .ripDisp = 98, .ripLength = 102},
    };
    // The object manager's watch (0x180C9B198) and unwatch (0x180A198C0): (manager, const WUID *, listener).
    constexpr Hooks::Rung kWatchRungs[] = {
        {.pattern =
             "E8 ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B 88 60 01 00 00 48 8B 89 A8 00 00 00 48 8B 01 FF 90 F0 00 00 00 "
             "48 8B 0E 4C 8D 83 88 00 00 00 48 8B 51 10 48 8B C8 48 83 C2 08 E8",
         .ripDisp = 55,
         .ripLength = 59},
    };
    constexpr Hooks::Rung kUnwatchRungs[] = {
        {.pattern = kMarkerTeardown, .ripDisp = 50, .ripLength = 54},
    };
    // The linkable object for a WUID (0x18047C1AC): looks the WUID up in the AI object manager and returns its link
    // node, ignoring rcx.
    constexpr Hooks::Rung kFindRungs[] = {
        {.pattern =
             "48 83 EC 28 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8B C8 48 83 C4 28 E9 ?? ?? ?? ?? "
             "48 83 C4 28 C3 CC CC 48 83 EC 28 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B C8 33 C0 48 85 C9 74 ?? "
             "48 8B 41 18"},
        {.pattern =
             "48 83 EC 28 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 8B C8 33 C0 48 85 C9 74 ?? 48 8B 41 18 48 83 C4 28 "
             "C3 CC 40 53 48 83 EC 20 48 8B D9 41 B8 08 00 00 00 48 B9 25 23 22 84 E4 9C F2 CB",
         .offset = -0x28},
    }; // An entity's WUID, as XGenAIModule.GetMyWUID finds it (0x1819771B8): the entity GUID (entity slot 2) looked up
    // in the GUID -> WUID map (global 0x185496338, lookup 0x18047E638, which returns a pointer to the WUID).
    constexpr char kGetMyWuid[] =
        "48 8B 03 48 8B CB 8B 54 24 48 FF 50 70 48 85 C0 0F 84 ?? ?? ?? ?? 48 8B 10 48 8B C8 48 8B 1D ?? ?? ?? ?? "
        "FF 52 10 48 8B D0 48 8B CB E8 ?? ?? ?? ?? 48 8B 08";
    constexpr Hooks::Rung kWuidMapRungs[] = {{.pattern = kGetMyWuid, .ripDisp = 31, .ripLength = 35}};
    constexpr Hooks::Rung kWuidLookupRungs[] = {{.pattern = kGetMyWuid, .ripDisp = 45, .ripLength = 49}};
    // An inventory's count of an item class, as inventory:GetCountOfClass finds it (0x182ABB538): the inventory's
    // item index at +0xE8 and the class GUID (0x180514834).
    constexpr Hooks::Rung kCountOfClassRungs[] = {
        {.pattern = "48 8D 8F E8 00 00 00 48 8D 54 24 28 E8 ?? ?? ?? ?? 48 8D 54 24 20 89 44 24 20",
         .ripDisp = 13,
         .ripLength = 17},
    };

    // An item slot by EntityId (0x18047DE0C: the entity's proxy 0x12 asked for its C_ItemSlot, null for other
    // entities), from the Skald item slot check that then reads the slot's item at +0x98 (0x180D595D4).
    constexpr Hooks::Rung kItemSlotRungs[] = {
        {.pattern = "48 8B 06 48 8B CE FF 50 08 8B C8 E8 ?? ?? ?? ?? 48 85 C0 0F 84 ?? ?? ?? ?? 48 8B B0 98 00 00 00",
         .ripDisp = 12,
         .ripLength = 16},
    };
    // The quest log's tracking toggle (0x180DC760C) finds a quest by name in the quest manager (0x180DC4988:
    // (manager, quest pointer *out, const char *const *name), null when there is none) and reads its progress
    // (0x1823D5EF0), in two places.
    constexpr char kTrackQuest[] =
        "E8 ?? ?? ?? ?? 4C 8D 45 ?? 48 8D 55 ?? 48 8B 88 18 01 00 00 48 8B 49 10 E8 ?? ?? ?? ?? "
        "48 8B 4D ?? E8 ?? ?? ?? ?? 83 F8 01";
    constexpr char kTrackQuestOther[] = "E8 ?? ?? ?? ?? 4C 8D 45 ?? 48 8D 55 ?? 48 8B 88 18 01 00 00 48 8B 49 10 "
                                        "E8 ?? ?? ?? ?? 48 8B 5D ?? 48 8B CB E8";
    constexpr Hooks::Rung kFindQuestRungs[] = {
        {.pattern = kTrackQuest, .ripDisp = 25, .ripLength = 29},
        {.pattern = kTrackQuestOther, .ripDisp = 25, .ripLength = 29},
    };
    constexpr Hooks::Rung kQuestProgressRungs[] = {
        {.pattern = kTrackQuest, .ripDisp = 34, .ripLength = 38},
        {.pattern = kTrackQuestOther, .ripDisp = 37, .ripLength = 41},
    };
    // Releasing a quest pointer (0x1804C44A0: (quest pointer *)), from the active quest walk (0x180DC4AD0).
    constexpr Hooks::Rung kReleaseQuestRungs[] = {
        {.pattern =
             "4C 8D 44 24 30 48 8B D0 48 8B CB E8 ?? ?? ?? ?? 48 8D 4C 24 30 E8 ?? ?? ?? ?? 48 83 C7 08 48 3B FE 75",
         .ripDisp = 22,
         .ripLength = 26},
    };
    // The map screen's quest marker refresh (0x180DC577C) gathers the markers from the quest log screen at apse
    // +0x10C8 (0x180DC5E68: (quest log, records *out)) and frees them (0x1823D5EFC: (records *)).
    constexpr Hooks::Rung kGatherMarkersRungs[] = {
        {.pattern = "4C 8B F9 48 8D 55 ?? 48 8B 89 70 03 00 00 48 81 C1 C8 10 00 00 E8",
         .ripDisp = 22,
         .ripLength = 26},
        {.pattern = "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 50 48 8B F2 48 C7 02 00 00 00 00 48 8B D9 "
                    "48 C7 42 08 00 00 00 00 48 C7 42 10 00 00 00 00 E8 ?? ?? ?? ?? 48 8D 54 24 30 "
                    "48 8B 88 18 01 00 00 48 8B 49 10"},
    };
    constexpr Hooks::Rung kFreeMarkersRungs[] = {
        {.pattern = "E8 ?? ?? ?? ?? 48 8D 4D 07 E8 ?? ?? ?? ?? 48 8D 4D EF E8 ?? ?? ?? ?? 4C 8D 9C 24 C0 00 00 00",
         .ripDisp = 1,
         .ripLength = 5},
        {.pattern = "48 8D 4D EF E8 ?? ?? ?? ?? 4C 8D 9C 24 C0 00 00 00 49 8B 5B 38 49 8B 73 40 49 8B E3 "
                    "41 5F 41 5E 41 5C 5F 5D C3",
         .ripDisp = 5,
         .ripLength = 9},
    };
    // The map's quest marker (0x1814A81D0) tells a search area from a point by its target's WUID (0x1814A8444) and
    // looks the area up (0x1823D14E8: the area, or null); neither reads rcx.
    constexpr char kQuestMarkerTarget[] =
        "E8 ?? ?? ?? ?? 48 8B 88 60 01 00 00 48 8B 89 A8 00 00 00 48 8B 01 FF 50 78 48 8B 53 08 E8 ?? ?? ?? ?? "
        "84 C0 0F 84 ?? ?? ?? ?? 48 8B 53 08 E8";
    constexpr Hooks::Rung kIsAreaRungs[] = {{.pattern = kQuestMarkerTarget, .ripDisp = 30, .ripLength = 34}};
    constexpr Hooks::Rung kFindAreaRungs[] = {{.pattern = kQuestMarkerTarget, .ripDisp = 47, .ripLength = 51}};

    // S_GameContext: +0x00 the framework (C_Framework, whose C_GameplayManager at +0xB8 keeps the started flag at
    // +0x10), +0xE8 the GUI module.
    constexpr std::size_t kFramework = 0x00;
    constexpr std::size_t kGameplayStarted = 0xB8 + 0x10;
    constexpr std::size_t kGuiModule = 0xE8;
    // S_GameContext +0x160 is XGenAI, whose +0xA8 hands out the object manager from its slot 30.
    constexpr std::size_t kXGenAi = 0x160;
    constexpr std::size_t kXGenAiObjects = 0xA8;
    constexpr std::size_t kObjectManagerSlot = 30;
    constexpr std::size_t kMapInApse = 0x1508;
    // A screen (C_UIFlashBase) keeps its bound element at +0x48 (its call into Flash, 0x180566BCC); the element
    // (CFlashUIElement) its player at +0x58 (its setup, 0x18088EF1C).
    constexpr std::size_t kScreenElement = 0x48;
    constexpr std::size_t kElementPlayer = 0x58;
    // C_UIMap: its map content screen at +0x208, the POI filter flags at +0x618 (0x61 of them).
    constexpr std::size_t kMapContent = 0x208;
    constexpr std::size_t kFilterFlags = 0x618;
    constexpr std::uint32_t kFilterCount = 0x61;
    // S_GameContext +0x130 is the RPG module, whose +0x98 tracks book reading: slot 36 gives an item class's reading
    // progress (0..1) by its GUID. The inventory's document info marks a document new while it is below 1
    // (C_UIFlashInfoItemDocument, 0x18119EDD8).
    constexpr std::size_t kRpgModule = 0x130;
    constexpr std::size_t kReading = 0x98;
    constexpr std::size_t kReadProgressSlot = 0x120 / 8;
    // C_Inventory: its items in a vector at +8 (inventory:GetCount, 0x182ABB49C), the class index at +0xE8.
    constexpr std::size_t kInventoryItems = 0x08;
    constexpr std::size_t kInventoryClassIndex = 0xE8;
    constexpr std::size_t kEntityGuidSlot = 2; // CEntity: 0x180638480, the EntityGUID at +0x10
    // C_ItemSlot: the item it holds at +0x98, null once taken (IsItemInSlot, 0x1809E5498).
    constexpr std::size_t kSlotItem = 0x98;
    // S_GameContext +0x118 holds the quest module, whose +0x10 is the quest manager.
    constexpr std::size_t kQuestModule = 0x118;
    constexpr std::size_t kQuestManager = 0x10;
    // The apse screen's quest log, whose pointer at +0x420 (by its use, back to the apse) the marker gather
    // dereferences (0x181F45A20).
    constexpr std::size_t kQuestLogInApse = 0x10C8;
    constexpr std::size_t kQuestLogApse = 0x420;
    // A gathered marker record (0x38 bytes, built in 0x181F457F0) points at its target at +0x30: the target's slot 2
    // gives its position, and its WUID is at +8. An area's slot 3 fills its bounds (min, then max) into a buffer and
    // returns them (0x1814A81D0).
    constexpr std::size_t kMarkerRecordSize = 0x38;
    constexpr std::size_t kMarkerTarget = 0x30;
    constexpr std::size_t kTargetPositionSlot = 2;
    constexpr std::size_t kTargetWuid = 0x08;
    constexpr std::size_t kAreaBoundsSlot = 3;

    using ContextFn = std::uint8_t *(__fastcall *)();
    using ScreenFn = std::uint8_t *(__fastcall *)(void *guiModule);
    using CreateMarkFn = void *(__fastcall *)(Game::Map *, std::shared_ptr<Game::EntityMark> *, std::uint32_t, void *,
                                              std::uint32_t);
    using RegisterMarkFn = void(__fastcall *)(Game::Map *, std::shared_ptr<Game::EntityMark> *);
    using WatchFn = void(__fastcall *)(void *manager, const std::uint64_t *, Game::ObjectListener *);
    using FindFn = void *(__fastcall *)(void *, const std::uint64_t *);
    using WuidLookupFn = const std::uint64_t *(__fastcall *)(const void *map, std::uint64_t entityGuid);
    using CountOfClassFn = int(__fastcall *)(const void *index, const Game::ItemClassId *);
    using ItemSlotFn = const void *(__fastcall *)(std::uint32_t entityId);
    using FindQuestFn = void **(__fastcall *)(void *manager, void **out, const char *const *name);
    using QuestProgressFn = int(__fastcall *)(void *quest);
    using ReleaseQuestFn = void(__fastcall *)(void **quest);
    // The records vector the marker gather fills (an MSVC std::vector of 0x38-byte records).
    struct MarkerRecords {
        const std::uint8_t *begin = nullptr, *end = nullptr, *capacity = nullptr;
    };
    using GatherMarkersFn = MarkerRecords *(__fastcall *)(void *questLog, MarkerRecords *out);
    using FreeMarkersFn = void(__fastcall *)(MarkerRecords *);
    using IsAreaFn = bool(__fastcall *)(const void *, std::uint64_t wuid);
    using FindAreaFn = const void *(__fastcall *)(const void *, std::uint64_t wuid);

    ContextFn g_context;
    ScreenFn g_apse;
    ScreenFn g_compass;
    CreateMarkFn g_createMark;
    RegisterMarkFn g_registerMark;
    RegisterMarkFn g_removeMark;
    WatchFn g_watch;
    WatchFn g_unwatch;
    FindFn g_find;
    const void *const *g_wuidMap;
    WuidLookupFn g_wuidLookup;
    CountOfClassFn g_countOfClass;
    ItemSlotFn g_itemSlot;
    FindQuestFn g_findQuest;
    QuestProgressFn g_questProgress;
    ReleaseQuestFn g_releaseQuest;
    GatherMarkersFn g_gatherMarkers;
    FreeMarkersFn g_freeMarkers;
    IsAreaFn g_isArea;
    FindAreaFn g_findArea;

    template <typename T> T At(const void *base, const std::size_t offset) {
        return *reinterpret_cast<const T *>(static_cast<const std::uint8_t *>(base) + offset);
    }

    void *GuiModule() {
        const std::uint8_t *context = g_context ? g_context() : nullptr;
        return context ? At<void *>(context, kGuiModule) : nullptr;
    }

    void *ObjectManager() {
        const std::uint8_t *context = g_context();
        return Engine::VCall<void *>(At<void *>(At<void *>(context, kXGenAi), kXGenAiObjects), kObjectManagerSlot);
    }

    // Calls a game function that takes a shared_ptr by value: the caller builds the copy, the callee releases it.
    void PassMark(const RegisterMarkFn function, Game::Map *map, const std::shared_ptr<Game::EntityMark> &mark) {
        alignas(std::shared_ptr<Game::EntityMark>) unsigned char argument[sizeof(std::shared_ptr<Game::EntityMark>)];
        function(map, new (argument) std::shared_ptr(mark));
    }

    Flash::Player *ScreenPlayer(const void *screen) {
        const void *element = screen ? At<void *>(screen, kScreenElement) : nullptr;
        return element ? At<Flash::Player *>(element, kElementPlayer) : nullptr;
    }
} // namespace

bool Game::Install() {
    g_context = reinterpret_cast<ContextFn>(Hooks::Find("game context", kContextRungs));
    g_apse = reinterpret_cast<ScreenFn>(Hooks::Find("apse screen", kApseRungs));
    g_compass = reinterpret_cast<ScreenFn>(Hooks::Find("compass screen", kCompassRungs));
    g_createMark = reinterpret_cast<CreateMarkFn>(Hooks::Find("GetOrCreateEntityMark", kCreateMarkRungs));
    g_registerMark = reinterpret_cast<RegisterMarkFn>(Hooks::Find("RegisterEntityMark", kRegisterMarkRungs));
    g_removeMark = reinterpret_cast<RegisterMarkFn>(Hooks::Find("RemoveEntityMark", kRemoveMarkRungs));
    g_watch = reinterpret_cast<WatchFn>(Hooks::Find("object watch", kWatchRungs));
    g_unwatch = reinterpret_cast<WatchFn>(Hooks::Find("object unwatch", kUnwatchRungs));
    g_find = reinterpret_cast<FindFn>(Hooks::Find("linkable lookup", kFindRungs));
    g_wuidMap = reinterpret_cast<const void *const *>(Hooks::Find("WUID map", kWuidMapRungs));
    g_wuidLookup = reinterpret_cast<WuidLookupFn>(Hooks::Find("WUID lookup", kWuidLookupRungs));
    g_countOfClass = reinterpret_cast<CountOfClassFn>(Hooks::Find("item class count", kCountOfClassRungs));
    g_itemSlot = reinterpret_cast<ItemSlotFn>(Hooks::Find("item slot lookup", kItemSlotRungs));
    g_findQuest = reinterpret_cast<FindQuestFn>(Hooks::Find("quest lookup", kFindQuestRungs));
    g_questProgress = reinterpret_cast<QuestProgressFn>(Hooks::Find("quest progress", kQuestProgressRungs));
    g_releaseQuest = reinterpret_cast<ReleaseQuestFn>(Hooks::Find("quest release", kReleaseQuestRungs));
    g_gatherMarkers = reinterpret_cast<GatherMarkersFn>(Hooks::Find("quest marker gather", kGatherMarkersRungs));
    g_freeMarkers = reinterpret_cast<FreeMarkersFn>(Hooks::Find("quest marker free", kFreeMarkersRungs));
    g_isArea = reinterpret_cast<IsAreaFn>(Hooks::Find("area WUID check", kIsAreaRungs));
    g_findArea = reinterpret_cast<FindAreaFn>(Hooks::Find("area lookup", kFindAreaRungs));
    return g_context && g_apse && g_compass && g_createMark && g_registerMark && g_removeMark && g_watch && g_unwatch &&
           g_find && g_wuidMap && g_wuidLookup && g_countOfClass && g_itemSlot && g_findQuest && g_questProgress &&
           g_releaseQuest && g_gatherMarkers && g_freeMarkers && g_isArea && g_findArea;
}

bool Game::GameplayStarted() {
    const std::uint8_t *context = g_context ? g_context() : nullptr;
    const void *framework = context ? At<void *>(context, kFramework) : nullptr;
    return framework && At<bool>(framework, kGameplayStarted);
}

Flash::Player *Game::HudPlayer() {
    void *gui = GuiModule();
    return gui ? ScreenPlayer(g_compass(gui)) : nullptr;
}

Game::Map *Game::GetMap() {
    void *gui = GuiModule();
    std::uint8_t *apse = gui ? g_apse(gui) : nullptr;
    return apse ? reinterpret_cast<Map *>(apse + kMapInApse) : nullptr;
}

Flash::Player *Game::MapPlayer(const Map *map) {
    return ScreenPlayer(reinterpret_cast<const std::uint8_t *>(map) + kMapContent);
}

bool Game::IsTypeVisible(const Map *map, const std::uint32_t type) {
    // Mark types map to filter ids one to one, apart from the DLC groups (folded by 0x180A1B1C4), which we do not
    // use.
    return type < kFilterCount && At<bool>(map, kFilterFlags + type);
}

std::shared_ptr<Game::EntityMark> Game::AddEntityMark(Map *map, const std::uint32_t type, void *object,
                                                      const std::uint32_t param) {
    std::shared_ptr<EntityMark> mark;
    g_createMark(map, &mark, type, object, param);
    if (mark)
        PassMark(g_registerMark, map, mark);
    return mark;
}

void Game::RemoveEntityMark(Map *map, const std::shared_ptr<EntityMark> &mark) { PassMark(g_removeMark, map, mark); }

void *Game::FindLinkable(const std::uint64_t wuid) { return g_find(nullptr, &wuid); }

float Game::ReadProgress(const ItemClassId &item) {
    const std::uint8_t *context = g_context();
    const void *reading = At<void *>(At<void *>(context, kRpgModule), kReading);
    return Engine::VCall<float>(reading, kReadProgressSlot, &item);
}

const Game::Inventory *Game::EntityInventory(const Engine::Entity *entity) {
    // The script's `inventory` table carries its C_Inventory as `__this` (made in 0x180A2F6F4).
    const Engine::ScriptTable *script = Engine::EntityScript(entity);
    Engine::ScriptValue table, self;
    if (!script || !Engine::TableGet(script, "inventory", table) || table.type != Engine::ScriptValue::Table ||
        !Engine::TableGet(static_cast<const Engine::ScriptTable *>(table.table), "__this", self) ||
        self.type != Engine::ScriptValue::Handle)
        return nullptr;
    return reinterpret_cast<const Inventory *>(self.handle);
}

int Game::ItemCount(const Inventory *inventory) {
    const auto *items = reinterpret_cast<const std::uint8_t *>(inventory) + kInventoryItems;
    return static_cast<int>((At<std::uintptr_t>(items, 8) - At<std::uintptr_t>(items, 0)) / sizeof(void *));
}

int Game::CountOfClass(const Inventory *inventory, const ItemClassId &item) {
    return g_countOfClass(reinterpret_cast<const std::uint8_t *>(inventory) + kInventoryClassIndex, &item);
}

std::uint64_t Game::EntityWuid(const Engine::Entity *entity) {
    return *g_wuidLookup(*g_wuidMap, Engine::VCall<std::uint64_t>(entity, kEntityGuidSlot));
}

std::optional<bool> Game::SlotHoldsItem(const std::uint32_t entityId) {
    const void *slot = g_itemSlot(entityId);
    if (!slot)
        return std::nullopt;
    return At<void *>(slot, kSlotItem) != nullptr;
}

Game::QuestProgress Game::GetQuestProgress(const char *name) {
    void *manager = At<void *>(At<void *>(g_context(), kQuestModule), kQuestManager);
    void *quest = nullptr;
    g_findQuest(manager, &quest, &name);
    if (!quest)
        return QuestProgress::None;
    const auto progress = static_cast<QuestProgress>(g_questProgress(quest));
    g_releaseQuest(&quest);
    return progress;
}

std::vector<Game::QuestMarker> Game::QuestMarkers() {
    std::vector<QuestMarker> markers;
    void *gui = GuiModule();
    std::uint8_t *apse = gui ? g_apse(gui) : nullptr;
    // The gather dereferences the quest log's +0x420 pointer; without one the quest log is not set up yet.
    if (!apse || !At<void *>(apse + kQuestLogInApse, kQuestLogApse))
        return markers;
    MarkerRecords records;
    g_gatherMarkers(apse + kQuestLogInApse, &records);
    for (const std::uint8_t *record = records.begin; record != records.end; record += kMarkerRecordSize) {
        const void *target = At<void *>(record, kMarkerTarget);
        const auto wuid = At<std::uint64_t>(target, kTargetWuid);
        if (const void *area = g_isArea(nullptr, wuid) ? g_findArea(nullptr, wuid) : nullptr) {
            float buffer[6];
            const float *bounds = Engine::VCall<const float *>(area, kAreaBoundsSlot, buffer);
            markers.push_back({.min = {bounds[0], bounds[1], bounds[2]}, .max = {bounds[3], bounds[4], bounds[5]}});
        } else {
            const Engine::Vec3 pos = *Engine::VCall<const Engine::Vec3 *>(target, kTargetPositionSlot);
            markers.push_back({.min = pos, .max = pos});
        }
    }
    g_freeMarkers(&records);
    return markers;
}

void Game::WatchObject(const std::uint64_t *wuid, ObjectListener *listener) {
    g_watch(ObjectManager(), wuid, listener);
}

void Game::UnwatchObject(const std::uint64_t *wuid, ObjectListener *listener) {
    g_unwatch(ObjectManager(), wuid, listener);
}
