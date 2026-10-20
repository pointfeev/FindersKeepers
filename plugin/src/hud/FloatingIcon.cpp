#include "hud/FloatingIcon.h"

#include <algorithm>
#include <array>
#include <format>
#include <span>
#include <string>

#include "Log.h"
#include "game/Engine.h"
#include "game/Flash.h"
#include "game/Game.h"

using Engine::Vec3;
using Flash::Value;
using Flash::VarPtr;

namespace {
    // Metres from the target: hidden beyond kShowDistance, fading in until fully opaque at kFullDistance.
    constexpr float kShowDistance = 40.f;
    constexpr float kFullDistance = 25.f;
    // Metres above the target's origin (its base) where the icon is anchored.
    constexpr float kHeightAbove = 1.2f;
    // Icon size in pixels on a 1080-line screen; scaled with the screen height.
    constexpr float kSizeAt1080 = 48.f;

    // Per type, in MarkType::kTypes order: whether its icons' first showing has been logged, and how many clips it has
    // made (clips are kept and reused; the ones not needed in a frame are hidden).
    std::array<bool, MarkType::kTypes.size()> g_loggedShown{};
    std::array<std::size_t, MarkType::kTypes.size()> g_clipCount{};

    std::size_t TypeIndex(const MarkType::Type &type) {
        return static_cast<std::size_t>(MarkType::Find(type.id) - MarkType::kTypes.data());
    }

    std::string ClipPath(const MarkType::Type &type, const std::size_t index) {
        return std::format("_root.treasureFinderFloat_{}_{}", type.name, index);
    }

    // The type's index-th clip, directly under the hud movie's _root, whose coordinates are screen pixels; made on
    // first use (and again after the HUD is rebuilt). Its image child, the type's own map icon, is offset by half its
    // size so the clip's position is the icon's centre.
    bool GetClip(const Flash::Player *fp, const MarkType::Type &type, const std::size_t index, VarPtr &clip) {
        const std::string path = ClipPath(type, index);
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
        std::size_t &count = g_clipCount[TypeIndex(type)];
        count = std::max(count, index + 1);
        return true;
    }

    // Places a clip over a target and returns true, or returns false when the target is out of range or off screen.
    bool Place(const VarPtr &clip, const Vec3 &target, const Vec3 &playerPos, const float width, const float height,
               float &distance) {
        const Vec3 anchor(target.x, target.y, target.z + kHeightAbove);
        distance = (anchor - playerPos).Length();
        float sx = 0.f, sy = 0.f, sz = 0.f;
        // ProjectToScreen reports percent of the viewport; sz > 1 is behind the camera.
        if (distance > kShowDistance || !Engine::ProjectToScreen(anchor, sx, sy, sz) || sz > 1.f)
            return false;
        const float fade = std::clamp((kShowDistance - distance) / (kShowDistance - kFullDistance), 0.f, 1.f);
        const float size = kSizeAt1080 * height / 1080.f;
        clip->SetMember("_x", Value(sx * 0.01f * width));
        clip->SetMember("_y", Value(sy * 0.01f * height));
        clip->SetMember("_alpha", Value(fade * 100.f));
        clip->SetVisible(true);
        if (VarPtr img; clip->GetMember("img", img.put()) && img) {
            // The image takes its texture size when loadMovie completes, so its size is applied every frame.
            img->SetMember("_width", Value(size));
            img->SetMember("_height", Value(size));
            img->SetMember("_x", Value(-size / 2.f));
            img->SetMember("_y", Value(-size / 2.f));
        }
        return true;
    }
} // namespace

void FloatingIcon::Update(const MarkType::Type &type, std::span<const Vec3> targets, const bool tracked) {
    Vec3 playerPos;
    const auto *fp = Game::HudPlayer();
    if (!Engine::PlayerPosition(playerPos) || !fp)
        return;
    const float width = static_cast<float>(Engine::ScreenWidth());
    const float height = static_cast<float>(Engine::ScreenHeight());

    // Each target in range and on screen takes the next clip; a target that is not keeps its clip for the next one.
    std::size_t shown = 0;
    if (tracked && height > 0.f) {
        for (const Vec3 &target : targets) {
            // Most targets are far away: skip them before touching Flash.
            if ((Vec3(target.x, target.y, target.z + kHeightAbove) - playerPos).Length() > kShowDistance)
                continue;
            VarPtr clip;
            if (!GetClip(fp, type, shown, clip))
                break;
            if (float distance = 0.f; Place(clip, target, playerPos, width, height, distance)) {
                if (bool &logged = g_loggedShown[TypeIndex(type)]; !logged) {
                    Log::Write("float: '{}' shown over a target {:.1f} m away, on a {:.0f}x{:.0f} screen", type.name,
                               distance, width, height);
                    logged = true;
                }
                ++shown;
            } else {
                clip->SetVisible(false);
            }
        }
    }
    for (std::size_t i = shown; i < g_clipCount[TypeIndex(type)]; ++i)
        if (VarPtr clip; fp->IsAvailable(ClipPath(type, i).c_str()) &&
                         fp->GetVariable(ClipPath(type, i).c_str(), clip.put()) && clip)
            clip->SetVisible(false);
}
