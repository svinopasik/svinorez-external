#include "aimbot.hpp"

#include "common/keymap.hpp"
#include "common/log.hpp"

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <random>

namespace cs2esp {
namespace {

bool world_to_screen(const Vec3& v, const Matrix4& m, float w, float h, Vec2& out) {
    const float cx = v.x * m.m[0][0] + v.y * m.m[0][1] + v.z * m.m[0][2] + m.m[0][3];
    const float cy = v.x * m.m[1][0] + v.y * m.m[1][1] + v.z * m.m[1][2] + m.m[1][3];
    const float cw = v.x * m.m[3][0] + v.y * m.m[3][1] + v.z * m.m[3][2] + m.m[3][3];
    if (cw <= 0.001f) return false;
    out.x = (cx / cw * 0.5f + 0.5f) * w;
    out.y = (0.5f - cy / cw * 0.5f) * h;
    return true;
}

} // namespace

Aimbot::Aimbot(const Config& cfg, const Game& game)
    : cfg_(cfg), game_(game) {}

Aimbot::~Aimbot() {
    close();
}

void Aimbot::close() {
    if (dpy_) {
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
    }
    hotkey_code_ = 0;
    always_active_ = true;
    is_mouse_hotkey_ = false;
    mouse_mask_ = 0;
    cached_hotkey_.clear();
}

void Aimbot::update_hotkey() {
    cached_hotkey_ = cfg_.aimbot.hotkey;
    std::string hk = cached_hotkey_;
    for (char& c : hk) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (hk.empty() || hk == "always" || hk == "auto" || hk == "none") {
        always_active_ = true;
        hotkey_code_ = 0;
        is_mouse_hotkey_ = false;
        mouse_mask_ = 0;
    } else if (hk == "mouse1" || hk == "lmb") {
        always_active_ = false;
        is_mouse_hotkey_ = true;
        mouse_mask_ = Button1Mask;
        hotkey_code_ = 0;
    } else if (hk == "mouse2" || hk == "rmb") {
        always_active_ = false;
        is_mouse_hotkey_ = true;
        mouse_mask_ = Button3Mask;
        hotkey_code_ = 0;
    } else if (hk == "mouse3" || hk == "mmb") {
        always_active_ = false;
        is_mouse_hotkey_ = true;
        mouse_mask_ = Button2Mask;
        hotkey_code_ = 0;
    } else {
        always_active_ = false;
        is_mouse_hotkey_ = false;
        mouse_mask_ = 0;
        hotkey_code_ = resolve_hardware_keycode(dpy_, hk);
    }
}

bool Aimbot::init(std::string& error) {
    close();

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        dpy_ = XOpenDisplay(":1");
        if (!dpy_) dpy_ = XOpenDisplay(":0");
    }

    if (!dpy_) {
        error = "aimbot: cannot open X display";
        return false;
    }

    update_hotkey();

    log_info("aimbot initialized (hotkey: " + cfg_.aimbot.hotkey +
             ", bone: " + cfg_.aimbot.bone +
             ", fov: " + std::to_string(static_cast<int>(cfg_.aimbot.fov)) +
             "px, smooth: " + std::to_string(static_cast<int>(cfg_.aimbot.smooth)) +
             ", visible_check: " + (cfg_.aimbot.visible_check ? "ON" : "OFF") + ")");

    error.clear();
    return true;
}

bool Aimbot::is_hotkey_down() const {
    if (always_active_) return true;
    if (!dpy_) return false;

    if (is_mouse_hotkey_) {
        Window root_ret = 0, child_ret = 0;
        int rx = 0, ry = 0, wx = 0, wy = 0;
        unsigned int mask = 0;
        if (XQueryPointer(dpy_, DefaultRootWindow(dpy_), &root_ret, &child_ret,
                          &rx, &ry, &wx, &wy, &mask)) {
            return (mask & mouse_mask_) != 0;
        }
        return false;
    }

    if (hotkey_code_ == 0) return false;

    char keys[32];
    XQueryKeymap(dpy_, keys);
    return (keys[hotkey_code_ / 8] & (1 << (hotkey_code_ % 8))) != 0;
}

void Aimbot::tick(const Snapshot& snap, int screen_w, int screen_h) {
    if (!dpy_ || !snap.matrix_ok || screen_w <= 0 || screen_h <= 0) return;

    if (cfg_.aimbot.hotkey != cached_hotkey_) {
        update_hotkey();
    }

    if (!is_hotkey_down()) return;

    const float center_x = screen_w * 0.5f;
    const float center_y = screen_h * 0.5f;

    const int target_bone = (cfg_.aimbot.bone == "chest") ? 5 : 7; // 7 = head, 5 = chest

    float best_dist = cfg_.aimbot.fov;
    float best_dx = 0.0f;
    float best_dy = 0.0f;
    bool found = false;

    for (const PlayerSnap& p : snap.players) {
        if (!p.alive) continue;
        if (cfg_.aimbot.team_check && snap.have_local && p.team == snap.local_team) continue;

        // Visible Check: если включена проверка видимости — пропускаем цели за стенами
        if (cfg_.aimbot.visible_check && !p.is_visible) continue;

        Vec3 target_3d;
        if (p.has_bones && p.bones.size() > static_cast<size_t>(target_bone)) {
            target_3d = p.bones[target_bone];
        } else {
            target_3d = {p.origin.x, p.origin.y, p.origin.z + cfg_.esp.box_height_units * 0.9f};
        }

        Vec2 screen_target;
        if (!world_to_screen(target_3d, snap.matrix, static_cast<float>(screen_w),
                             static_cast<float>(screen_h), screen_target)) {
            continue;
        }

        const float dx = screen_target.x - center_x;
        const float dy = screen_target.y - center_y;
        const float dist = std::hypot(dx, dy);

        if (dist <= best_dist) {
            best_dist = dist;
            best_dx = dx;
            best_dy = dy;
            found = true;
        }
    }

    if (found) {
        const float smooth = std::max(1.0f, cfg_.aimbot.smooth);
        float move_x = best_dx / smooth;
        float move_y = best_dy / smooth;

        if (cfg_.aimbot.humanize) {
            static std::random_device rd;
            static std::mt19937 rng(rd());
            std::uniform_real_distribution<float> jitter(-0.35f, 0.35f);

            // Добавляем микро-джиттер, чтобы вектор движения не был прямой линейной интерполяцией
            move_x += jitter(rng);
            move_y += jitter(rng);

            constexpr float kMaxLegitStep = 10.0f;
            move_x = std::clamp(move_x, -kMaxLegitStep, kMaxLegitStep);
            move_y = std::clamp(move_y, -kMaxLegitStep, kMaxLegitStep);
        } else {
            constexpr float kMaxStep = 35.0f;
            move_x = std::clamp(move_x, -kMaxStep, kMaxStep);
            move_y = std::clamp(move_y, -kMaxStep, kMaxStep);
        }

        if (std::abs(best_dx) >= 1.0f && std::abs(move_x) < 1.0f) {
            move_x = (best_dx > 0) ? 1.0f : -1.0f;
        }
        if (std::abs(best_dy) >= 1.0f && std::abs(move_y) < 1.0f) {
            move_y = (best_dy > 0) ? 1.0f : -1.0f;
        }

        XTestFakeRelativeMotionEvent(dpy_, static_cast<int>(std::lround(move_x)),
                                     static_cast<int>(std::lround(move_y)), CurrentTime);
        XFlush(dpy_);
    }
}

} // namespace cs2esp
