#include "GameMark.h"

#include <algorithm>
#include <memory>
#include <string>
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
        Mark(const std::uint64_t wuid, std::string label) : wuid_(wuid), label_(std::move(label)) {}

        std::uint64_t Wuid() const { return wuid_; }
        const std::string &Label() const { return label_; }
        bool Live() const { return mark_ != nullptr; }
        bool Is(const std::int32_t id) const { return mark_ && mark_->id == id; }

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
        void OnObjectEvent(const int event) override {
            if (event != Game::kObjectRemoved)
                return;
            Log::Write("mark: object {:016x} destroyed; mark removed", wuid_);
            Remove();
        }

      private:
        std::uint64_t wuid_;
        std::string label_;
        std::shared_ptr<Game::EntityMark> mark_;
    };

    // The game holds pointers to these (as listeners and to their WUIDs), so they do not move.
    std::vector<std::unique_ptr<Mark>> g_marks;
    // WUIDs already logged as having no object, so a target out of reach is logged once.
    std::vector<std::uint64_t> g_loggedMissing;
} // namespace

void GameMark::Sync(std::span<const ProbeResult::Target> targets) {
    std::erase_if(g_marks, [&](const std::unique_ptr<Mark> &mark) {
        if (std::ranges::contains(targets, mark->Wuid(), &ProbeResult::Target::wuid) && mark->Live())
            return false;
        mark->Remove();
        return true;
    });
    auto *map = Game::GetMap();
    if (!map)
        return;
    for (const auto &target : targets) {
        const std::uint64_t wuid = target.wuid;
        // Two maps can lead to one object (Krizhan's and the Bandit's): the first one names the mark.
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
        auto mark = std::make_unique<Mark>(wuid, target.label);
        if (!mark->Add(map, object)) {
            Log::Write("mark: GetOrCreateEntityMark returned no mark for WUID {:016x}", wuid);
            continue;
        }
        Log::Write("mark: marked WUID {:016x}", wuid);
        g_marks.push_back(std::move(mark));
    }
}

const char *GameMark::LabelFor(const std::int32_t markId) {
    const auto mark = std::ranges::find_if(g_marks, [&](const std::unique_ptr<Mark> &m) { return m->Is(markId); });
    return mark == g_marks.end() ? nullptr : (*mark)->Label().c_str();
}

void GameMark::Clear() {
    for (const auto &mark : g_marks)
        mark->Remove();
    g_marks.clear();
    g_loggedMissing.clear();
}
