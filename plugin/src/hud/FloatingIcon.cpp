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

    // Per type, in MarkType::kTypes order: whether its icon's first showing has been logged.
    std::array<bool, MarkType::kTypes.size()> g_loggedShown{};

    std::string ClipName(const MarkType::Type &type) { return std::format("treasureFinderFloat_{}", type.name); }

    // A clip directly under the hud movie's _root, whose coordinates are screen pixels; its image child, the type's
    // own map icon, is offset by half its size so the clip's position is the icon's centre.
    bool EnsureClip(const Flash::Player *fp, const MarkType::Type &type) {
        const std::string name = ClipName(type);
        if (fp->IsAvailable(("_root." + name).c_str()))
            return true;
        VarPtr clip, img;
        if (VarPtr root; !fp->GetVariable("_root", root.put()) || !root ||
                         !root->CreateEmptyMovieClip(clip.put(), name.c_str()) || !clip ||
                         !clip->CreateEmptyMovieClip(img.put(), "img") || !img) {
            Log::Write("float: could not create the HUD clip for '{}'", type.name);
            return false;
        }
        const std::string icon = std::format("img://Libs/UI/Textures/Icons/Map/{}_icon.dds", type.name);
        const Value url(icon.c_str());
        if (!img->Invoke("loadMovie", std::span(&url, 1)))
            Log::Write("float: loadMovie({}) failed", icon);
        clip->SetVisible(false);
        return true;
    }
} // namespace

void FloatingIcon::Update(const MarkType::Type &type, std::span<const Vec3> targets, const bool tracked) {
    Vec3 playerPos;
    const auto *fp = Game::HudPlayer();
    if (!Engine::PlayerPosition(playerPos) || !fp || !EnsureClip(fp, type))
        return;
    VarPtr clip;
    if (!fp->GetVariable(("_root." + ClipName(type)).c_str(), clip.put()) || !clip)
        return;
    if (targets.empty()) {
        clip->SetVisible(false);
        return;
    }

    const auto anchorOf = [](const Vec3 &target) { return Vec3(target.x, target.y, target.z + kHeightAbove); };
    const Vec3 anchor =
        anchorOf(std::ranges::min(targets, {}, [&](const Vec3 &t) { return (anchorOf(t) - playerPos).Length(); }));
    const float distance = (anchor - playerPos).Length();

    float sx = 0.f, sy = 0.f, sz = 0.f;
    const bool onScreen = Engine::ProjectToScreen(anchor, sx, sy, sz);
    const float width = static_cast<float>(Engine::ScreenWidth());
    const float height = static_cast<float>(Engine::ScreenHeight());
    // ProjectToScreen reports percent of the viewport; sz > 1 is behind the camera.
    const float x = sx * 0.01f * width;
    const float y = sy * 0.01f * height;
    if (!tracked || distance > kShowDistance || !onScreen || sz > 1.f || height <= 0.f) {
        clip->SetVisible(false);
        return;
    }

    const float fade = std::clamp((kShowDistance - distance) / (kShowDistance - kFullDistance), 0.f, 1.f);
    const float size = kSizeAt1080 * height / 1080.f;
    clip->SetMember("_x", Value(x));
    clip->SetMember("_y", Value(y));
    clip->SetMember("_alpha", Value(fade * 100.f));
    clip->SetVisible(true);
    if (VarPtr img; clip->GetMember("img", img.put()) && img) {
        // The image takes its texture size when loadMovie completes, so its size is applied every frame.
        img->SetMember("_width", Value(size));
        img->SetMember("_height", Value(size));
        img->SetMember("_x", Value(-size / 2.f));
        img->SetMember("_y", Value(-size / 2.f));
    }
    if (bool &logged = g_loggedShown[MarkType::Find(type.id) - MarkType::kTypes.data()]; !logged) {
        Log::Write("float: '{}' shown at screen ({:.0f}, {:.0f}) of {:.0f}x{:.0f}, {:.1f} m away", type.name, x, y,
                   width, height, distance);
        logged = true;
    }
}
