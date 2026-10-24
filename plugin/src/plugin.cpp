#include <windows.h>

#include <algorithm>
#include <optional>
#include <vector>

#include "Log.h"
#include "game/Engine.h"
#include "game/Game.h"
#include "game/Hooks.h"
#include "hud/FloatingIcon.h"
#include "marks/GameMark.h"
#include "marks/MapImportance.h"
#include "marks/MarkType.h"
#include "options/Mcm.h"
#include "options/Options.h"
#include "probe/Probe.h"

// A KCSE plugin (see the exports at the end). It finds the game code it needs itself and runs on the game framework's
// per-frame update.
namespace {
    // Often enough that marks follow what the player does at once; one probe costs a few dozen lookups.
    constexpr ULONGLONG kProbeIntervalMs = 100;

    ULONGLONG g_nextProbe = 0;
    // Empty until the first probe after start-up or a save load, so that one is always logged.
    std::optional<ProbeResult> g_last;
    // The last probe's marked entities, for the floating icons every frame: destinations, then map locations.
    std::vector<std::uint32_t> g_destinations;
    std::vector<std::uint32_t> g_sources;
    std::uint32_t g_optionsVersion = 0;
    bool g_announced = false;
    bool g_wasStarted = false;
    bool g_cutscene = false;
    bool g_dialogue = false;

    // Whether two probes mark the same things; positions are left out, since an NPC holding a map moves.
    bool SameMarks(const ProbeResult &a, const ProbeResult &b) {
        return a.playerReady == b.playerReady &&
               std::ranges::equal(a.targets, b.targets, [](const auto &x, const auto &y) {
                   return x.map == y.map && x.source == y.source && x.entity == y.entity && x.wuid == y.wuid;
               });
    }

    // Logs the probe when what it marks changes, so picking up a map or digging up its treasure shows up in the log.
    void LogProbe(const ProbeResult &r) {
        const bool same = g_last && SameMarks(*g_last, r);
        g_last = r;
        if (same)
            return;
        Log::Write("probe: player={} targets={}", r.playerReady, r.targets.size());
        for (const auto &t : r.targets)
            Log::Write("probe: {} {} entity {} WUID {:016x} at ({:.1f}, {:.1f}, {:.1f})", t.map,
                       t.source ? "lies at" : "leads to", t.entity, t.wuid, t.pos.x, t.pos.y, t.pos.z);
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
                Probe::Reset();
                g_nextProbe = 0;
            } else {
                // Loading is starting: marks go before the objects they point at.
                GameMark::Clear();
                g_destinations.clear();
                g_sources.clear();
            }
        }
        g_wasStarted = started;

        if (const ULONGLONG now = GetTickCount64(); started && now >= g_nextProbe) {
            g_nextProbe = now + kProbeIntervalMs;
            Options::Refresh();
            const ProbeResult result = Probe::Run();
            LogProbe(result);
            g_destinations.clear();
            g_sources.clear();
            for (const auto &t : result.targets)
                (t.source ? g_sources : g_destinations).push_back(t.entity);
            GameMark::Sync(Options::Get().mapMarkers ? std::span(result.targets)
                                                     : std::span<const ProbeResult::Target>());
        }
        // A setting changed (the menu or the file): probe again at once.
        if (Options::Version() != g_optionsVersion) {
            g_optionsVersion = Options::Version();
            g_nextProbe = 0;
        }
        // Icons hide while a cutscene plays or the player is in a dialogue. Neither is asked while a save loads, when
        // there are no icons to hide and the dialogue manager may be mid-reset.
        const bool cutscene = started && Game::CutscenePlaying();
        if (cutscene != g_cutscene) {
            g_cutscene = cutscene;
            Log::Write("hud: a cutscene {}", cutscene ? "started; icons hidden" : "ended");
        }
        const Engine::Entity *player = Engine::PlayerEntity();
        const bool dialogue = started && player && Game::InDialogue(player);
        if (dialogue != g_dialogue) {
            g_dialogue = dialogue;
            Log::Write("hud: a dialogue {}", dialogue ? "started; icons hidden" : "ended");
        }
        // Icons follow the map legend's Track toggle for their type, except while map markers are off: the legend then
        // lists none of our types, so a type untracked earlier could not be tracked again.
        const auto *map = Game::GetMap();
        const Options::Values &options = Options::Get();
        const auto shown = [&](const MarkType::Type &type) {
            return options.floatingIcons && !cutscene && !dialogue &&
                   (!options.mapMarkers || (map && Game::IsTypeVisible(map, type.id)));
        };
        FloatingIcon::Update(MarkType::kMapDestination, g_destinations, shown(MarkType::kMapDestination));
        FloatingIcon::Update(MarkType::kMapLocation, g_sources, shown(MarkType::kMapLocation));
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

    // Finds what the plugin needs, reads the settings and offers them to MCM (once the game log exists, so their lines
    // reach it), and hooks the frame update, which installs the rest on the first frame.
    bool Install(const void *kcse) {
        if (!Engine::Install() || !Game::Install()) {
            Log::Write("not started: game code not found (an unsupported game version?)");
            return false;
        }
        Options::Install();
        Mcm::Install(kcse);
        // KCSE loads plugins after the framework exists, so a null one means the lookup is wrong for this game version.
        void *framework = Engine::Framework();
        if (!framework) {
            Log::Write("not started: the game framework was not found");
            return false;
        }
        g_frame = reinterpret_cast<FrameFn>(
            Hooks::ReplaceVirtual(framework, kPostUpdate, reinterpret_cast<void *>(&FrameUpdate)));
        return true;
    }
} // namespace

// KCSE loads DLLs from Mods/<mod>/KCSE/Plugins that export these two symbols: the plugin's description, then the
// load call, made on KCSE's start-up thread once WHGame.dll is loaded and the game framework exists, with KCSE's
// interface. The plugin uses only its messaging, to hear from MCM.
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
                                                              .name = "FindersKeepers",
                                                              .author = "pointfeev",
                                                              .compatibleGameVersions = {},
                                                              .kcseVersionRequired = 0,
                                                              .versionIndependence = 1};

__declspec(dllexport) bool KCSEPlugin_Load(const void *kcse) { return Install(kcse); }
}
