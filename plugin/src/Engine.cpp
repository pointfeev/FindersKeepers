#include "Engine.h"

#include "Hooks.h"
#include "Log.h"

namespace {
    // gEnv (SSystemGlobalEnvironment, 0x18492D800 in 1.5.6), found through the framework's per-frame update
    // (0x180669C88) reading gEnv->pTimer (+0x80) first thing on its main path.
    constexpr Hooks::Rung kEnvRungs[] = {
        {.pattern = "48 8B 0D ?? ?? ?? ?? 33 D2 48 8B 01 FF 50 48 48 8B 06 48 8B CE F3 0F 11 45 38 FF 50 78",
         .ripDisp = 3,
         .ripLength = 7,
         .adjust = -0x80},
        {.pattern = "48 8B 0D ?? ?? ?? ?? BA 01 00 00 00 48 8B 01 FF 50 20 0F 28 F0 0F 57 C9 F2 0F 5C B6",
         .ripDisp = 3,
         .ripLength = 7,
         .adjust = -0x80},
    };

    // The game framework's instance pointer (CCryAction, 0x18549D328), from the function that creates it ("Failed
    // to create the GameFramework Interface!") and from the game object's creation.
    constexpr Hooks::Rung kFrameworkRungs[] = {
        {.pattern =
             "40 53 48 83 EC 20 48 8B D9 48 8B 0D ?? ?? ?? ?? 48 85 C9 75 ?? 48 8D 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? "
             "48 89 05 ?? ?? ?? ?? 48 8B C8 48 85 C0 75 ?? 48 8D 15 ?? ?? ?? ?? B9 C8 00 00 00",
         .ripDisp = 12,
         .ripLength = 16},
        {.pattern =
             "48 8B 01 FF 90 98 00 00 00 48 8B 0D ?? ?? ?? ?? E8 ?? ?? ?? ?? 48 89 05 ?? ?? ?? ?? 48 8B C8 48 85 C0 "
             "74 ?? 48 8B 00 48 8B 15 ?? ?? ?? ?? FF 50 08",
         .ripDisp = 12,
         .ripLength = 16},
    };

    // gEnv members, each read by game code that then calls the interface below on it.
    constexpr std::size_t kScriptSystem = 0x28;
    constexpr std::size_t kEntitySystem = 0xA0;
    constexpr std::size_t kLog = 0xE0;
    constexpr std::size_t kRenderer = 0x108;

    // Interface slots, checked in their vtables in Ghidra.
    constexpr std::size_t kLogAlways = 12;       // CLog 0x183B27C30: 0x18079B32C, LogV(always, fmt, args)
    constexpr std::size_t kBeginCall = 15;       // script system 0x183B8C610: 0x1808FE42C (table, function name)
    constexpr std::size_t kEndCallAny = 20;      // script system: 0x180A286AC (ScriptAnyValue &result)
    constexpr std::size_t kPushParamAny = 30;    // script system: 0x180718548 (const ScriptAnyValue &)
    constexpr std::size_t kTableRelease = 4;     // CScriptTable 0x183A4A700: 0x180715EF0
    constexpr std::size_t kTableGetAny = 8;      // CScriptTable: 0x18071624C (key, ScriptAnyValue &, bool chain)
    constexpr std::size_t kGetEntity = 14;       // entity system: called by System.GetEntity, 0x18082B440 (EntityId)
    constexpr std::size_t kGetHeight = 70;       // CD3D9Renderer 0x183DFC008: 0x1809A8730
    constexpr std::size_t kGetWidth = 71;        // CD3D9Renderer: 0x1807F52A0
    constexpr std::size_t kProjectToScreen = 97; // CD3D9Renderer: 0x1809356D8 (wx, wy, wz, &sx, &sy, &sz), percent
    constexpr std::size_t kClientEntity = 66;    // CCryAction 0x1840472D0: 0x1835711A4
    constexpr std::size_t kWorldPos = 46;        // CEntity 0x184072C78: 0x180648030, translation into a Vec3
    constexpr std::size_t kEntityName = 18;      // CEntity: 0x181A76100, the name at +0xE0
    // CEntity slot 74 (proxy by type) with 2, the script proxy, whose slot 18 is its script table (0x1805D7094).
    constexpr std::size_t kEntityProxy = 74;
    constexpr int kScriptProxy = 2;
    constexpr std::size_t kProxyScriptTable = 18;

    const std::uint8_t *g_env = nullptr;
    void *const *g_framework = nullptr;

    void *Member(const std::size_t offset) {
        return g_env ? *reinterpret_cast<void *const *>(g_env + offset) : nullptr;
    }
} // namespace

bool Engine::Install() {
    g_env = reinterpret_cast<const std::uint8_t *>(Hooks::Find("gEnv", kEnvRungs));
    g_framework = reinterpret_cast<void *const *>(Hooks::Find("game framework", kFrameworkRungs));
    return g_env && g_framework;
}

void Engine::LogAlways(const char *text) {
    if (const void *log = Member(kLog))
        VCall<void>(log, kLogAlways, "%s", text);
}

int Engine::ScreenWidth() {
    const void *renderer = Member(kRenderer);
    return renderer ? VCall<int>(renderer, kGetWidth) : 0;
}

int Engine::ScreenHeight() {
    const void *renderer = Member(kRenderer);
    return renderer ? VCall<int>(renderer, kGetHeight) : 0;
}

bool Engine::ProjectToScreen(const Vec3 &world, float &x, float &y, float &z) {
    const void *renderer = Member(kRenderer);
    return renderer && VCall<bool>(renderer, kProjectToScreen, world.x, world.y, world.z, &x, &y, &z);
}

Engine::ScriptValue::~ScriptValue() {
    if (type == Table && table)
        VCall<void>(table, kTableRelease);
}

bool Engine::TableGet(const ScriptTable *table, const char *key, ScriptValue &out) {
    return VCall<bool>(table, kTableGetAny, key, &out, false);
}

bool Engine::CallMethod(const ScriptTable *self, const char *method, ScriptValue &result) {
    const void *scripts = Member(kScriptSystem);
    if (!scripts || !VCall<bool>(scripts, kBeginCall, self, method))
        return false;
    ScriptValue argument;
    argument.type = ScriptValue::Table;
    argument.table = self;
    VCall<void>(scripts, kPushParamAny, &argument);
    // Borrowed: pushing does not take a reference, so this value must not release one.
    argument.type = ScriptValue::Nil;
    return VCall<bool>(scripts, kEndCallAny, &result);
}

void *Engine::Framework() { return g_framework ? *g_framework : nullptr; }

const Engine::Entity *Engine::PlayerEntity() {
    const void *framework = Framework();
    return framework ? VCall<const Entity *>(framework, kClientEntity) : nullptr;
}

const Engine::Entity *Engine::GetEntity(const std::uint32_t id) {
    const void *entities = Member(kEntitySystem);
    return entities ? VCall<const Entity *>(entities, kGetEntity, id) : nullptr;
}

const char *Engine::EntityName(const Entity *entity) { return VCall<const char *>(entity, kEntityName); }

Engine::Vec3 Engine::EntityPosition(const Entity *entity) {
    Vec3 out;
    VCall<void>(entity, kWorldPos, &out);
    return out;
}

const Engine::ScriptTable *Engine::EntityScript(const Entity *entity) {
    const void *proxy = VCall<void *>(entity, kEntityProxy, kScriptProxy);
    return proxy ? VCall<const ScriptTable *>(proxy, kProxyScriptTable) : nullptr;
}

bool Engine::PlayerPosition(Vec3 &out) {
    const Entity *player = PlayerEntity();
    if (!player)
        return false;
    out = EntityPosition(player);
    return true;
}
