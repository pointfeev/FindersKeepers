#include "Probe.h"

#include <format>
#include <string>

#include "Engine.h"

namespace {
    constexpr const char kRunProbe[] = "TreasureFinder.Probe()";
    // TreasureFinder.Probe() publishes each result as a top-level global with this prefix.
    constexpr const char kPrefix[] = "TreasureFinderProbe_";

    using Engine::ScriptValue;

    // Per target n, the fields are TreasureFinderProbe_<field>_<n>.
    std::string Field(std::string_view field, int n) { return std::format("{}_{}", field, n); }

    bool Get(std::string_view field, ScriptValue &out) {
        return Engine::GetGlobal(std::format("{}{}", kPrefix, field).c_str(), out);
    }

    bool GetBool(std::string_view field) {
        ScriptValue v;
        return Get(field, v) && v.type == ScriptValue::Boolean && v.boolean;
    }

    bool GetNumber(std::string_view field, float &out) {
        ScriptValue v;
        if (!Get(field, v) || v.type != ScriptValue::Number)
            return false;
        out = v.number;
        return true;
    }
} // namespace

const char *Probe::StatusName(ProbeResult::Status status) {
    switch (status) {
    case ProbeResult::Status::ExecuteFailed:
        return "Lua call failed";
    case ProbeResult::Status::NoGlobal:
        return "mod Lua script not loaded (TreasureFinderLoaded unset)";
    case ProbeResult::Status::NotRun:
        return "TreasureFinderProbe_ran not set after the call";
    case ProbeResult::Status::Ran:
        return "ran";
    }
    return "?";
}

ProbeResult Probe::Run() {
    ProbeResult r;
    ScriptValue loaded;
    if (!Engine::GetGlobal("TreasureFinderLoaded", loaded) || loaded.type != ScriptValue::Boolean || !loaded.boolean)
        return r;
    if (!Engine::Execute(kRunProbe, "TreasureFinder probe")) {
        r.status = ProbeResult::Status::ExecuteFailed;
        return r;
    }
    if (!GetBool("ran")) {
        r.status = ProbeResult::Status::NotRun;
        return r;
    }
    r.status = ProbeResult::Status::Ran;
    r.playerReady = GetBool("playerReady");
    float count = 0.f;
    GetNumber("count", count);
    for (int n = 1; n <= static_cast<int>(count); ++n) {
        ScriptValue map, wuid;
        float x = 0.f, y = 0.f, z = 0.f;
        if (!Get(Field("map", n), map) || map.type != ScriptValue::String || !map.string ||
            !Get(Field("wuid", n), wuid) || wuid.type != ScriptValue::Handle || !GetNumber(Field("x", n), x) ||
            !GetNumber(Field("y", n), y) || !GetNumber(Field("z", n), z)) {
            r.errors += std::format("{}target {} not readable", r.errors.empty() ? "" : "; ", n);
            continue;
        }
        r.targets.push_back({map.string, static_cast<std::uint64_t>(wuid.handle), Engine::Vec3(x, y, z)});
    }
    ScriptValue errors;
    if (Get("errors", errors) && errors.type == ScriptValue::String && errors.string)
        r.errors += std::format("{}{}", r.errors.empty() ? "" : "; ", errors.string);
    return r;
}
