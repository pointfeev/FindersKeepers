#include "Probe.h"

#include <string>

#include "Engine.h"

namespace {
    constexpr const char kRunProbe[] = "TreasureFinder.Probe()";
    // TreasureFinder.Probe() publishes each result as a top-level global with this prefix.
    constexpr const char kPrefix[] = "TreasureFinderProbe_";

    using Engine::ScriptValue;

    bool Get(const char *field, ScriptValue &out) {
        const std::string name = std::string(kPrefix) + field;
        return Engine::GetGlobal(name.c_str(), out);
    }

    bool GetBool(const char *field) {
        ScriptValue v;
        return Get(field, v) && v.type == ScriptValue::Boolean && v.boolean;
    }

    bool GetNumber(const char *field, float &out) {
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
    float n = 0.f;
    if (GetNumber("mapCount", n))
        r.mapCount = static_cast<int>(n);
    r.foundById = GetBool("foundById");
    r.foundByName = GetBool("foundByName");
    r.sameEntity = GetBool("sameEntity");
    float x = 0.f, y = 0.f, z = 0.f;
    r.hasPos = GetNumber("x", x) && GetNumber("y", y) && GetNumber("z", z);
    if (r.hasPos)
        r.pos = Engine::Vec3(x, y, z);
    if (GetNumber("itemCount", n))
        r.itemCount = static_cast<int>(n);
    ScriptValue errors;
    if (Get("errors", errors) && errors.type == ScriptValue::String && errors.string)
        r.errors = errors.string;
    ScriptValue wuid;
    if (Get("wuid", wuid)) {
        r.wuidType = static_cast<int>(wuid.type);
        if (wuid.type == ScriptValue::Handle)
            r.wuid = static_cast<uint64_t>(wuid.handle);
    }
    return r;
}
