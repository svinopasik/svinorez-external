#include "triggerbot.hpp"

#include "common/keymap.hpp"
#include "common/log.hpp"

#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

#include <algorithm>
#include <cctype>
#include <random>
#include <thread>

namespace cs2esp {


Triggerbot::Triggerbot(const Config& cfg, const Game& game)
    : cfg_(cfg), game_(game) {}

Triggerbot::~Triggerbot() {
    close();
}

void Triggerbot::close() {
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

void Triggerbot::update_hotkey() {
    cached_hotkey_ = cfg_.triggerbot.hotkey;
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
    } else if (hk == "mouse4") {
        always_active_ = false;
        is_mouse_hotkey_ = true;
        mouse_mask_ = Button4Mask;
        hotkey_code_ = 0;
    } else if (hk == "mouse5") {
        always_active_ = false;
        is_mouse_hotkey_ = true;
        mouse_mask_ = Button5Mask;
        hotkey_code_ = 0;
    } else {
        always_active_ = false;
        is_mouse_hotkey_ = false;
        mouse_mask_ = 0;
        hotkey_code_ = resolve_hardware_keycode(dpy_, hk);
    }
}

bool Triggerbot::init(std::string& error) {
    close();

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        dpy_ = XOpenDisplay(":1");
        if (!dpy_) dpy_ = XOpenDisplay(":0");
    }

    if (!dpy_) {
        error = "triggerbot: cannot open X display";
        return false;
    }

    update_hotkey();

    log_info("triggerbot initialized (hotkey: " + cfg_.triggerbot.hotkey +
             ", delays: " + std::to_string(cfg_.triggerbot.pre_delay_ms) + "ms pre / " +
             std::to_string(cfg_.triggerbot.post_delay_ms) + "ms post / " +
             std::to_string(cfg_.triggerbot.cooldown_ms) + "ms cd)");

    error.clear();
    return true;
}

bool Triggerbot::is_hotkey_down() const {
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

void Triggerbot::shoot(int target_idx, int target_health) {
    (void)target_idx;
    (void)target_health;
    if (!dpy_) return;

    if (cfg_.triggerbot.humanize) {
        static std::random_device rd;
        static std::mt19937 rng(rd());

        // Человеческая реакция: среднее из конфига с естественной дисперсией ~28 мс
        const float mean = static_cast<float>(std::max(150, cfg_.triggerbot.pre_delay_ms));
        std::normal_distribution<float> delay_dist(mean, 28.0f);
        const int delay = std::clamp(static_cast<int>(delay_dist(rng)), 135, 380);

        std::this_thread::sleep_for(std::chrono::milliseconds(delay));

        // Длительность удержания ЛКМ: 35..75 мс (не константная)
        std::uniform_int_distribution<int> hold_dist(35, 75);
        const int hold = hold_dist(rng);

        XTestFakeButtonEvent(dpy_, 1, True, CurrentTime);
        XFlush(dpy_);

        std::this_thread::sleep_for(std::chrono::milliseconds(hold));

        XTestFakeButtonEvent(dpy_, 1, False, CurrentTime);
        XFlush(dpy_);
    } else {
        if (cfg_.triggerbot.pre_delay_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(cfg_.triggerbot.pre_delay_ms));
        }

        XTestFakeButtonEvent(dpy_, 1, True, CurrentTime);
        XFlush(dpy_);

        std::this_thread::sleep_for(std::chrono::milliseconds(
            cfg_.triggerbot.post_delay_ms > 0 ? cfg_.triggerbot.post_delay_ms : 20));

        XTestFakeButtonEvent(dpy_, 1, False, CurrentTime);
        XFlush(dpy_);
    }

    last_shot_ = std::chrono::steady_clock::now();
}

void Triggerbot::tick(int local_team) {
    if (cfg_.triggerbot.hotkey != cached_hotkey_) {
        update_hotkey();
    }

    if (!is_hotkey_down()) return;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_shot_).count();
    if (elapsed < cfg_.triggerbot.cooldown_ms) {
        return;
    }

    int target_ent_idx = -1;
    int target_team = -1;
    int target_health = 0;

    if (game_.get_crosshair_target(target_ent_idx, target_team, target_health)) {
        if (target_ent_idx > 0 && target_health > 0) {
            if (cfg_.team_check && local_team > 0 && target_team == local_team) {
                return; // Игнорируем тиммейта
            }
            shoot(target_ent_idx, target_health);
        }
    }
}

} // namespace cs2esp
