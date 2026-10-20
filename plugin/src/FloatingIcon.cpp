#include "FloatingIcon.h"

#include <algorithm>
#include <span>

#include "Engine.h"
#include "Flash.h"
#include "Game.h"
#include "Log.h"

using Engine::Vec3;
using Flash::Value;
using Flash::VarPtr;

namespace {
    constexpr char kClip[] = "_root.treasureFinderFloat";
    constexpr char kIconUrl[] = "img://Libs/UI/Textures/Icons/Items/map_book_icon.dds";
    // Metres from the treasure: hidden beyond kShowDistance, fading in until fully opaque at kFullDistance.
    constexpr float kShowDistance = 40.f;
    constexpr float kFullDistance = 25.f;
    // Metres above the treasure's origin (its base) where the icon is anchored.
    constexpr float kHeightAbove = 1.2f;
    // Icon size in pixels on a 1080-line screen; scaled with the screen height.
    constexpr float kSizeAt1080 = 48.f;

    bool g_loggedShown = false;

    // A clip directly under the hud movie's _root, whose coordinates are screen pixels; its image child is offset
    // by half its size so the clip's position is the icon's centre.
    bool EnsureClip(const Flash::Player *fp) {
        if (fp->IsAvailable(kClip))
            return true;
        VarPtr clip, img;
        if (VarPtr root; !fp->GetVariable("_root", root.put()) || !root ||
                         !root->CreateEmptyMovieClip(clip.put(), "treasureFinderFloat") || !clip ||
                         !clip->CreateEmptyMovieClip(img.put(), "img") || !img) {
            Log::Write("float: could not create the HUD clip");
            return false;
        }
        const Value url(kIconUrl);
        if (!img->Invoke("loadMovie", std::span(&url, 1)))
            Log::Write("float: loadMovie({}) failed", kIconUrl);
        clip->SetVisible(false);
        return true;
    }
} // namespace

void FloatingIcon::Update(std::span<const Vec3> treasures, const bool tracked) {
    Vec3 playerPos;
    const auto *fp = Game::HudPlayer();
    if (!Engine::PlayerPosition(playerPos) || !fp || !EnsureClip(fp))
        return;
    VarPtr clip;
    if (!fp->GetVariable(kClip, clip.put()) || !clip)
        return;
    if (treasures.empty()) {
        clip->SetVisible(false);
        return;
    }

    const auto anchorOf = [](const Vec3 &treasure) { return Vec3(treasure.x, treasure.y, treasure.z + kHeightAbove); };
    const Vec3 anchor =
        anchorOf(std::ranges::min(treasures, {}, [&](const Vec3 &t) { return (anchorOf(t) - playerPos).Length(); }));
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
    if (!g_loggedShown) {
        Log::Write("float: shown at screen ({:.0f}, {:.0f}) of {:.0f}x{:.0f}, {:.1f} m away", x, y, width, height,
                   distance);
        g_loggedShown = true;
    }
}
