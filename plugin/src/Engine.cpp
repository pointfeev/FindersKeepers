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
    constexpr std::size_t kLog = 0xE0;
    constexpr std::size_t kRenderer = 0x108;

    // Interface slots, checked in their vtables in Ghidra.
    constexpr std::size_t kLogAlways = 12;       // CLog 0x183B27C30: 0x18079B32C, LogV(always, fmt, args)
    constexpr std::size_t kExecuteBuffer = 6;    // script system 0x183B8C610: 0x1804D4484 (buffer, size, desc, env)
    constexpr std::size_t kGetGlobalAny = 32;    // script system: 0x180AD48E4 (name, may be dotted; ScriptAnyValue &)
    constexpr std::size_t kGetHeight = 70;       // CD3D9Renderer 0x183DFC008: 0x1809A8730
    constexpr std::size_t kGetWidth = 71;        // CD3D9Renderer: 0x1807F52A0
    constexpr std::size_t kProjectToScreen = 97; // CD3D9Renderer: 0x1809356D8 (wx, wy, wz, &sx, &sy, &sz), percent
    constexpr std::size_t kClientEntity = 66;    // CCryAction 0x1840472D0: 0x1835711A4
    constexpr std::size_t kWorldPos = 46;        // CEntity 0x184072C78: 0x180648030, translation into a Vec3

    const std::uint8_t *g_env = nullptr;
    void *const *g_framework = nullptr;

    void *Member(std::size_t offset) { return g_env ? *reinterpret_cast<void *const *>(g_env + offset) : nullptr; }
} // namespace

bool Engine::Install() {
    g_env = reinterpret_cast<const std::uint8_t *>(Hooks::Find("gEnv", kEnvRungs));
    g_framework = reinterpret_cast<void *const *>(Hooks::Find("game framework", kFrameworkRungs));
    return g_env && g_framework;
}

void Engine::LogAlways(const char *text) {
    if (void *log = Member(kLog))
        VCall<void>(log, kLogAlways, "%s", text);
}

int Engine::ScreenWidth() {
    void *renderer = Member(kRenderer);
    return renderer ? VCall<int>(renderer, kGetWidth) : 0;
}

int Engine::ScreenHeight() {
    void *renderer = Member(kRenderer);
    return renderer ? VCall<int>(renderer, kGetHeight) : 0;
}

bool Engine::ProjectToScreen(const Vec3 &world, float &x, float &y, float &z) {
    void *renderer = Member(kRenderer);
    return renderer && VCall<bool>(renderer, kProjectToScreen, world.x, world.y, world.z, &x, &y, &z);
}

bool Engine::GetGlobal(const char *name, ScriptValue &out) {
    void *scripts = Member(kScriptSystem);
    return scripts && VCall<bool>(scripts, kGetGlobalAny, name, &out);
}

bool Engine::Execute(std::string_view code, const char *description) {
    void *scripts = Member(kScriptSystem);
    return scripts &&
           VCall<bool>(scripts, kExecuteBuffer, code.data(), code.size(), description, static_cast<void *>(nullptr));
}

void *Engine::Framework() { return g_framework ? *g_framework : nullptr; }

bool Engine::PlayerPosition(Vec3 &out) {
    void *framework = Framework();
    void *entity = framework ? VCall<void *>(framework, kClientEntity) : nullptr;
    if (!entity)
        return false;
    VCall<void>(entity, kWorldPos, &out);
    return true;
}
