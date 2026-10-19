#pragma once

#include "Engine.h"

// The one map the test plugin tracks: the Vidlak bandit's map (poi_vyplavenyPoklad_map), whose washed-up chest is
// trosecko Stash 15785. The Lua probe looks the chest up live; this position (from the level data) is the fallback
// while it has not reported one.
namespace TestTarget {
    inline const Engine::Vec3 kChestPos(1545.787f, 3002.118f, 15.915f);
}
