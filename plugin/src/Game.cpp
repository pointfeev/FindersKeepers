#include "Game.h"

#include <new>

#include "Hooks.h"
#include "Log.h"

namespace {
    // Most functions are found through two call sites that use several of them in a row: the dog marker
    // (0x180393678: game context, map screen, then GetOrCreateEntityMark with type 9 and RegisterEntityMark) and
    // the quest "show map marker" node (0x180C4DADC). The patterns also pin the map screen's offset, 0x1508.
    constexpr const char kDogMarker[] =
        "8B DA 48 8B F9 3B 51 20 0F 84 ?? ?? ?? ?? 89 51 20 E8 ?? ?? ?? ?? 48 8B 88 E8 00 00 00 E8 ?? ?? ?? ?? "
        "48 8D B0 08 15 00 00 85 DB 74 ?? 83 FB 01 75 ?? 48 8D 5F 28 48 8B CB E8 ?? ?? ?? ?? 84 C0 75 ?? "
        "48 8B CF E8 ?? ?? ?? ?? 4C 8B C8 C7 44 24 20 02 00 00 00 41 B8 09 00 00 00 48 8D 54 24 30 48 8B CE E8";
    constexpr const char kShowMapMarker[] =
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
    };

    // S_GameContext: +0x00 the framework (C_Framework, whose C_GameplayManager at +0xB8 keeps the started flag at
    // +0x10), +0xE8 the GUI module.
    constexpr std::size_t kFramework = 0x00;
    constexpr std::size_t kGameplayStarted = 0xB8 + 0x10;
    constexpr std::size_t kGuiModule = 0xE8;
    constexpr std::size_t kMapInApse = 0x1508;
    // A screen (C_UIFlashBase) keeps its bound element at +0x48 (its call into Flash, 0x180566BCC); the element
    // (CFlashUIElement) its player at +0x58 (its setup, 0x18088EF1C).
    constexpr std::size_t kScreenElement = 0x48;
    constexpr std::size_t kElementPlayer = 0x58;
    // C_UIMap: its map content screen at +0x208, the POI filter flags at +0x618 (0x61 of them).
    constexpr std::size_t kMapContent = 0x208;
    constexpr std::size_t kFilterFlags = 0x618;
    constexpr std::uint32_t kFilterCount = 0x61;

    using ContextFn = std::uint8_t *(__fastcall *)();
    using ScreenFn = std::uint8_t *(__fastcall *)(void *guiModule);
    using CreateMarkFn = void *(__fastcall *)(Game::Map *, std::shared_ptr<Game::EntityMark> *, std::uint32_t, void *,
                                              std::uint32_t);
    using RegisterMarkFn = void(__fastcall *)(Game::Map *, std::shared_ptr<Game::EntityMark> *);
    using FindFn = void *(__fastcall *)(void *, const std::uint64_t *);

    ContextFn g_context;
    ScreenFn g_apse;
    ScreenFn g_compass;
    CreateMarkFn g_createMark;
    RegisterMarkFn g_registerMark;
    FindFn g_find;

    template <typename T> T At(const void *base, std::size_t offset) {
        return *reinterpret_cast<const T *>(static_cast<const std::uint8_t *>(base) + offset);
    }

    void *GuiModule() {
        const std::uint8_t *context = g_context ? g_context() : nullptr;
        return context ? At<void *>(context, kGuiModule) : nullptr;
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
    g_find = reinterpret_cast<FindFn>(Hooks::Find("linkable lookup", kFindRungs));
    return g_context && g_apse && g_compass && g_createMark && g_registerMark && g_find;
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

Flash::Player *Game::MapPlayer(Map *map) {
    return ScreenPlayer(reinterpret_cast<const std::uint8_t *>(map) + kMapContent);
}

bool Game::IsTypeVisible(Map *map, std::uint32_t type) {
    // Mark types map to filter ids one to one, apart from the DLC groups (folded by 0x180A1B1C4), which we do not
    // use.
    return type < kFilterCount && At<bool>(map, kFilterFlags + type);
}

std::shared_ptr<Game::EntityMark> Game::AddEntityMark(Map *map, std::uint32_t type, void *object, std::uint32_t param) {
    std::shared_ptr<EntityMark> mark;
    g_createMark(map, &mark, type, object, param);
    if (!mark)
        return mark;
    // RegisterEntityMark takes its shared_ptr by value: the caller builds the copy, the callee releases it.
    alignas(std::shared_ptr<EntityMark>) unsigned char argument[sizeof(std::shared_ptr<EntityMark>)];
    g_registerMark(map, new (argument) std::shared_ptr<EntityMark>(mark));
    return mark;
}

void *Game::FindLinkable(std::uint64_t wuid) { return g_find(nullptr, &wuid); }
