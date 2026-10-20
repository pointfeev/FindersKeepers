#include "Hooks.h"

#include <windows.h>

#include <algorithm>
#include <charconv>
#include <span>
#include <string_view>
#include <vector>

#include "Log.h"

namespace {
    // One page of executable memory within 2 GB of WHGame.dll, so a 5-byte jump from game code reaches it; it holds
    // every stub and relay (about 200 bytes today).
    constexpr std::size_t kPatchMemory = 4096;
    constexpr std::size_t kRelaySize = Hooks::kAbsoluteJumpSize;

    std::uint8_t *g_memory = nullptr;
    std::size_t g_used = 0;

    bool CreatePatchMemory() {
        const auto base = reinterpret_cast<std::uintptr_t>(GetModuleHandleA("WHGame.dll"));
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        const std::uintptr_t step = info.dwAllocationGranularity;
        constexpr std::uintptr_t reach = std::uintptr_t{0x7FFF0000};
        const std::uintptr_t lowest = base > reach ? base - reach : step;
        for (std::uintptr_t address = (base - step) & ~(step - 1); address >= lowest; address -= step) {
            void *memory = VirtualAlloc(reinterpret_cast<void *>(address), kPatchMemory, MEM_COMMIT | MEM_RESERVE,
                                        PAGE_EXECUTE_READWRITE);
            if (memory) {
                g_memory = static_cast<std::uint8_t *>(memory);
                return true;
            }
        }
        return false;
    }

    // `size` bytes of patch memory, keeping room for the relay of the branch that will lead to them.
    std::uint8_t *Reserve(const char *what, const std::size_t size) {
        if (!g_memory && !CreatePatchMemory()) {
            Log::Write("hooks: could not allocate patch memory near WHGame.dll; {} not patched", what);
            return nullptr;
        }
        if (kPatchMemory - g_used < size + kRelaySize) {
            Log::Write("hooks: patch memory is full; {} not patched", what);
            return nullptr;
        }
        std::uint8_t *result = g_memory + g_used;
        g_used += size;
        return result;
    }

    constexpr int kAnyByte = -1;

    std::vector<int> ParsePattern(const std::string_view text) {
        std::vector<int> bytes;
        for (std::size_t i = 0; i < text.size();) {
            if (text[i] == ' ') {
                ++i;
                continue;
            }
            const std::string_view token = text.substr(i, 2);
            int byte = kAnyByte;
            if (token != "??")
                std::from_chars(token.data(), token.data() + token.size(), byte, 16);
            bytes.push_back(byte);
            i += 2;
        }
        return bytes;
    }

    std::vector<std::uintptr_t> Scan(std::uint8_t *module, const std::vector<int> &pattern) {
        std::vector<std::uintptr_t> matches;
        const auto *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(module);
        const auto *nt = reinterpret_cast<const IMAGE_NT_HEADERS64 *>(module + dos->e_lfanew);
        const auto *section = IMAGE_FIRST_SECTION(nt);
        for (WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s, ++section) {
            if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE))
                continue;
            std::span<const std::uint8_t> rest(module + section->VirtualAddress, section->Misc.VirtualSize);
            const auto matchesByte = [](const std::uint8_t byte, const int expected) {
                return expected == kAnyByte || byte == expected;
            };
            for (auto hit = std::ranges::search(rest, pattern, matchesByte); !hit.empty();
                 hit = std::ranges::search(rest, pattern, matchesByte)) {
                matches.push_back(reinterpret_cast<std::uintptr_t>(hit.data()));
                rest = rest.subspan(hit.data() - rest.data() + 1);
            }
        }
        return matches;
    }

    void Write(const std::uintptr_t address, std::span<const std::uint8_t> bytes) {
        auto *target = reinterpret_cast<std::uint8_t *>(address);
        DWORD old = 0;
        VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &old);
        std::ranges::copy(bytes, target);
        VirtualProtect(target, bytes.size(), old, &old);
        FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
    }
} // namespace

std::uintptr_t Hooks::Find(const char *what, const std::span<const Rung> rungs) {
    auto *module = reinterpret_cast<std::uint8_t *>(GetModuleHandleA("WHGame.dll"));
    if (!module) {
        Log::Write("hooks: WHGame.dll is not loaded; {} not found", what);
        return 0;
    }
    for (std::size_t r = 0; r < rungs.size(); ++r) {
        const Rung &rung = rungs[r];
        const auto matches = Scan(module, ParsePattern(rung.pattern));
        if (matches.size() != 1) {
            Log::Write("hooks: {} rung {} matched {} times, expected 1", what, r + 1, matches.size());
            continue;
        }
        const std::uintptr_t at = matches[0] + rung.offset;
        if (!rung.ripLength)
            return at;
        const auto disp = *reinterpret_cast<const std::int32_t *>(at + rung.ripDisp);
        return at + rung.ripLength + disp + rung.adjust;
    }
    return 0;
}

bool Hooks::StartsWith(const std::uintptr_t function, std::span<const std::uint8_t> bytes) {
    return std::ranges::equal(bytes, std::span(reinterpret_cast<const std::uint8_t *>(function), bytes.size()));
}

std::uintptr_t Hooks::Detour(const char *what, const std::uintptr_t function, const std::span<const std::uint8_t> entry,
                             const void *hook) {
    if (!StartsWith(function, entry)) {
        Log::Write("hooks: {} does not start as expected; not patched", what);
        return 0;
    }
    auto *original = Reserve(what, entry.size() + kAbsoluteJumpSize);
    if (!original)
        return 0;
    StubWriter(original).Bytes(entry).AbsoluteJump(function + entry.size());
    Branch(function, reinterpret_cast<std::uintptr_t>(hook));
    return reinterpret_cast<std::uintptr_t>(original);
}

void *Hooks::ReplaceVirtual(void *object, const std::size_t slot, void *function) {
    void **entry = *static_cast<void ***>(object) + slot;
    DWORD old = 0;
    VirtualProtect(entry, sizeof *entry, PAGE_READWRITE, &old);
    void *previous = InterlockedExchangePointer(entry, function);
    VirtualProtect(entry, sizeof *entry, old, &old);
    return previous;
}

std::uint8_t *Hooks::AllocateStub(const char *what, const std::size_t size) { return Reserve(what, size); }

void Hooks::Branch(const std::uintptr_t function, const std::uintptr_t target) {
    // Reserve always leaves room for this relay.
    std::uint8_t *relay = g_memory + g_used;
    g_used += kRelaySize;
    StubWriter(relay).AbsoluteJump(target);
    // jmp rel32 to the relay.
    std::array<std::uint8_t, 5> jump{};
    const auto rel = static_cast<std::int32_t>(reinterpret_cast<std::intptr_t>(relay) -
                                               static_cast<std::intptr_t>(function + jump.size()));
    StubWriter(jump.data()).Bytes({0xE9}).Value(rel);
    Write(function, jump);
}
