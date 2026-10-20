#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "Engine.h"

// Results of TreasureFinder.Probe() in Scripts/Mods/treasurefinder.lua, read back from its Lua globals.
struct ProbeResult {
    enum class Status {
        NoGlobal,      // TreasureFinderLoaded is unset: the mod's Lua init script did not run
        ExecuteFailed, // the call into Lua failed to load or raised an error
        NotRun,        // the call returned but TreasureFinderProbe_ran is not true
        Ran,
    };
    // Where a carried map leads, in the loaded level and not dealt with yet.
    struct Target {
        std::string map;    // the map item's name
        std::uint64_t wuid; // the destination's WUID (XGenAIModule.GetMyWUID)
        Engine::Vec3 pos;

        bool operator==(const Target &) const = default;
    };

    Status status = Status::NoGlobal;
    bool playerReady = false;
    std::vector<Target> targets;
    std::string errors; // maps whose check raised a Lua error, "map: message; ..."

    bool operator==(const ProbeResult &) const = default;
};

namespace Probe {
    ProbeResult Run();
    const char *StatusName(ProbeResult::Status status);
} // namespace Probe
