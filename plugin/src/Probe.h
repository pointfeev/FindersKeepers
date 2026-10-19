#pragma once

#include <cstdint>
#include <string>

#include "Engine.h"

// Results of TreasureFinder.Probe() in Scripts/Mods/treasurefinder.lua, read back from its Lua globals.
struct ProbeResult {
    enum class Status {
        NoGlobal,      // TreasureFinderLoaded is unset: the mod's Lua init script did not run
        ExecuteFailed, // the call into Lua failed to load or raised an error
        NotRun,        // the call returned but TreasureFinderProbe_ran is not true
        Ran,
    };

    Status status = Status::NoGlobal;
    bool playerReady = false;
    int mapCount = -1;
    bool foundById = false;
    bool foundByName = false;
    bool sameEntity = false;
    bool hasPos = false;
    Engine::Vec3 pos;
    int itemCount = -1;
    uint64_t wuid = 0;  // the chest's WUID (XGenAIModule.GetMyWUID), 0 when unknown
    int wuidType = -1;  // Lua value type the WUID arrived as; only a handle is read
    std::string errors; // game calls that raised a Lua error, "call: message; ..."

    bool operator==(const ProbeResult &) const = default;
};

namespace Probe {
    ProbeResult Run();
    const char *StatusName(ProbeResult::Status status);
} // namespace Probe
