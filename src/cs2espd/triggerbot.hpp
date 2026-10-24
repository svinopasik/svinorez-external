#pragma once

#include "common/config.hpp"
#include "game.hpp"

#include <X11/Xlib.h>

#include <chrono>
#include <string>

namespace cs2esp {

class Triggerbot {
public:
    Triggerbot(const Config& cfg, const Game& game);
    ~Triggerbot();

    Triggerbot(const Triggerbot&) = delete;
    Triggerbot& operator=(const Triggerbot&) = delete;

    bool init(std::string& error);
    void close();

    // Проверка условий и выстрел
    void tick(int local_team);

    // Проверка зажатия горячей клавиши (если задана)
    bool is_hotkey_down() const;
    void update_hotkey();

private:
    void shoot(int target_idx, int target_health);

    const Config& cfg_;
    const Game& game_;

    Display* dpy_ = nullptr;
    std::string cached_hotkey_;
    KeyCode hotkey_code_ = 0;
    bool always_active_ = true;
    bool is_mouse_hotkey_ = false;
    unsigned int mouse_mask_ = 0;

    std::chrono::steady_clock::time_point last_shot_{};
};

} // namespace cs2esp
