#pragma once

#include "common/config.hpp"
#include "game.hpp"

#include <X11/Xlib.h>

#include <atomic>
#include <chrono>
#include <string>

namespace cs2esp {

class Aimbot {
public:
    Aimbot(const Config& cfg, const Game& game);
    ~Aimbot();

    bool init(std::string& error);
    void close();

    // Шаг доводки прицела
    void tick(const Snapshot& snap, int screen_w, int screen_h);

    bool is_hotkey_down() const;
    void update_hotkey();

private:
    const Config& cfg_;
    const Game& game_;

    Display* dpy_ = nullptr;
    std::string cached_hotkey_;
    int hotkey_code_ = 0;
    bool always_active_ = true;
    bool is_mouse_hotkey_ = false;
    int mouse_button_ = 0;
};

} // namespace cs2esp
