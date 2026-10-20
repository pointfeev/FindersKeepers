#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string_view>

// The CryEngine pieces the plugin uses, declared from our own reverse engineering of WHGame.dll 1.5.6.
namespace Engine {
    // Calls virtual function `slot` of a game object. CryEngine shuffles interface order per build, so every slot is
    // this build's, checked in Ghidra.
    template <typename R, typename... A> R VCall(const void *self, std::size_t slot, A... args) {
        using Fn = R(__fastcall *)(const void *, A...);
        return (*static_cast<Fn *const *>(self))[slot](self, args...);
    }

    struct Vec3 {
        float x = 0.f, y = 0.f, z = 0.f;

        Vec3() = default;
        Vec3(const float x, const float y, const float z) : x(x), y(y), z(z) {}
        Vec3 operator-(const Vec3 &o) const { return {x - o.x, y - o.y, z - o.z}; }
        bool operator==(const Vec3 &) const = default;
        float Length() const { return std::sqrt(x * x + y * y + z * z); }
    };

    // Finds gEnv and the game framework; false (logged) when a signature is not found.
    bool Install();

    // ILog::LogAlways; writes `text` as is.
    void LogAlways(const char *text);

    // Viewport size in pixels, and a world position projected to percent of the viewport (z > 1 is behind the
    // camera); false when off screen.
    int ScreenWidth();
    int ScreenHeight();
    bool ProjectToScreen(const Vec3 &world, float &x, float &y, float &z);

    // A Lua value as the script system exchanges it (ScriptAnyValue, 0x18 bytes; ToAny 0x18071698C): the type at +0,
    // the value at +8; numbers are floats.
    struct ScriptValue {
        enum Type : std::int32_t {
            Any,
            Nil,
            Boolean,
            Handle,
            Number,
            String,
            Table,
            Function,
            UserData,
            Vector,
        };
        Type type = Any;
        union {
            bool boolean;
            std::int64_t handle;
            float number;
            const char *string;
            std::uint8_t raw[0x10];
        };

        ScriptValue() : raw{} {}
    };
    static_assert(sizeof(ScriptValue) == 0x18, "ScriptAnyValue is 0x18 bytes");

    // A Lua global by name; false when the script system is not up or the global does not exist.
    bool GetGlobal(const char *name, ScriptValue &out);
    // Runs a chunk of Lua; false when it fails to load or raises an error.
    bool Execute(std::string_view code, const char *description);

    // The game framework (CCryAction); null before the engine creates it.
    void *Framework();
    // The player's world position; false before the player exists.
    bool PlayerPosition(Vec3 &out);
} // namespace Engine
