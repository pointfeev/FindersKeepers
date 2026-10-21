#include "marks/MapTooltip.h"

#include <charconv>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "Log.h"
#include "game/Flash.h"
#include "marks/GameMark.h"
#include "marks/MarkType.h"

namespace {
    constexpr char kRoot[] = "_root";
    constexpr char kShowTooltip[] = "OnShowTooltip";
    // The game's own OnShowTooltip, kept on _root under this name; its presence also marks the movie as patched.
    constexpr char kOriginal[] = "findersKeepersOnShowTooltip";
    constexpr char kOriginalPath[] = "_root.findersKeepersOnShowTooltip";

    std::optional<Flash::Value> Member(const Flash::Var *object, const char *name) {
        if (Flash::VarPtr member; object->GetMember(name, member.put()) && member)
            return member->ToValue();
        return std::nullopt;
    }

    // A PoiMarker's Id: the entity mark's id, which C_UIFlashMapPoiMarker sends as a string.
    std::optional<std::int32_t> MarkerId(const Flash::Var *marker) {
        const auto id = Member(marker, "Id");
        if (!id)
            return std::nullopt;
        switch (id->type) {
        case Flash::Value::String: {
            const std::string_view text(id->string);
            std::int32_t value = 0;
            if (const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
                error == std::errc() && end == text.data() + text.size())
                return value;
            return std::nullopt;
        }
        case Flash::Value::Int:
            return id->i;
        case Flash::Value::Double:
            return static_cast<std::int32_t>(id->d);
        default:
            return std::nullopt;
        }
    }

    // The mark's tooltip label when `marker` is one of our PoiMarkers; null otherwise.
    const char *OurLabel(const Flash::Var *marker) {
        if (const auto icon = Member(marker, "m_IconName");
            !icon || icon->type != Flash::Value::String || !MarkType::Find(std::string_view(icon->string)))
            return nullptr;
        const auto id = MarkerId(marker);
        return id ? GameMark::LabelFor(*id) : nullptr;
    }

    class ShowTooltip final : public Flash::Function {
      public:
        void Call(const Params &params, Result *) override {
            Flash::VarPtr root;
            if (!params.player || !params.player->GetVariable(kRoot, root.put()) || !root)
                return;
            const auto args = params.Args();
            const char *label = args.size() >= 2 && args[1] ? OurLabel(args[1]) : nullptr;
            if (!label) {
                root->Invoke(kOriginal, args);
                return;
            }
            // The marker is the game's object, which we change; the const is only how arguments arrive.
            auto *marker = const_cast<Flash::Var *>(args[1]);
            Flash::VarPtr uiName;
            if (!marker->SetMember("m_UiName", Flash::Value(label)) || !marker->GetMember("m_UiName", uiName.put()) ||
                !uiName) {
                root->Invoke(kOriginal, args);
                return;
            }
            std::vector relabelled(args.begin(), args.end());
            relabelled[0] = uiName.get();
            root->Invoke(kOriginal, std::span<const Flash::Var *const>(relabelled));
        }
    };

    ShowTooltip g_function;
} // namespace

void MapTooltip::Patch(const Game::Map *map) {
    Flash::Player *player = Game::MapPlayer(map);
    if (!player || player->IsAvailable(kOriginalPath))
        return;
    Flash::VarPtr root, original, replacement;
    if (!player->GetVariable(kRoot, root.put()) || !root || !root->GetMember(kShowTooltip, original.put()) ||
        !original) {
        Log::Write("map tooltip: _root.OnShowTooltip not found; markers keep the type name");
        return;
    }
    if (!player->CreateFunction(replacement.put(), &g_function) || !replacement ||
        !root->SetMember(kOriginal, original.get()) || !root->SetMember(kShowTooltip, replacement.get())) {
        Log::Write("map tooltip: could not replace _root.OnShowTooltip");
        return;
    }
    Log::Write("map tooltip: our markers name their map");
}
