#pragma once

#include "game.hpp"

#include <string>
#include <vector>

namespace cs2esp {

struct ScreenLine {
    float x1 = 0, y1 = 0;
    float x2 = 0, y2 = 0;
};

// Бокс в экранных координатах; готов к отрисовке оверлеем.
struct ScreenBox {
    float x = 0, y = 0;    // верхний левый угол
    float w = 0, h = 0;
    unsigned color = 0xFF00FF00u; // 0xAARRGGBB
    int health = 0;
    float dist_units = 0.f;
    std::string name;      // пусто, если имена отключены или нечитаемы
    std::vector<ScreenLine> skeleton_lines;
};

class Esp {
public:
    explicit Esp(const Config& cfg);

    // Проецирует снапшот в экранные боксы на канвасе w×h px.
    void to_boxes(const Snapshot& snap, int screen_w, int screen_h,
                  std::vector<ScreenBox>& out);

private:
    const Config& cfg_;
    unsigned color_enemy_    = 0xFF4040FFu;
    unsigned color_teammate_ = 0xFF40A0FFu;
};

} // namespace cs2esp
