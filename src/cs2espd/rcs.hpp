#pragma once

#include "common/config.hpp"
#include "game.hpp"

#include <X11/Xlib.h>

#include <string>

namespace cs2esp {

class Rcs {
public:
    explicit Rcs(const Config& cfg);
    ~Rcs();

    Rcs(const Rcs&) = delete;
    Rcs& operator=(const Rcs&) = delete;

    bool init(std::string& error);
    void close();

    // Вызывается каждый тик: компенсирует отдачу на основе текущего угла и выстрелов
    void tick(int shots_fired, const Vec2& punch);

private:
    const Config& cfg_;
    Display* dpy_ = nullptr;

    Vec2 old_punch_{0.0f, 0.0f};
    bool old_punch_valid_ = false;

    float accum_x_ = 0.0f;
    float accum_y_ = 0.0f;
};

} // namespace cs2esp
