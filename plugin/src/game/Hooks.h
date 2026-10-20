#pragma once

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <span>

// Finding game code and data by signature and redirecting functions, for everything the plugin touches in
// WHGame.dll.
namespace Hooks {
    // A byte pattern in pat.py syntax (hex bytes, ?? for any byte). The result is the match plus `offset`; with
    // `ripLength` set, it is instead the target of the RIP-relative operand there (displacement at `ripDisp`, the
    // instruction `ripLength` bytes long), plus `adjust`. Every rung is checked for exactly one match with pat.py
    // before it ships. Written with designated initializers: {.pattern = "...", .ripDisp = 3, .ripLength = 7}.
    struct Rung {
        const char *pattern;
        std::ptrdiff_t offset = 0;
        std::ptrdiff_t ripDisp = 0;
        std::ptrdiff_t ripLength = 0;
        std::ptrdiff_t adjust = 0;
    };

    // The result of the first rung that matches exactly once in WHGame.dll's executable sections, or 0 (logged
    // under `what`).
    std::uintptr_t Find(const char *what, std::span<const Rung> rungs);

    // Whether the code at `function` starts with `bytes`.
    bool StartsWith(std::uintptr_t function, std::span<const std::uint8_t> bytes);

    // Redirects `function` to `hook` and returns the original as a callable copy: its first `entrySize` bytes,
    // which must equal `entry` and be whole position-independent instructions, then a jump back into its body.
    // Returns 0 (logged, nothing changed) when the function starts differently or patch memory is short.
    std::uintptr_t Detour(const char *what, std::uintptr_t function, std::span<const std::uint8_t> entry,
                          const void *hook);

    // Swaps entry `slot` of `object`'s vtable for `function` and returns the entry it held. The swap is one atomic
    // pointer write, so a thread calling through the table sees either function.
    void *ReplaceVirtual(void *object, std::size_t slot, void *function);

    // Executable memory within a 5-byte jump of the game for a hand-written stub, or null (logged) when short. The
    // room for the relay of the branch that will lead to it is reserved too.
    std::uint8_t *AllocateStub(const char *what, std::size_t size);
    // Overwrites the first 5 bytes of `function` with a jump to `target`, through a 14-byte relay in patch memory.
    void Branch(std::uintptr_t function, std::uintptr_t target);

    // jmp [rip+0] with the 8-byte target after it.
    constexpr std::size_t kAbsoluteJumpSize = 14;

    // Writes machine code into patch memory, piece by piece.
    class StubWriter {
      public:
        explicit StubWriter(std::uint8_t *at) : at_(at) {}

        StubWriter &Bytes(std::span<const std::uint8_t> bytes) {
            at_ = std::ranges::copy(bytes, at_).out;
            return *this;
        }
        StubWriter &Bytes(const std::initializer_list<std::uint8_t> bytes) {
            return Bytes(std::span(bytes.begin(), bytes.size()));
        }
        // A value's bytes, little-endian as the CPU reads them.
        template <typename T> StubWriter &Value(T value) {
            return Bytes(std::bit_cast<std::array<std::uint8_t, sizeof(T)>>(value));
        }
        StubWriter &AbsoluteJump(const std::uintptr_t target) { return Bytes({0xFF, 0x25, 0, 0, 0, 0}).Value(target); }

      private:
        std::uint8_t *at_;
    };
} // namespace Hooks
