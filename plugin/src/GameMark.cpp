#include "GameMark.h"

#include <memory>

#include "Game.h"
#include "Log.h"
#include "MarkType.h"

namespace {
    // The param the game's own "show map marker" quest node passes.
    constexpr uint32_t kMarkParam = 2;

    // Registered for the rest of the session: the mark cannot be removed yet (RemoveEntityMark is not wrapped), so it
    // is registered once, not again after a save loads.
    std::shared_ptr<Game::EntityMark> g_mark;
    bool g_loggedNoObject = false;
} // namespace

void GameMark::Update(uint64_t wuid) {
    if (g_mark)
        return;
    auto *map = Game::GetMap();
    // Registered during loading, the mark's compass marker is lost when loading resets the compass.
    if (!map || wuid == 0 || !Game::GameplayStarted())
        return;
    void *object = Game::FindLinkable(wuid);
    if (!object) {
        if (!g_loggedNoObject)
            Log::Write("mark: no linkable object for WUID {:016x}", wuid);
        g_loggedNoObject = true;
        return;
    }
    g_mark = Game::AddEntityMark(map, MarkType::kId, object, kMarkParam);
    if (!g_mark) {
        Log::Write("mark: GetOrCreateEntityMark returned no mark");
        return;
    }
    Log::Write("mark: registered mark id {} (type {}) for WUID {:016x}", g_mark->id, MarkType::kId, wuid);
}
