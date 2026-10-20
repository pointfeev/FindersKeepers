#include "MapImportance.h"

#include <string_view>

#include "Flash.h"
#include "Game.h"
#include "Hooks.h"
#include "Log.h"
#include "MapTooltip.h"
#include "MarkType.h"

namespace {
    constexpr char kMEnum[] = "_global.MEnum";
    // The game's own GetPoiImportance, kept on MEnum under this name; its presence also marks the movie as patched.
    constexpr char kOriginal[] = "treasureFinderGetPoiImportance";
    constexpr char kOriginalPath[] = "_global.MEnum.treasureFinderGetPoiImportance";
    // MEnum.POI_IMPORTANCE_HIGH in ApseMap.gfx.
    constexpr int kImportanceHigh = 2;

    class GetPoiImportance final : public Flash::Function {
      public:
        void Call(const Params &params, Result *result) override {
            const auto args = params.Args();
            if (!args.empty() && args[0]) {
                if (const Flash::Value name = args[0]->ToValue();
                    name.type == Flash::Value::String && std::string_view(name.string) == MarkType::kName) {
                    result->Set(Flash::Value(kImportanceHigh));
                    return;
                }
            }
            Flash::Value importance;
            if (Flash::VarPtr menu; params.player && params.player->GetVariable(kMEnum, menu.put()) && menu &&
                                    menu->Invoke(kOriginal, args, &importance))
                result->Set(importance);
        }
    };

    GetPoiImportance g_function;

    // Replaces MEnum.GetPoiImportance in the loaded map movie unless it is already ours.
    void Patch(const Game::Map *map) {
        Flash::Player *player = Game::MapPlayer(map);
        if (!player || !player->IsAvailable(kMEnum)) {
            Log::Write("map importance: the map movie is not loaded yet; '{}' markers keep normal importance",
                       MarkType::kName);
            return;
        }
        if (player->IsAvailable(kOriginalPath))
            return;
        Flash::VarPtr menu, original, replacement;
        if (!player->GetVariable(kMEnum, menu.put()) || !menu || !menu->GetMember("GetPoiImportance", original.put()) ||
            !original) {
            Log::Write("map importance: MEnum.GetPoiImportance not found");
            return;
        }
        if (!player->CreateFunction(replacement.put(), &g_function) || !replacement ||
            !menu->SetMember(kOriginal, original.get()) || !menu->SetMember("GetPoiImportance", replacement.get())) {
            Log::Write("map importance: could not replace MEnum.GetPoiImportance");
            return;
        }
        Log::Write("map importance: '{}' markers are high importance", MarkType::kName);
    }

    // The map screen's step that sends the POI markers and legend items to the map movie (RVA 0xA1AE28), run each
    // time the map opens. The map movie is loaded by then: earlier steps of the same refresh already talk to it.
    constexpr Hooks::Rung kSendRungs[] = {
        {.pattern = "E8 ?? ?? ?? ?? 34 01 48 8D 4C 24 ?? 02 C0 41 8B D6 88 45 ?? E8 ?? ?? ?? ?? "
                    "48 8D 4D ?? 4C 89 64 24 ?? 48 8D 96 ?? ?? ?? ??",
         .offset = -0x1B5},
        {.pattern = "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 ?? 48 81 EC ?? ?? ?? ?? 8B 15 ?? ?? ?? ?? 0F 57 C0 "
                    "65 48 8B 04 25 58 00 00 00 48 8B F1 B9 18 00 00 00"},
    };
    // The original's `push rbp; push rbx; push rsi; push rdi`.
    constexpr std::uint8_t kSendEntry[] = {0x40, 0x55, 0x53, 0x56, 0x57};
    using SendFn = void(__fastcall *)(Game::Map *);
    SendFn g_send;

    // Patches before the markers are sent, so they are created with our importance and tooltip.
    void __fastcall SendMapContent(Game::Map *map) {
        Patch(map);
        MapTooltip::Patch(map);
        g_send(map);
    }
} // namespace

void MapImportance::Install() {
    if (const auto function = Hooks::Find("map content sender", kSendRungs))
        g_send = reinterpret_cast<SendFn>(Hooks::Detour("map content sender", function, kSendEntry, &SendMapContent));
}
