#include "hud/FloatingIcon.h"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>
#include <vector>

#include "Log.h"
#include "game/Engine.h"
#include "game/Flash.h"
#include "game/Game.h"
#include "options/Options.h"

using Engine::Vec3;
using Flash::Value;
using Flash::VarPtr;

namespace {
    // Per type, in MarkType::kTypes order, its clips' paths, one per clip it has made, and how many it showed last
    // frame. Clips are kept and reused; the ones not needed in a frame are hidden. Only paths are kept: a clip
    // reference would outlive the HUD movie when the game rebuilds it.
    std::array<std::vector<std::string>, MarkType::kTypes.size()> g_clipPaths;
    std::array<std::size_t, MarkType::kTypes.size()> g_shownCount{};

    std::size_t TypeIndex(const MarkType::Type &type) {
        return static_cast<std::size_t>(MarkType::Find(type.id) - MarkType::kTypes.data());
    }

    const std::string &ClipPath(const MarkType::Type &type, const std::size_t index) {
        std::vector<std::string> &paths = g_clipPaths[TypeIndex(type)];
        while (paths.size() <= index)
            paths.push_back(std::format("_root.findersKeepersFloat_{}_{}", type.name, paths.size()));
        return paths[index];
    }

    // The type's index-th clip, directly under the hud movie's _root, whose coordinates are screen pixels; made on
    // first use (and again after the HUD is rebuilt). Its image child, the type's own map icon, is offset by half its
    // width and its full height so the clip's position is the icon's bottom centre: it then sits above the object at
    // any distance rather than reaching down into it as it grows on screen.
    bool GetClip(const Flash::Player *fp, const MarkType::Type &type, const std::size_t index, VarPtr &clip) {
        const std::string &path = ClipPath(type, index);
        if (fp->IsAvailable(path.c_str()))
            return fp->GetVariable(path.c_str(), clip.put()) && clip;
        VarPtr img;
        if (VarPtr root; !fp->GetVariable("_root", root.put()) || !root ||
                         !root->CreateEmptyMovieClip(clip.put(), path.substr(sizeof("_root.") - 1).c_str()) || !clip ||
                         !clip->CreateEmptyMovieClip(img.put(), "img") || !img) {
            Log::Write("float: could not create HUD clip {} for '{}'", index, type.name);
            return false;
        }
        const std::string icon = std::format("img://Libs/UI/Textures/Icons/Map/{}_icon.dds", type.name);
        const Value url(icon.c_str());
        if (!img->Invoke("loadMovie", std::span(&url, 1)))
            Log::Write("float: loadMovie({}) failed", icon);
        clip->SetVisible(false);
        return true;
    }

    // Icons are full size within this distance: next to the object, where you can see it is small anyway. Icon range
    // starts at 10 m, so the shrinking always has room.
    constexpr float kFullSizeWithin = 5.f; // metres

    // Where the icon is anchored: the options' height above the target's top.
    Vec3 Anchor(const Vec3 &target) { return {target.x, target.y, target.z + Options::Get().iconHeight}; }

    // Places a clip over a target and returns true, or returns false when the target is out of range or off screen.
    // Icons are hidden beyond the options' range and fade in until fully opaque at their fade distance. Their size is
    // full within kFullSizeWithin and from there shrinks at every distance, like perspective (1 / (1 + k * beyond)),
    // but gentler: k is chosen so an icon reaches the far scale at the edge of the range, not a speck. So an icon is
    // not massive over a small, far object and is still readable. The size is given for a 1080-line screen and scaled
    // with the screen height.
    bool Place(const VarPtr &clip, const Vec3 &target, const Vec3 &playerPos, const float width, const float height) {
        const Options::Values &options = Options::Get();
        const Vec3 anchor = Anchor(target);
        const float distance = (anchor - playerPos).Length();
        float sx = 0.f, sy = 0.f, sz = 0.f;
        // ProjectToScreen reports percent of the viewport; sz > 1 is behind the camera.
        if (distance > options.iconShowDistance || !Engine::ProjectToScreen(anchor, sx, sy, sz) || sz > 1.f)
            return false;
        const float fadeSpan = options.iconShowDistance - options.iconFullDistance;
        const float fade =
            fadeSpan > 0.f ? std::clamp((options.iconShowDistance - distance) / fadeSpan, 0.f, 1.f) : 1.f;
        const float falloff = (100.f / options.iconFarScale - 1.f) / (options.iconShowDistance - kFullSizeWithin);
        const float scale = 1.f / (1.f + falloff * std::max(distance - kFullSizeWithin, 0.f));
        const float size = options.iconSize * height / 1080.f * scale;
        clip->SetMember("_x", Value(sx * 0.01f * width));
        clip->SetMember("_y", Value(sy * 0.01f * height));
        clip->SetMember("_alpha", Value(fade * 100.f));
        clip->SetVisible(true);
        if (VarPtr img; clip->GetMember("img", img.put()) && img) {
            // The image takes its texture size when loadMovie completes, so its size is applied every frame.
            img->SetMember("_width", Value(size));
            img->SetMember("_height", Value(size));
            img->SetMember("_x", Value(-size / 2.f));
            img->SetMember("_y", Value(-size));
        }
        return true;
    }
} // namespace

void FloatingIcon::Update(const MarkType::Type &type, const std::span<const std::uint32_t> targets,
                          const bool tracked) {
    Vec3 playerPos;
    const auto *fp = Game::HudPlayer();
    if (!Engine::PlayerPosition(playerPos) || !fp)
        return;
    const float width = static_cast<float>(Engine::ScreenWidth());
    const float height = static_cast<float>(Engine::ScreenHeight());

    // Each target in range and on screen takes the next clip; a target that is not keeps its clip for the next one.
    std::size_t shown = 0;
    if (tracked && height > 0.f) {
        for (const std::uint32_t entity : targets) {
            const Engine::Entity *found = Engine::GetEntity(entity);
            if (!found)
                continue;
            const Vec3 target = Engine::EntityTop(found);
            // Most targets are far away: skip them before touching Flash.
            if ((Anchor(target) - playerPos).Length() > Options::Get().iconShowDistance)
                continue;
            VarPtr clip;
            if (!GetClip(fp, type, shown, clip))
                break;
            if (Place(clip, target, playerPos, width, height))
                ++shown;
            else
                clip->SetVisible(false);
        }
    }
    // Clips beyond those shown now are hidden already, unless they were shown last frame.
    std::size_t &lastShown = g_shownCount[TypeIndex(type)];
    for (std::size_t i = shown; i < lastShown; ++i)
        if (VarPtr clip; fp->IsAvailable(ClipPath(type, i).c_str()) &&
                         fp->GetVariable(ClipPath(type, i).c_str(), clip.put()) && clip)
            clip->SetVisible(false);
    lastShown = shown;
}
