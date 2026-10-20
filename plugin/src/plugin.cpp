#include <windows.h>

#include <optional>
#include <vector>

#include "Engine.h"
#include "FloatingIcon.h"
#include "Game.h"
#include "GameMark.h"
#include "Hooks.h"
#include "Log.h"
#include "MapImportance.h"
#include "MarkType.h"
#include "Probe.h"

// A KCSE plugin (see the exports at the end) that brings its own game access: it runs on the game framework's
// per-frame update.
namespace {
    constexpr ULONGLONG kProbeIntervalMs = 2000;

    ULONGLONG g_nextProbe = 0;
    // Empty until the first probe after start-up or a save load, so that one is always logged.
    std::optional<ProbeResult> g_last;
    // The last probe's targets, for the floating icon every frame.
    std::vector<Engine::Vec3> g_positions;
    bool g_announced = false;
    bool g_wasStarted = false;

    // Logs the probe when its result changes, so picking up a map or digging up its treasure shows up in the log.
    void LogProbe(const ProbeResult &r) {
        if (g_last && *g_last == r)
            return;
        g_last = r;
        if (r.status != ProbeResult::Status::Ran) {
            Log::Write("probe: {}", Probe::StatusName(r.status));
            return;
        }
        Log::Write("probe: player={} targets={}{}{}", r.playerReady, r.targets.size(),
                   r.errors.empty() ? "" : " errors: ", r.errors);
        for (const auto &t : r.targets)
            Log::Write("probe: {} leads to WUID {:016x} at ({:.1f}, {:.1f}, {:.1f})", t.map, t.wuid, t.pos.x, t.pos.y,
                       t.pos.z);
    }

    void Tick() {
        if (!g_announced) {
            Log::Write("running");
            g_announced = true;
            // On the game's main thread rather than KCSE's start-up thread, so code the game is running is not
            // patched under it.
            if (MarkType::Install())
                MapImportance::Install();
        }
        const bool started = Game::GameplayStarted();
        if (started != g_wasStarted) {
            if (started) {
                // A save finished loading or a new game started: probe afresh.
                g_last.reset();
                g_nextProbe = 0;
            } else {
                // Loading is starting: marks go before the objects they point at.
                GameMark::Clear();
                g_positions.clear();
            }
        }
        g_wasStarted = started;

        if (const ULONGLONG now = GetTickCount64(); started && now >= g_nextProbe) {
            g_nextProbe = now + kProbeIntervalMs;
            LogProbe(Probe::Run());
            g_positions.clear();
            for (const auto &t : g_last->targets)
                g_positions.push_back(t.pos);
            GameMark::Sync(g_last->targets);
        }
        const auto *map = Game::GetMap();
        FloatingIcon::Update(g_positions, map && Game::IsTypeVisible(map, MarkType::kId));
    }

    // CCryAction::PostUpdate (vtable slot 11, 0x180669C88): (framework, haveFocus, updateFlags), once per frame. KCSE
    // swaps the same slot before loading plugins; ours calls whatever the slot held.
    constexpr std::size_t kPostUpdate = 11;
    using FrameFn = void(__fastcall *)(void *, bool, unsigned);
    FrameFn g_frame;

    void __fastcall FrameUpdate(void *framework, const bool haveFocus, const unsigned flags) {
        g_frame(framework, haveFocus, flags);
        Tick();
    }

    // Finds what the plugin needs and hooks the frame update, which installs the rest on the first frame.
    bool Install() {
        if (!Engine::Install() || !Game::Install()) {
            Log::Write("not started: game code not found (an unsupported game version?)");
            return false;
        }
        void *framework = Engine::Framework();
        if (!framework) {
            Log::Write("not started: the game framework does not exist yet");
            return false;
        }
        g_frame = reinterpret_cast<FrameFn>(
            Hooks::ReplaceVirtual(framework, kPostUpdate, reinterpret_cast<void *>(&FrameUpdate)));
        return true;
    }
} // namespace

// KCSE loads DLLs from Mods/<mod>/KCSE/Plugins that export these two symbols: the plugin's description, then the
// load call, made on KCSE's start-up thread once WHGame.dll is loaded and the game framework exists. The plugin uses
// nothing else from KCSE.
extern "C" {
// KCSE's PluginVersionData (version 1).
struct KcsePluginVersion {
    std::uint32_t dataVersion;
    std::uint32_t pluginVersion;
    char name[256];
    char author[256];
    std::uint32_t compatibleGameVersions[16]; // none listed: any game version
    std::uint32_t kcseVersionRequired;
    std::uint32_t versionIndependence; // 1: finds game code by signature
};

__declspec(dllexport) KcsePluginVersion KCSEPlugin_Version = {.dataVersion = 1,
                                                              .pluginVersion = 1,
                                                              .name = "TreasureFinder",
                                                              .author = "pointfeev",
                                                              .compatibleGameVersions = {},
                                                              .kcseVersionRequired = 0,
                                                              .versionIndependence = 1};

__declspec(dllexport) bool KCSEPlugin_Load(const void *) { return Install(); }
}
