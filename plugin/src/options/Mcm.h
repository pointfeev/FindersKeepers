#pragma once

// The settings in Mod Configuration Menu (MCM, Nexus 3363), when it is installed: an in-game page that edits the same
// values as TreasureFinder.ini. MCM keeps nothing itself; each change goes to Options, which rewrites the file.
namespace Mcm {
    // Listens for MCM's messages through KCSE's messaging interface; `kcse` is the interface KCSEPlugin_Load receives.
    // Without MCM the messages never arrive and the INI alone holds the settings.
    void Install(const void *kcse);
} // namespace Mcm
