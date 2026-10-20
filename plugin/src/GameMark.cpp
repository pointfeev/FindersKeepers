#include "GameMark.h"

#include <algorithm>
#include <memory>
#include <vector>

#include "Game.h"
#include "Log.h"
#include "MarkType.h"

namespace {
    // The param the game's own "show map marker" quest node passes.
    constexpr std::uint32_t kMarkParam = 2;

    // One mark, watching its object the way the quest node does, so the mark goes before the object it points at.
    class Mark final : public Game::ObjectListener {
      public:
        explicit Mark(std::uint64_t wuid) : wuid_(wuid) {}

        std::uint64_t Wuid() const { return wuid_; }
        bool Live() const { return mark_ != nullptr; }

        bool Add(Game::Map *map, void *object) {
            mark_ = Game::AddEntityMark(map, MarkType::kId, object, kMarkParam);
            if (!mark_)
                return false;
            Game::WatchObject(&wuid_, this);
            return true;
        }

        void Remove() {
            if (!mark_)
                return;
            Game::UnwatchObject(&wuid_, this);
            if (auto *map = Game::GetMap())
                Game::RemoveEntityMark(map, mark_);
            else
                Log::Write("mark: no map screen to remove the mark for WUID {:016x} from", wuid_);
            mark_.reset();
        }

        // The game calls this while it destroys the object; the mark goes now and the entry on the next Sync.
        void OnObjectEvent(int event) override {
            if (event != Game::kObjectRemoved)
                return;
            Log::Write("mark: object {:016x} destroyed; mark removed", wuid_);
            Remove();
        }

      private:
        std::uint64_t wuid_;
        std::shared_ptr<Game::EntityMark> mark_;
    };

    // The game holds pointers to these (as listeners and to their WUIDs), so they do not move.
    std::vector<std::unique_ptr<Mark>> g_marks;
    // WUIDs already logged as having no object, so a target out of reach is logged once.
    std::vector<std::uint64_t> g_loggedMissing;
} // namespace

void GameMark::Sync(std::span<const std::uint64_t> wuids) {
    std::erase_if(g_marks, [&](const std::unique_ptr<Mark> &mark) {
        if (std::ranges::contains(wuids, mark->Wuid()) && mark->Live())
            return false;
        mark->Remove();
        return true;
    });
    auto *map = Game::GetMap();
    if (!map)
        return;
    for (const std::uint64_t wuid : wuids) {
        if (std::ranges::contains(g_marks, wuid, &Mark::Wuid))
            continue;
        void *object = Game::FindLinkable(wuid);
        if (!object) {
            if (!std::ranges::contains(g_loggedMissing, wuid)) {
                Log::Write("mark: no linkable object for WUID {:016x} yet", wuid);
                g_loggedMissing.push_back(wuid);
            }
            continue;
        }
        std::erase(g_loggedMissing, wuid);
        auto mark = std::make_unique<Mark>(wuid);
        if (!mark->Add(map, object)) {
            Log::Write("mark: GetOrCreateEntityMark returned no mark for WUID {:016x}", wuid);
            continue;
        }
        Log::Write("mark: marked WUID {:016x}", wuid);
        g_marks.push_back(std::move(mark));
    }
}

void GameMark::Clear() {
    for (const auto &mark : g_marks)
        mark->Remove();
    g_marks.clear();
    g_loggedMissing.clear();
}
