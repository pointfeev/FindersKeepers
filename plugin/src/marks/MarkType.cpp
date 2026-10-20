#include "marks/MarkType.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "Log.h"
#include "game/Game.h"
#include "game/Hooks.h"

namespace {
    // Signatures, each rung checked for exactly one match with pat.py against WHGame.dll 1.5.6, most specific first.
    //
    // The type naming function (RVA 0xC4D664): `inc ecx; cmp ecx, 0x61; ja <"?">; movsxd rax, ecx; lea rdx, [base];
    // mov ecx, [rdx + rax*4 + table]; add rcx, rdx; jmp rcx`. The type count is wildcarded: it grows when the game adds
    // types.
    constexpr Hooks::Rung kNameRungs[] = {
        {.pattern =
             "FF C1 83 F9 ?? 0F 87 ?? ?? ?? ?? 48 63 C1 48 8D 15 ?? ?? ?? ?? 8B 8C 82 ?? ?? ?? ?? 48 03 CA FF E1"},
        {.pattern = "FF C1 83 F9 ?? ?? ?? ?? ?? ?? ?? 48 63 C1 48 8D 15"},
    };
    // The type name -> filter id mapper (RVA 0xDCA50C), a _stricmp chain that starts with the names mapped to -1
    // ("Invalid", "Pillory", "Church", "Unknown") and then "Checkpoint" (0) and "Main" (1).
    constexpr Hooks::Rung kMapperRungs[] = {
        {.pattern = "40 53 48 83 EC 20 48 8D 15 ?? ?? ?? ?? 48 8B D9 FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? "
                    "48 8D 15 ?? ?? ?? ?? 48 8B CB FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? "
                    "48 8D 15 ?? ?? ?? ?? 48 8B CB FF 15 ?? ?? ?? ?? 85 C0 0F 84 ?? ?? ?? ?? 48 8D 15"},
        {.pattern = "85 C0 0F 84 ?? ?? ?? ?? 48 8D 15 ?? ?? ?? ?? 48 8B CB FF 15 ?? ?? ?? ?? "
                    "85 C0 75 ?? B8 01 00 00 00 E9 ?? ?? ?? ?? 48 8D 15",
         .offset = -0x76},
    };
    // The map legend's top-group check (RVA 0x19DEF10), a compare chain on the type id.
    constexpr Hooks::Rung kTopGroupRungs[] = {
        {.pattern = "83 F9 5E 7F ?? 74 ?? 83 F9 05 7F ?? 74 ?? 85 C9 EB ?? 83 E9 06"},
        {.pattern = "83 F9 5E ?? ?? ?? ?? 83 F9 05 ?? ?? ?? ?? 85 C9 ?? ?? 83 E9 06 ?? ?? 83 E9 01"},
    };
    // The map POI marker's constructor from an entity mark (RVA 0x15B9D70): it names the type, copies the mark's
    // param and takes the marker's priority from the type's poi_type row (ui_order), or the type id without one.
    constexpr Hooks::Rung kMarkerRungs[] = {
        {.pattern = "41 8B 4E 04 E8 ?? ?? ?? ?? 48 8B D0 48 8D 4F 18 E8 ?? ?? ?? ?? 41 8B 46 08 89 47 20 C6 47 24 00",
         .offset = -0x129},
        {.pattern = "FF 90 ?? ?? ?? ?? 48 85 C0 74 ?? 8B 40 30 EB ?? 41 8B 46 04 89 47 28", .offset = -0x163},
    };

    using NameFn = const char *(__fastcall *)(uint32_t);
    using MapperFn = int(__fastcall *)(const char *);
    using TopGroupFn = bool(__fastcall *)(int);
    using MarkerFn = void *(__fastcall *)(void *, const Game::EntityMark *);

    // Naming: the original's `inc ecx; cmp ecx, imm8`, whose immediate (the type count) is read rather than assumed.
    constexpr std::uint8_t kNamePrefix[] = {0xFF, 0xC1, 0x83, 0xF9};
    constexpr std::size_t kNameEntrySize = 5;
    NameFn g_name;

    const char *__fastcall Name(const uint32_t id) {
        const MarkType::Type *type = MarkType::Find(id);
        return type ? type->name : g_name(id);
    }

    bool PatchName() {
        const auto function = Hooks::Find("naming function", kNameRungs);
        if (!function)
            return false;
        if (!Hooks::StartsWith(function, kNamePrefix)) {
            Log::Write("mark type: naming function does not start as expected; not patched");
            return false;
        }
        for (const MarkType::Type &type : MarkType::kTypes) {
            if (const char *current = reinterpret_cast<NameFn>(function)(type.id); std::string_view(current) != "?") {
                Log::Write("mark type: id {} is already named '{}' by the game; not patched", type.id, current);
                return false;
            }
        }
        std::array<std::uint8_t, kNameEntrySize> entry{};
        std::ranges::copy_n(reinterpret_cast<const std::uint8_t *>(function), entry.size(), entry.begin());
        g_name = reinterpret_cast<NameFn>(Hooks::Detour("naming function", function, entry, &Name));
        if (!g_name)
            return false;
        for (const MarkType::Type &type : MarkType::kTypes)
            Log::Write("mark type: id {} is now '{}'", type.id, reinterpret_cast<NameFn>(function)(type.id));
        return true;
    }

    // Mapper: the original's `push rbx; sub rsp, 0x20`.
    constexpr std::uint8_t kMapperEntry[] = {0x40, 0x53, 0x48, 0x83, 0xEC, 0x20};
    MapperFn g_mapper;

    int __fastcall MapName(const char *name) {
        for (const MarkType::Type &type : MarkType::kTypes)
            if (_stricmp(name, type.name) == 0)
                return static_cast<int>(type.id);
        return g_mapper(name);
    }

    void PatchMapper() {
        const auto function = Hooks::Find("name mapper", kMapperRungs);
        if (!function)
            return;
        // Its first string is "Invalid" (lea rdx, [rip + disp32] at +6), a check on top of the pattern.
        if (const auto disp = *reinterpret_cast<const std::int32_t *>(function + 9);
            std::string_view(reinterpret_cast<const char *>(function + 13 + disp)) != "Invalid") {
            Log::Write("mark type: name mapper's first name is not 'Invalid'; not patched");
            return;
        }
        for (const MarkType::Type &type : MarkType::kTypes) {
            if (const int before = reinterpret_cast<MapperFn>(function)(type.name); before != -1) {
                Log::Write("mark type: name mapper already maps '{}' to {}; not patched", type.name, before);
                return;
            }
        }
        g_mapper = reinterpret_cast<MapperFn>(Hooks::Detour("name mapper", function, kMapperEntry, &MapName));
        if (!g_mapper)
            return;
        for (const MarkType::Type &type : MarkType::kTypes)
            Log::Write("mark type: name mapper maps '{}' to {}", type.name,
                       reinterpret_cast<MapperFn>(function)(type.name));
    }

    // Top group: the original's `cmp ecx, 0x5E; jg rel8`, rebuilt with the jump as an absolute one.
    constexpr std::uint8_t kTopGroupEntry[] = {0x83, 0xF9, 0x5E, 0x7F};
    constexpr std::size_t kTopGroupEntrySize = 5;
    constexpr std::size_t kTopGroupStubSize = 3 + 2 + Hooks::kAbsoluteJumpSize + Hooks::kAbsoluteJumpSize;
    TopGroupFn g_topGroup;

    bool __fastcall IsTopGroup(const int type) {
        return MarkType::Find(static_cast<uint32_t>(type)) || g_topGroup(type);
    }

    void PatchTopGroup() {
        const auto function = Hooks::Find("legend top-group check", kTopGroupRungs);
        if (!function)
            return;
        if (!Hooks::StartsWith(function, kTopGroupEntry)) {
            Log::Write("mark type: legend top-group check does not start as expected; not patched");
            return;
        }
        auto *stub = Hooks::AllocateStub("legend top-group check", kTopGroupStubSize);
        if (!stub)
            return;
        const auto rel8 = *reinterpret_cast<const std::int8_t *>(function + 4);
        Hooks::StubWriter(stub)
            .Bytes({0x83, 0xF9, 0x5E})                          // cmp ecx, 0x5E
            .Bytes({0x7E, Hooks::kAbsoluteJumpSize})            // jle over the next jump
            .AbsoluteJump(function + kTopGroupEntrySize + rel8) // the original jg target
            .AbsoluteJump(function + kTopGroupEntrySize);
        g_topGroup = reinterpret_cast<TopGroupFn>(stub);
        Hooks::Branch(function, reinterpret_cast<std::uintptr_t>(&IsTopGroup));
        for (const MarkType::Type &type : MarkType::kTypes)
            Log::Write("mark type: legend lists '{}' in the top group: {}", type.name,
                       reinterpret_cast<TopGroupFn>(function)(static_cast<int>(type.id)));
    }

    // Marker: the original's `mov [rsp+0x18], rbx`.
    constexpr std::uint8_t kMarkerEntry[] = {0x48, 0x89, 0x5C, 0x24, 0x18};
    // C_UIFlashMapPoiMarker's priority.
    constexpr std::size_t kMarkerPriorityOffset = 0x28;
    MarkerFn g_marker;

    // The map stacks POI markers by this priority (PoiMarker.GetDepth in ApseMap.gfx: MEnum.MAX_DEPTH - priority *
    // 1000), lowest on top; 0 puts ours above every other POI.
    void *__fastcall ConstructMarker(void *marker, const Game::EntityMark *mark) {
        void *result = g_marker(marker, mark);
        if (MarkType::Find(mark->type))
            *reinterpret_cast<std::int32_t *>(static_cast<std::uint8_t *>(marker) + kMarkerPriorityOffset) = 0;
        return result;
    }

    void PatchMarker() {
        const auto function = Hooks::Find("POI marker constructor", kMarkerRungs);
        if (!function)
            return;
        g_marker = reinterpret_cast<MarkerFn>(
            Hooks::Detour("POI marker constructor", function, kMarkerEntry, &ConstructMarker));
        if (g_marker)
            Log::Write("mark type: our map markers stack above other POIs");
    }
} // namespace

bool MarkType::Install() {
    if (!PatchName())
        return false;
    // Without these the type still shows on the map and compass, but the legend lists it under POIs, its Track
    // toggle (and the saved filter state) does nothing and other POIs can cover its marker.
    PatchMapper();
    PatchTopGroup();
    PatchMarker();
    return true;
}
