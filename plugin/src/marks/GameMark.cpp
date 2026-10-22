#include "marks/GameMark.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Log.h"
#include "game/Game.h"
#include "marks/MarkType.h"

namespace {
    // The param the game's own "show map marker" quest node passes.
    constexpr std::uint32_t kMarkParam = 2;

    // Stands in for the linkable object an entity mark points at, for an entity the game made no AI object for (an
    // item slot nothing links to has no WUID). It is plain memory the game only gets a pointer to, not an entity:
    // nothing in the world changes. For a mark that is not an area, the map and compass read only two things from the
    // object (Ghidra, 1.5.6): its position from vtable slot 2 (the map marker 0x1815B9D70 and the compass marker
    // 0x181F443D0 copy it once), and the WUID at +8, which the compass marker watches and unwatches as a key
    // (0x180C4D228, 0x180A195A4) and whose top byte 0x0F or 0x07 would make it an area.
    struct StandIn {
        const void *const *vtable;
        std::uint64_t wuid;
        Engine::Vec3 pos;
    };
    static_assert(offsetof(StandIn, wuid) == 8);

    // The stand-ins' WUID type, away from the types seen in the game (3 inventories, 5 souls, 0x0A stashes and other AI
    // objects, 0x07, 0x0F, 0x14, 0x15 areas); the low bits are the EntityId.
    constexpr std::uint64_t kStandInWuid = 0xFEull << 56u;
    constexpr std::size_t kStandInPositionSlot = 2;
    // Well past the highest linkable vfunc the traced code calls (+0xF8, slot 31, on area marks).
    constexpr std::size_t kStandInSlots = 128;

    const Engine::Vec3 *StandInPosition(const StandIn *self) { return &self->pos; }

    // Every other slot: the game should call none, so a call is logged (once per slot) to show what the mark needed.
    template <std::size_t Slot> std::uintptr_t StandInUnexpected(const StandIn *self) {
        static bool logged = false;
        if (!logged) {
            Log::Write("mark: the game called slot {} of the stand-in for entity {}", Slot, self->wuid & ~kStandInWuid);
            logged = true;
        }
        return 0;
    }

    template <std::size_t... Slots>
    std::array<const void *, kStandInSlots> StandInVtable(std::index_sequence<Slots...>) noexcept {
        return {(Slots == kStandInPositionSlot ? reinterpret_cast<const void *>(&StandInPosition)
                                               : reinterpret_cast<const void *>(&StandInUnexpected<Slots>))...};
    }
    const std::array<const void *, kStandInSlots> kStandInVtable =
        StandInVtable(std::make_index_sequence<kStandInSlots>());

    // One mark. A mark on a game object watches it the way the quest node does, so the mark goes before the object it
    // points at; a target without a WUID is marked through its own stand-in, which lives as long as the mark.
    class Mark final : public Game::ObjectListener {
      public:
        explicit Mark(const ProbeResult::Target &target)
            : entity_(target.entity), wuid_(target.wuid), source_(target.source), label_(target.label) {
            if (!wuid_)
                standIn_ = std::make_unique<StandIn>(
                    StandIn{.vtable = kStandInVtable.data(), .wuid = kStandInWuid | entity_, .pos = target.pos});
        }

        // The same object and the same kind of mark.
        bool For(const ProbeResult::Target &target) const {
            return entity_ == target.entity && wuid_ == target.wuid && source_ == target.source;
        }
        const std::string &Label() const { return label_; }
        bool Live() const { return mark_ != nullptr; }
        bool Is(const std::int32_t id) const { return mark_ && mark_->id == id; }
        std::string Name() const {
            return standIn_ ? std::format("entity {} (stand-in)", entity_) : std::format("WUID {:016x}", wuid_);
        }

        // What the mark points at: the object's linkable, null while it does not exist, or the stand-in.
        void *Object() const { return standIn_ ? standIn_.get() : Game::FindLinkable(wuid_); }

        bool Add(Game::Map *map, void *object) {
            const MarkType::Type &type = source_ ? MarkType::kMapLocation : MarkType::kMapDestination;
            mark_ = Game::AddEntityMark(map, type.id, object, kMarkParam);
            if (!mark_)
                return false;
            if (!standIn_)
                Game::WatchObject(&wuid_, this);
            return true;
        }

        // The compass marker reads the stand-in while it is removed, so the stand-in goes only with the Mark.
        void Remove() {
            if (!mark_)
                return;
            if (!standIn_)
                Game::UnwatchObject(&wuid_, this);
            if (auto *map = Game::GetMap())
                Game::RemoveEntityMark(map, mark_);
            else
                Log::Write("mark: no map screen to remove the mark for {} from", Name());
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
        std::uint32_t entity_;
        std::uint64_t wuid_;
        bool source_;
        std::string label_;
        std::unique_ptr<StandIn> standIn_;
        std::shared_ptr<Game::EntityMark> mark_;
    };

    // The game holds pointers to these (as listeners and to their WUIDs), so they do not move.
    std::vector<std::unique_ptr<Mark>> g_marks;
    // WUIDs already logged as having no object, so a target out of reach is logged once.
    std::vector<std::uint64_t> g_loggedMissing;
} // namespace

void GameMark::Sync(std::span<const ProbeResult::Target> targets) {
    std::erase_if(g_marks, [&](const std::unique_ptr<Mark> &mark) {
        if (mark->Live() && std::ranges::any_of(targets, [&](const auto &target) { return mark->For(target); }))
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
        if (std::ranges::any_of(g_marks, [&](const auto &mark) { return mark->For(target); }))
            continue;
        auto mark = std::make_unique<Mark>(target);
        void *object = mark->Object();
        if (!object) {
            if (!std::ranges::contains(g_loggedMissing, wuid)) {
                Log::Write("mark: no linkable object for WUID {:016x} yet", wuid);
                g_loggedMissing.push_back(wuid);
            }
            continue;
        }
        std::erase(g_loggedMissing, wuid);
        if (!mark->Add(map, object)) {
            Log::Write("mark: GetOrCreateEntityMark returned no mark for {}", mark->Name());
            continue;
        }
        Log::Write("mark: marked {}{}", mark->Name(), target.source ? " (map location)" : "");
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
