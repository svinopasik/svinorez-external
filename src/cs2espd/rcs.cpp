#include "rcs.hpp"

#include "common/log.hpp"

#include <X11/extensions/XTest.h>

#include <algorithm>
#include <cmath>

namespace cs2esp {

Rcs::Rcs(const Config& cfg)
    : cfg_(cfg) {}

Rcs::~Rcs() {
    close();
}

void Rcs::close() {
    if (dpy_) {
        XCloseDisplay(dpy_);
        dpy_ = nullptr;
    }
    old_punch_ = {0.0f, 0.0f};
    old_punch_valid_ = false;
    accum_x_ = 0.0f;
    accum_y_ = 0.0f;
}

bool Rcs::init(std::string& error) {
    close();

    dpy_ = XOpenDisplay(nullptr);
    if (!dpy_) {
        dpy_ = XOpenDisplay(":1");
        if (!dpy_) dpy_ = XOpenDisplay(":0");
    }

    if (!dpy_) {
        error = "rcs: cannot open X display";
        return false;
    }

    log_info("rcs initialized (scale_x: " + std::to_string(cfg_.rcs.scale_x) +
             ", scale_y: " + std::to_string(cfg_.rcs.scale_y) +
             ", sens: " + std::to_string(cfg_.rcs.sensitivity) + ")");

    error.clear();
    return true;
}

void Rcs::tick(int shots_fired, const Vec2& punch) {
    if (!dpy_) return;
    if (!cfg_.rcs.enabled) {
        old_punch_valid_ = false;
        old_punch_ = {0.0f, 0.0f};
        accum_x_ = 0.0f;
        accum_y_ = 0.0f;
        return;
    }

    // Компенсируем только при стрельбе очередью (начиная со 2-го выстрела)
    if (shots_fired >= 2) {
        if (old_punch_valid_) {
            const float delta_pitch = punch.x - old_punch_.x;
            const float delta_yaw   = punch.y - old_punch_.y;

            // В движке Source:
            // 1 count (пиксель) = sensitivity * 0.022 градусов
            // Смещение прицела от отдачи составляет 2.0 * aimPunchAngle
            const float sens = std::max(0.01f, cfg_.rcs.sensitivity);
            const float factor = 2.0f / (sens * 0.022f);

            // delta_pitch < 0 при подбросе вверх -> тянем мышь вниз (+Y)
            // delta_yaw > 0 при уводе вправо -> тянем мышь влево (-X)
            const float move_x = -delta_yaw   * factor * cfg_.rcs.scale_x;
            const float move_y = -delta_pitch * factor * cfg_.rcs.scale_y;

            accum_x_ += move_x;
            accum_y_ += move_y;

            constexpr float kMaxStep = 60.0f;
            accum_x_ = std::clamp(accum_x_, -kMaxStep, kMaxStep);
            accum_y_ = std::clamp(accum_y_, -kMaxStep, kMaxStep);

            int ix = static_cast<int>(accum_x_);
            int iy = static_cast<int>(accum_y_);

            if (ix != 0 || iy != 0) {
                accum_x_ -= static_cast<float>(ix);
                accum_y_ -= static_cast<float>(iy);

                XTestFakeRelativeMotionEvent(dpy_, ix, iy, CurrentTime);
                XFlush(dpy_);
            }
        }
        old_punch_ = punch;
        old_punch_valid_ = true;
    } else {
        // При отпускании стрельбы сбрасываем предыдущий угол,
        // чтобы игра сама вернула прицел в исходную позицию без рывка вверх
        old_punch_ = {0.0f, 0.0f};
        old_punch_valid_ = false;
        accum_x_ = 0.0f;
        accum_y_ = 0.0f;
    }
}

} // namespace cs2esp
